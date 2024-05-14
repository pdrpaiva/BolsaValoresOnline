#include <stdio.h>
#include <io.h>
#include <fcntl.h>
#include "../utils.h"

void InitializeServerState(ServerState* stateServ) {
    for (int i = 0; i < MAXCLIENTES; ++i) {
        stateServ->clientPipes[i] = NULL;
    }
    stateServ->writeReady = CreateEvent(NULL, TRUE, FALSE, NULL);
    stateServ->readEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    stateServ->numEmpresas = 0;
    stateServ->numUtilizadores = 0;
    stateServ->tradingPaused = FALSE;
}

void PrintMenu() {


#ifdef UNICODE
    _setmode(_fileno(stdin), _O_WTEXT);
    _setmode(_fileno(stdout), _O_WTEXT);
    _setmode(_fileno(stderr), _O_WTEXT);
#endif

    _tprintf(TEXT("\n--- Comandos da Bolsa ---\n\n"));
    _tprintf(TEXT("addc <nome-empresa> <número-ações> <preço-ação> - Adicionar empresa\n"));
    _tprintf(TEXT("listc - Listar todas as empresas\n"));
    _tprintf(TEXT("stock <nome-empresa> <novo-preço> - Alterar preço das ações\n"));
    _tprintf(TEXT("users - Listar utilizadores\n"));
    _tprintf(TEXT("pause <segundos> - Pausar operações\n"));
    _tprintf(TEXT("close - Encerrar sistema\n"));
    _tprintf(TEXT("Digite um comando:\n"));
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

void AddCompany(ServerState* state, const TCHAR* nomeEmpresa, int numAcoes, double precoAcao, TCHAR* response) {
    if (state->numEmpresas < MAX_EMPRESAS) {
        _tcsncpy_s(state->empresas[state->numEmpresas].nomeEmpresa, _countof(state->empresas[state->numEmpresas].nomeEmpresa), nomeEmpresa, _TRUNCATE);
        state->empresas[state->numEmpresas].numAcoes = numAcoes;
        state->empresas[state->numEmpresas].precoAcao = precoAcao;
        state->numEmpresas++;
        _stprintf_s(response, MSG_TAM, TEXT("Empresa %s adicionada com %d ações a %.2f cada.\n"), nomeEmpresa, numAcoes, precoAcao);
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
            return;
        }
    }
    _stprintf_s(response, MSG_TAM, TEXT("Empresa %s não encontrada.\n"), nomeEmpresa);
}

void ListUsers(const ServerState* state, TCHAR* response) {
    TCHAR buffer[MSG_TAM];
    _stprintf_s(response, MSG_TAM, TEXT("Utilizadores na Bolsa:\n"));
    for (int i = 0; i < state->numUtilizadores; ++i) {
        _stprintf_s(buffer, MSG_TAM, TEXT("Utilizador: %s, Saldo: %.2f, Online: %s\n"), state->utilizadores[i].username, state->utilizadores[i].saldo, state->utilizadores[i].isOnline ? TEXT("Sim") : TEXT("Não"));
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

void CloseSystem(ServerState* state, TCHAR* response) {
    _tcscpy_s(response, MSG_TAM, TEXT("Sistema encerrado.\n"));
    for (int i = 0; i < MAXCLIENTES; ++i) {
        if (state->clientPipes[i] != NULL) {
            DisconnectNamedPipe(state->clientPipes[i]);
            CloseHandle(state->clientPipes[i]);
        }
    }
}

void ProcessAdminCommand(ServerState* stateServ, TCHAR* command) {
    if (_tcsncmp(command, TEXT("addc"), 4) == 0) {
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
        // Não usar ExitProcess, apenas marcar o estado para que o loop possa encerrar graciosamente
        exit(0); // Sair do programa completamente
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

    while (1) {
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

    while (1) {
        _tprintf(TEXT("\nDigite um comando administrativo: "));
        _fgetts(command, MSG_TAM, stdin);
        command[_tcslen(command) - 1] = '\0'; // Remover o caractere de nova linha
        ProcessAdminCommand(stateServ, command);
    }

    return 0;
}

int _tmain(void) {
    ServerState stateServ;

    InitializeServerState(&stateServ);

    PrintMenu();

    _setmode(_fileno(stdout), _O_WTEXT);
    _setmode(_fileno(stdin), _O_WTEXT);
    _setmode(_fileno(stderr), _O_WTEXT);

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
            return -1;
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
                return -1;
            }
            else {
                CloseHandle(hThread);
            }
        }
        else {
            CloseHandle(hPipe);
        }
    }

    CloseHandle(hAdminThread);

    return 0;
}
