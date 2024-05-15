#include <stdio.h>
#include <io.h>
#include <fcntl.h>
#include "../utils.h"

HANDLE hMapFile, hMutex, hEvent;
SharedData* pSharedData;

void ReadUsersFromFile(ServerState* state, const TCHAR* filename) {
    FILE* file;
    _tfopen_s(&file, filename, TEXT("r"));
    if (!file) {
        _tprintf(TEXT("Erro ao abrir o ficheiro de utilizadores.\n"));
        return;
    }

    TCHAR username[50], password[50];
    double saldo;
    while (_ftscanf_s(file, TEXT("%49s %49s %lf"), username, (unsigned)_countof(username), password, (unsigned)_countof(password), &saldo) == 3) {
        if (state->numUtilizadores < MAX_UTILIZADORES) {
            wcscpy_s(state->utilizadores[state->numUtilizadores].username, _countof(state->utilizadores[state->numUtilizadores].username), username);
            state->utilizadores[state->numUtilizadores].saldo = saldo;
            state->utilizadores[state->numUtilizadores].isOnline = FALSE;
            state->numUtilizadores++;
        }
        else {
            _tprintf(TEXT("Número máximo de utilizadores atingido.\n"));
            break;
        }
    }

    fclose(file);
}

void CleanUpServer(ServerState* stateServ) {
    // Fechar todos os handles dos clientes
    for (int i = 0; i < MAXCLIENTES; ++i) {
        if (stateServ->clientPipes[i] != NULL) {
            DisconnectNamedPipe(stateServ->clientPipes[i]);
            CloseHandle(stateServ->clientPipes[i]);
            stateServ->clientPipes[i] = NULL;
        }
    }

    // Fechar eventos
    if (stateServ->writeReady) {
        CloseHandle(stateServ->writeReady);
    }
    if (stateServ->readEvent) {
        CloseHandle(stateServ->readEvent);
    }

    // Fechar handles de memória partilhada
    if (pSharedData) {
        UnmapViewOfFile(pSharedData);
    }
    if (hMapFile) {
        CloseHandle(hMapFile);
    }
    if (hMutex) {
        CloseHandle(hMutex);
    }
    if (hEvent) {
        CloseHandle(hEvent);
    }
}

void CloseSystem(ServerState* state, TCHAR* response) {
    _tcscpy_s(response, MSG_TAM, TEXT("Sistema encerrado.\n"));
    for (int i = 0; i < MAXCLIENTES; ++i) {
        if (state->clientPipes[i] != NULL) {
            DisconnectNamedPipe(state->clientPipes[i]);
            CloseHandle(state->clientPipes[i]);
        }
    }

    state->running = FALSE;

}

void InitializeServerState(ServerState* stateServ) {
    for (int i = 0; i < MAXCLIENTES; ++i) {
        stateServ->clientPipes[i] = NULL;
    }
    stateServ->writeReady = CreateEvent(NULL, TRUE, FALSE, NULL);
    stateServ->readEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    stateServ->numEmpresas = 0;
    stateServ->numUtilizadores = 0;
    stateServ->tradingPaused = FALSE;
    stateServ->running = TRUE;

}

void PrintMenu() {
    _tprintf(TEXT("\n------------------------------- Comandos da Bolsa -------------------------------\n\n"));
    _tprintf(TEXT("- Adicionar empresa         -   addc <nome-empresa> <número-ações> <preço-ação>\n"));
    _tprintf(TEXT("- Listar todas as empresas  -   listc\n"));
    _tprintf(TEXT("- Alterar preço das ações   -   stock <nome-empresa> <novo-preço>\n"));
    _tprintf(TEXT("- Listar utilizadores       -   users\n"));
    _tprintf(TEXT("- Pausar operações          -   pause <segundos>\n"));
    _tprintf(TEXT("- Encerrar sistema          -   close\n"));
    _tprintf(TEXT("\n---------------------------------------------------------------------------------\n"));
}

void PrintLastError(const TCHAR* msg) {
    DWORD eNum;
    TCHAR sysMsg[256];
    TCHAR* p;

    eNum = GetLastError();
    FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL, eNum,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        sysMsg, 256, NULL);

    p = sysMsg;
    while ((*p > 31) || (*p == 9))
        ++p;
    do { *p-- = 0; } while ((p >= sysMsg) && ((*p == '.') || (*p < 33)));

    _tprintf(TEXT("\n%s: %s (Error %d)\n"), msg, sysMsg, eNum);
}

void adicionaCliente(ServerState* stateServ, HANDLE hCli) {
    for (int i = 0; i < MAXCLIENTES; ++i) {
        if (stateServ->clientPipes[i] == NULL) {
            stateServ->clientPipes[i] = hCli;
            break;
        }
    }
}

void removeCliente(ServerState* stateServ, HANDLE hCli) {
    for (int i = 0; i < MAXCLIENTES; ++i) {
        if (stateServ->clientPipes[i] == hCli) {
            CloseHandle(stateServ->clientPipes[i]);
            stateServ->clientPipes[i] = NULL;
            if (stateServ->readEvent != NULL) {
                CloseHandle(stateServ->readEvent);
                stateServ->readEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
            }
            _tprintf(TEXT("\nClient %d disconnected."), i);
            break;
        }
    }
}

void UpdateSharedData(ServerState* state) {
    WaitForSingleObject(hMutex, INFINITE);
    pSharedData->numEmpresas = state->numEmpresas;
    for (int i = 0; i < state->numEmpresas; ++i) {
        pSharedData->empresas[i] = state->empresas[i];
    }
    SetEvent(hEvent);
    ReleaseMutex(hMutex);
}

void AddCompany(ServerState* state, const TCHAR* nomeEmpresa, int numAcoes, double precoAcao, TCHAR* response) {
    if (state->numEmpresas < MAX_EMPRESAS) {
        _tcsncpy_s(state->empresas[state->numEmpresas].nomeEmpresa, _countof(state->empresas[state->numEmpresas].nomeEmpresa), nomeEmpresa, _TRUNCATE);
        state->empresas[state->numEmpresas].numAcoes = numAcoes;
        state->empresas[state->numEmpresas].precoAcao = precoAcao;
        state->numEmpresas++;
        _stprintf_s(response, MSG_TAM, TEXT("Empresa %s adicionada com %d ações a %.2f cada.\n"), nomeEmpresa, numAcoes, precoAcao);
        UpdateSharedData(state);
    }
    else {
        _stprintf_s(response, MSG_TAM, TEXT("Limite de empresas atingido.\n"));
    }
}

void ListCompanies(const ServerState* state, TCHAR* response) {
    TCHAR buffer[MSG_TAM];
    _stprintf_s(response, MSG_TAM, TEXT("Empresas na Bolsa:\n"));
    for (int i = 0; i < state->numEmpresas; ++i) {
        _stprintf_s(buffer, MSG_TAM, TEXT("Empresa: %s, Ações: %d, Preço: %.2f\n"), state->empresas[i].nomeEmpresa, state->empresas[i].numAcoes, state->empresas[i].precoAcao);
        _tcscat_s(response, MSG_TAM, buffer);
    }
}

void SetStockPrice(ServerState* state, const TCHAR* nomeEmpresa, double newPrice, TCHAR* response) {
    for (int i = 0; i < state->numEmpresas; ++i) {
        if (_tcscmp(state->empresas[i].nomeEmpresa, nomeEmpresa) == 0) {
            state->empresas[i].precoAcao = newPrice;
            _stprintf_s(response, MSG_TAM, TEXT("Preço da empresa %s alterado para %.2f.\n"), nomeEmpresa, newPrice);
            UpdateSharedData(state);
            return;
        }
    }
    _stprintf_s(response, MSG_TAM, TEXT("Empresa %s não encontrada.\n"), nomeEmpresa);
}

void ListUsers(const ServerState* state, TCHAR* response) {
    TCHAR buffer[MSG_TAM];
    _stprintf_s(response, MSG_TAM, TEXT("Utilizadores na Bolsa:\n"));
    for (int i = 0; i < state->numUtilizadores; ++i) {
        _stprintf_s(buffer, MSG_TAM, TEXT("Utilizador: %s, Saldo: %.2f, Online: %s\n"),
            state->utilizadores[i].username, state->utilizadores[i].saldo,
            state->utilizadores[i].isOnline ? TEXT("Sim") : TEXT("Não"));
        _tcscat_s(response, MSG_TAM, buffer);
    }
}

void PauseTrading(ServerState* state, int duration, TCHAR* response) {
    state->tradingPaused = TRUE;
    _stprintf_s(response, MSG_TAM, TEXT("Operações de compra e venda pausadas por %d segundos.\n"), duration);
    Sleep(duration * 1000);
    state->tradingPaused = FALSE;
    _tcscpy_s(response, MSG_TAM, TEXT("Operações de compra e venda retomadas.\n"));
}

void ProcessAdminCommand(ServerState* stateServ, TCHAR* command, BOOL* closeFlag) {
    if (_tcscmp(command, TEXT("comandos")) == 0) {
        PrintMenu();
    }
    else if (_tcsncmp(command, TEXT("addc"), 4) == 0) {
        TCHAR nomeEmpresa[50];
        int numAcoes;
        double precoAcao;
        if (_stscanf_s(command, TEXT("addc %49s %d %lf"), nomeEmpresa, (unsigned)_countof(nomeEmpresa), &numAcoes, &precoAcao) == 3) {
            TCHAR response[MSG_TAM];
            AddCompany(stateServ, nomeEmpresa, numAcoes, precoAcao, response);
            _tprintf(TEXT("%s\n"), response);
        }
    }
    else if (_tcscmp(command, TEXT("listc")) == 0) {
        TCHAR response[MSG_TAM];
        ListCompanies(stateServ, response);
        _tprintf(TEXT("%s\n"), response);
    }
    else if (_tcsncmp(command, TEXT("stock"), 5) == 0) {
        TCHAR nomeEmpresa[50];
        double newPrice;
        if (_stscanf_s(command, TEXT("stock %49s %lf"), nomeEmpresa, (unsigned)_countof(nomeEmpresa), &newPrice) == 2) {
            TCHAR response[MSG_TAM];
            SetStockPrice(stateServ, nomeEmpresa, newPrice, response);
            _tprintf(TEXT("%s\n"), response);
        }
    }
    else if (_tcscmp(command, TEXT("users")) == 0) {
        TCHAR response[MSG_TAM];
        ListUsers(stateServ, response);
        _tprintf(TEXT("%s\n"), response);
    }
    else if (_tcsncmp(command, TEXT("pause"), 5) == 0) {
        int duration;
        if (_stscanf_s(command, TEXT("pause %d"), &duration) == 1) {
            TCHAR response[MSG_TAM];
            PauseTrading(stateServ, duration, response);
            _tprintf(TEXT("%s\n"), response);
        }
    }
    else if (_tcscmp(command, TEXT("close")) == 0) {
        TCHAR response[MSG_TAM];
        CloseSystem(stateServ, response);
        _tprintf(TEXT("%s\n"), response);
        *closeFlag = TRUE;
    }
    else {
        _tprintf(TEXT("Comando inválido. Tente novamente.\n"));
    }
}

DWORD WINAPI InstanceThread(LPVOID lpvParam) {
    ServerState* stateServ = (ServerState*)lpvParam;
    HANDLE hPipe = (HANDLE)stateServ->clientPipes[0]; // Use the first pipe for simplicity
    Msg msgRequest, msgResponse;
    DWORD bytesRead = 0;
    BOOL fSuccess;
    OVERLAPPED overl = { 0 };
    overl.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

    while (stateServ->running) {
        ZeroMemory(&msgRequest, sizeof(Msg));
        ResetEvent(overl.hEvent);
        fSuccess = ReadFile(hPipe, &msgRequest, sizeof(Msg), &bytesRead, &overl);

        if (!fSuccess && GetLastError() == ERROR_IO_PENDING) {
            WaitForSingleObject(overl.hEvent, INFINITE);
            if (!GetOverlappedResult(hPipe, &overl, &bytesRead, FALSE) || bytesRead == 0) {
                if (GetLastError() == ERROR_BROKEN_PIPE) {
                    _tprintf(TEXT("Client disconnected.\n"));
                    break;
                }
                else {
                    PrintLastError(TEXT("ReadFile failed"));
                    continue;
                }
            }
        }
        else if (!fSuccess) {
            PrintLastError(TEXT("ReadFile failed"));
            break;
        }

        _tprintf(TEXT("Received message: %s\n"), msgRequest.msg);

        msgResponse = msgRequest;

        if (_tcscmp(msgRequest.msg, TEXT("exit")) == 0) {
            _tprintf(TEXT("Client requested disconnect.\n"));
            removeCliente(stateServ, hPipe);
            break;
        }

        fSuccess = WriteFile(hPipe, &msgResponse, sizeof(Msg), NULL, &overl);
        if (!fSuccess && GetLastError() == ERROR_IO_PENDING) {
            WaitForSingleObject(overl.hEvent, INFINITE);
            if (!GetOverlappedResult(hPipe, &overl, NULL, FALSE)) {
                PrintLastError(TEXT("WriteFile failed"));
            }
        }
    }

    CloseHandle(overl.hEvent);
    DisconnectNamedPipe(hPipe);
    CloseHandle(hPipe);

    return 0;
}

DWORD WINAPI AdminCommandThread(LPVOID lpvParam) {
    ServerState* stateServ = (ServerState*)lpvParam;
    TCHAR command[MSG_TAM];
    BOOL closeFlag = FALSE;

    while (!closeFlag) {
        _tprintf(TEXT("\nDigite um comando administrativo: "));
        _fgetts(command, MSG_TAM, stdin);
        command[_tcslen(command) - 1] = '\0'; // Remover o caractere de nova linha



    }

    return 0;
}

int _tmain(int argc, TCHAR* argv[]) {
    if (argc < 2) {
        _tprintf(TEXT("Uso: %s <ficheiro_utilizadores>\n"), argv[0]);
        return 1;
    }

    ServerState stateServ;
    InitializeServerState(&stateServ);
    ReadUsersFromFile(&stateServ, argv[1]);

#ifdef UNICODE
    _setmode(_fileno(stdin), _O_WTEXT);
    _setmode(_fileno(stdout), _O_WTEXT);
    _setmode(_fileno(stderr), _O_WTEXT);
#endif

    PrintMenu();

    HANDLE hAdminThread = CreateThread(
        NULL, 0,
        AdminCommandThread,
        &stateServ,
        0, NULL
    );

    if (hAdminThread == NULL) {
        PrintLastError(TEXT("AdminCommandThread creation failed"));
        return -1;
    }

    // Criação de memória partilhada e sincronização
    hMapFile = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof(SharedData), SHARED_MEM_NAME);
    if (hMapFile == NULL) {
        PrintLastError(TEXT("CreateFileMapping failed"));
        return -1;
    }

    pSharedData = (SharedData*)MapViewOfFile(hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(SharedData));
    if (pSharedData == NULL) {
        PrintLastError(TEXT("MapViewOfFile failed"));
        CloseHandle(hMapFile);
        return -1;
    }

    hMutex = CreateMutex(NULL, FALSE, MUTEX_NAME);
    if (hMutex == NULL) {
        PrintLastError(TEXT("CreateMutex failed"));
        UnmapViewOfFile(pSharedData);
        CloseHandle(hMapFile);
        return -1;
    }

    hEvent = CreateEvent(NULL, TRUE, FALSE, EVENT_NAME);
    if (hEvent == NULL) {
        PrintLastError(TEXT("CreateEvent failed"));
        CloseHandle(hMutex);
        UnmapViewOfFile(pSharedData);
        CloseHandle(hMapFile);
        return -1;
    }

    LPTSTR lpszPipename = TEXT("\\\\.\\pipe\\teste");

    while (1) {
        _tprintf(TEXT("\nServer - main loop - creating named pipe - %s"), lpszPipename);

        HANDLE hPipe = CreateNamedPipe(
            lpszPipename,
            PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
            PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
            PIPE_UNLIMITED_INSTANCES,
            BUFSIZ, BUFSIZ,
            5000,
            NULL
        );

        if (hPipe == INVALID_HANDLE_VALUE) {
            PrintLastError(TEXT("CreateNamedPipe failed"));
            break;
        }

        BOOL fConnected = ConnectNamedPipe(hPipe, NULL) ? TRUE : (GetLastError() == ERROR_PIPE_CONNECTED);

        if (fConnected) {
            _tprintf(TEXT("\nClient connected. Creating a thread for it."));

            stateServ.clientPipes[0] = hPipe; // Use the first pipe for simplicity

            HANDLE hThread = CreateThread(
                NULL, 0,
                InstanceThread,
                &stateServ,
                0, NULL
            );

            if (hThread == NULL) {
                PrintLastError(TEXT("Thread creation failed"));
                CloseHandle(hPipe);
                break;
            }
            else {
                CloseHandle(hThread);
            }
        }
        else {
            CloseHandle(hPipe);
        }
    }

    WaitForSingleObject(hAdminThread, INFINITE);
    CloseHandle(hAdminThread);

    CleanUpServer(&stateServ);

    return 0;
}
