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
            wcscpy_s(state->utilizadores[state->numUtilizadores].password, _countof(state->utilizadores[state->numUtilizadores].password), password);
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

void InitializeServerState(ServerState* stateServ) {
    for (int i = 0; i < MAXCLIENTES; ++i) {
        stateServ->clientPipes[i] = NULL;
    }
    stateServ->writeReady = CreateEvent(NULL, TRUE, FALSE, NULL);
    stateServ->readEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    stateServ->numEmpresas = 0;
    stateServ->numUtilizadores = 0;
    stateServ->tradingPaused = FALSE;
    stateServ->closeFlag = FALSE;
    stateServ->closeMutex = CreateMutex(NULL, FALSE, NULL);
    stateServ->closeEvent = CreateEvent(NULL, TRUE, FALSE, CLOSE_EVENT_NAME); // Evento para sinalizar encerramento
}

void PrintMenu() {
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
    _tprintf(TEXT("\n%s: %s (Error %d)\n"), msg, sysMsg, eNum);
}

int adicionaCliente(ServerState* stateServ, HANDLE hCli) {
    for (int i = 0; i < MAXCLIENTES; ++i) {
        if (stateServ->clientPipes[i] == NULL) {
            stateServ->clientPipes[i] = hCli;
            return i;
        }
    }

    return -1;
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
void CloseSystem(ServerState* state, TCHAR* response) {
    _tcscpy_s(response, MSG_TAM, TEXT("Sistema encerrado.\n"));
    SetEvent(state->closeEvent); // Sinalizar o evento de encerramento

    // Enviar mensagem especial de encerramento para os clientes
    for (int i = 0; i < MAXCLIENTES; ++i) {
        HANDLE hPipe = state->clientPipes[i];
        if (hPipe != NULL) {
            // Enviar mensagem de encerramento
            Msg msg;
            _tcscpy_s(msg.msg, MSG_TAM, TEXT("Bolsa encerrada."));
            DWORD bytesWritten;
            BOOL fSuccess = WriteFile(hPipe, &msg, sizeof(Msg), &bytesWritten, NULL);
            if (!fSuccess || bytesWritten == 0) {
                // Se houver falha ao enviar a mensagem, fecha a conexão
                DisconnectNamedPipe(hPipe);
                CloseHandle(hPipe);
                state->clientPipes[i] = NULL;
            }
        }
    }

    // Esperar que todos os clientes desconectem-se
    Sleep(1000);

    // Fechar eventos do servidor
    CloseHandle(state->readEvent);
    state->readEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
}

BOOL verificaLogin(const ServerState* stateServ, const TCHAR* username, const TCHAR* password) {
    for (int i = 0; i < stateServ->numUtilizadores; ++i) {
        if (_tcscmp(stateServ->utilizadores[i].username, username) == 0 &&
            _tcscmp(stateServ->utilizadores[i].password, password) == 0) {
            return TRUE;
        }
    }
    return FALSE;
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
    }
    else {
        _tprintf(TEXT("Comando inválido. Tente novamente.\n"));
    }
}

DWORD WINAPI InstanceThread(LPVOID lpvParam) {
    ServerState* stateServ = (ServerState*)lpvParam;
    HANDLE hPipe = stateServ->currentPipe;
    TCHAR buffer[MSG_TAM * 2]; // Buffer para armazenar dados lidos parcialmente
    Msg msgResponse;
    DWORD bytesRead = 0;
    BOOL fSuccess;
    OVERLAPPED overl = { 0 };
    overl.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

    ZeroMemory(buffer, sizeof(buffer));

    while (1) {
        ResetEvent(overl.hEvent);
        fSuccess = ReadFile(hPipe, buffer, sizeof(buffer) - sizeof(TCHAR), &bytesRead, &overl);

        if (!fSuccess && GetLastError() == ERROR_IO_PENDING) {
            WaitForSingleObject(overl.hEvent, INFINITE);
            fSuccess = GetOverlappedResult(hPipe, &overl, &bytesRead, FALSE);
        }

        if (!fSuccess || bytesRead == 0) {
            if (GetLastError() == ERROR_BROKEN_PIPE) {
                _tprintf(TEXT("Cliente desconectado.\n"));
                break;
            }
            else if (GetLastError() == ERROR_MORE_DATA) {
                // Continue lendo até obter todos os dados
                DWORD totalBytesRead = bytesRead;
                while (GetLastError() == ERROR_MORE_DATA) {
                    fSuccess = ReadFile(hPipe, buffer + totalBytesRead, sizeof(buffer) - totalBytesRead - sizeof(TCHAR), &bytesRead, &overl);
                    if (!fSuccess && GetLastError() == ERROR_IO_PENDING) {
                        WaitForSingleObject(overl.hEvent, INFINITE);
                        fSuccess = GetOverlappedResult(hPipe, &overl, &bytesRead, FALSE);
                    }
                    totalBytesRead += bytesRead;
                }
                buffer[totalBytesRead / sizeof(TCHAR)] = TEXT('\0'); // Assegure-se de terminar a string
            }
            else {
                PrintLastError(TEXT("ReadFile falhou"));
                break;
            }
        }

        buffer[bytesRead / sizeof(TCHAR)] = TEXT('\0'); // Assegure-se de terminar a string

        _tprintf(TEXT("Received message: %s\n"), buffer);

        if (_tcsncmp(buffer, TEXT("login "), 6) == 0) {
            TCHAR username[50];
            TCHAR password[50];

            if (_stscanf_s(buffer + 6, TEXT("%49s %49s"), username, (unsigned)_countof(username), password, (unsigned)_countof(password)) == 2) {
                // Verificar login
                BOOL loginValido = verificaLogin(stateServ, username, password);
                if (loginValido) {
                    _tprintf(TEXT("Login bem-sucedido para o usuário: %s\n"), username);
                    _tcscpy_s(msgResponse.msg, MSG_TAM, TEXT("login_success"));
                    for (int i = 0; i < stateServ->numUtilizadores; ++i) {
                        if (_tcscmp(stateServ->utilizadores[i].username, username) == 0) {
                            stateServ->utilizadores[i].isOnline = TRUE;
                            break;
                        }
                    }
                }
                else {
                    _tprintf(TEXT("Falha no login para o usuário: %s\n"), username);
                    _tcscpy_s(msgResponse.msg, MSG_TAM, TEXT("login_failed"));
                }

                // Enviar resposta ao cliente
                DWORD cWritten;
                fSuccess = WriteFile(hPipe, &msgResponse, sizeof(Msg), &cWritten, &overl);
                if (!fSuccess && GetLastError() == ERROR_IO_PENDING) {
                    WaitForSingleObject(overl.hEvent, INFINITE);
                    GetOverlappedResult(hPipe, &overl, &cWritten, FALSE);
                }

                if (!fSuccess) {
                    PrintLastError(TEXT("WriteFile falhou"));
                    break;
                }
            }
        }
        else {
            // Processar outros comandos aqui...
            _tcscpy_s(msgResponse.msg, MSG_TAM, TEXT("Comando não reconhecido."));
            DWORD cWritten;
            fSuccess = WriteFile(hPipe, &msgResponse, sizeof(Msg), &cWritten, &overl);
            if (!fSuccess && GetLastError() == ERROR_IO_PENDING) {
                WaitForSingleObject(overl.hEvent, INFINITE);
                GetOverlappedResult(hPipe, &overl, &cWritten, FALSE);
            }

            if (!fSuccess) {
                PrintLastError(TEXT("WriteFile falhou"));
                break;
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
            return -1;
        }

        OVERLAPPED ol = { 0 };
        ol.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
        if (ol.hEvent == NULL) {
            PrintLastError(TEXT("CreateEvent failed for OVERLAPPED"));
            CloseHandle(hPipe);
            return -1;
        }

        BOOL fConnected = ConnectNamedPipe(hPipe, &ol) ? TRUE : (GetLastError() == ERROR_PIPE_CONNECTED);
        if (!fConnected) {
            if (GetLastError() == ERROR_IO_PENDING) {
                HANDLE waitHandles[] = { ol.hEvent, stateServ.closeEvent };
                DWORD dwWait = WaitForMultipleObjects(2, waitHandles, FALSE, INFINITE);
                if (dwWait == WAIT_OBJECT_0) {
                    fConnected = TRUE;
                }
                else if (dwWait == WAIT_OBJECT_0 + 1) {
                    _tprintf(TEXT("Encerrando servidor principal.\n"));
                    CloseHandle(hPipe);
                    CloseHandle(ol.hEvent);
                    break;
                }
                else {
                    PrintLastError(TEXT("WaitForMultipleObjects failed"));
                    CloseHandle(hPipe);
                    CloseHandle(ol.hEvent);
                    return -1;
                }
            }
            else {
                PrintLastError(TEXT("ConnectNamedPipe failed"));
                CloseHandle(hPipe);
                CloseHandle(ol.hEvent);
                return -1;
            }
        }

        if (fConnected) {
            _tprintf(TEXT("\nClient connected. Creating a thread for it."));

            // Encontrar um slot vazio para o novo cliente
            int slot = adicionaCliente(&stateServ, hPipe);

            if (slot == -1) {
                _tprintf(TEXT("Número máximo de clientes atingido.\n"));
                DisconnectNamedPipe(hPipe);
                CloseHandle(hPipe);
                CloseHandle(ol.hEvent);
                continue;
            }

            stateServ.currentPipe = hPipe; // Configura o pipe atual no estado do servidor

            HANDLE hThread = CreateThread(
                NULL, 0,
                InstanceThread,
                (LPVOID)&stateServ, // Passar o ponteiro do estado do servidor para a thread
                0, NULL
            );

            if (hThread == NULL) {
                PrintLastError(TEXT("Thread creation failed"));
                CloseHandle(hPipe);
                CloseHandle(ol.hEvent);
                return -1;
            }
            else {
                CloseHandle(hThread);
            }
        }
        else {
            CloseHandle(hPipe);
            CloseHandle(ol.hEvent);
        }

        // Verificar se o sistema deve ser fechado
        if (WaitForSingleObject(stateServ.closeEvent, 0) == WAIT_OBJECT_0) {
            _tprintf(TEXT("Encerrando servidor principal.\n"));
            break;
        }
    }

    // Encerrar todos os clientes restantes
    for (int i = 0; i < MAXCLIENTES; ++i) {
        if (stateServ.clientPipes[i] != NULL) {
            DisconnectNamedPipe(stateServ.clientPipes[i]);
            CloseHandle(stateServ.clientPipes[i]);
            stateServ.clientPipes[i] = NULL;
        }
    }

    ReleaseMutex(stateServ.closeMutex);
    CloseHandle(hAdminThread);
    CloseHandle(stateServ.closeEvent);
    CloseHandle(stateServ.closeMutex);

    UnmapViewOfFile(pSharedData);
    CloseHandle(hMapFile);
    CloseHandle(hMutex);
    CloseHandle(hEvent);

    return 0;
}