﻿#include <stdio.h>
#include <io.h>
#include <fcntl.h>
#include "../utils.h"

void UpdateSharedData(ServerState* state) {
    WaitForSingleObject(state->hMutex, INFINITE);
    SharedData* pSharedData = (SharedData*)MapViewOfFile(state->hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(SharedData));
    if (pSharedData != NULL) {
        pSharedData->numEmpresas = state->numEmpresas;
        for (int i = 0; i < state->numEmpresas; ++i) {
            pSharedData->empresas[i] = state->empresas[i];
        }
        SetEvent(state->hEvent);
        UnmapViewOfFile(pSharedData);
    }
    ReleaseMutex(state->hMutex);
}

BOOL InitializeSharedResources(ServerState* state) {
    state->hMapFile = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof(SharedData), SHARED_MEM_NAME);
    if (state->hMapFile == NULL) {
        PrintLastError(TEXT("CreateFileMapping failed"));
        return FALSE;
    }

    state->hMutex = CreateMutex(NULL, FALSE, MUTEX_NAME);
    if (state->hMutex == NULL) {
        PrintLastError(TEXT("CreateMutex failed"));
        CloseHandle(state->hMapFile);
        return FALSE;
    }

    state->hEvent = CreateEvent(NULL, TRUE, FALSE, EVENT_NAME);
    if (state->hEvent == NULL) {
        PrintLastError(TEXT("CreateEvent failed"));
        CloseHandle(state->hMutex);
        CloseHandle(state->hMapFile);
        return FALSE;
    }

    return TRUE;
}

void CleanupSharedResources(ServerState* state) {
    CloseHandle(state->hEvent);
    CloseHandle(state->hMutex);
    CloseHandle(state->hMapFile);
}

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
            state->utilizadores[state->numUtilizadores].numAcoes = 0;
            state->numUtilizadores++;
        }
        else {
            _tprintf(TEXT("Número máximo de utilizadores atingido.\n"));
            break;
        }
    }

    fclose(file);
}


DWORD WINAPI ResumeTradingAfterPause(LPVOID lpParam) {
    ServerState* state = (ServerState*)lpParam;
    Sleep(state->pauseDuration * 1000);
    state->tradingPaused = FALSE;
    _tprintf(TEXT("Operações de compra e venda retomadas.\n"));
    //opcional
    NotifyClients(state, TEXT("Operações de compra e venda foram retomadas.\n"));
    return 0;
}

void PauseTrading(ServerState* state, int duration, TCHAR* response) {
    if (state->tradingPaused) {
        _stprintf_s(response, MSG_TAM, TEXT("As operações já estão suspensas.\n"));
        return;
    }

    state->tradingPaused = TRUE;
    state->pauseDuration = duration;
    _stprintf_s(response, MSG_TAM, TEXT("Operações de compra e venda suspensas por %d segundos.\n"), duration);

    NotifyClients(state, TEXT("Operações de compra e venda foram suspensas.\n"));

    // Cria uma thread para gerenciar o tempo de pausa
    CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)ResumeTradingAfterPause, state, 0, NULL);
}

void NotifyClients(ServerState* state, const TCHAR* message) {
    for (int i = 0; i < MAXCLIENTES; ++i) {
        HANDLE hPipe = state->clientPipes[i];
        if (hPipe != NULL) {
            Msg msg;
            _tcscpy_s(msg.msg, MSG_TAM, message);
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
}

double CheckBalance(ServerState* state, TCHAR* username) {
    for (int i = 0; i < state->numUtilizadores; i++) {
        if (_tcscmp(state->utilizadores[i].username, username) == 0) {
            return state->utilizadores[i].saldo;
        }
    }
}


BOOL RegistrarVenda(Utilizador* utilizador, const TCHAR* nomeEmpresa, int numAcoes) {
    for (int i = 0; i < utilizador->numAcoes; ++i) {
        if (_tcscmp(utilizador->carteira[i].nomeEmpresa, nomeEmpresa) == 0) {
            utilizador->carteira[i].numAcoes -= numAcoes;
            if (utilizador->carteira[i].numAcoes == 0) {
                // Remove a empresa da carteira se as ações chegarem a zero
                for (int j = i; j < utilizador->numAcoes - 1; ++j) {
                    utilizador->carteira[j] = utilizador->carteira[j + 1];
                }
                utilizador->numAcoes--;
            }
            return TRUE;
        }
    }
    return FALSE; //Empresa não foi encontrada
}


BOOL RegistrarCompra(Utilizador* utilizador, const TCHAR* nomeEmpresa, int numAcoes) {
    for (int i = 0; i < utilizador->numAcoes; ++i) {
        if (_tcscmp(utilizador->carteira[i].nomeEmpresa, nomeEmpresa) == 0) {
            utilizador->carteira[i].numAcoes += numAcoes;
            return TRUE;
        }
    }

    if (utilizador->numAcoes < MAX_ACOES) {
        _tcscpy_s(utilizador->carteira[utilizador->numAcoes].nomeEmpresa, _countof(utilizador->carteira[utilizador->numAcoes].nomeEmpresa), nomeEmpresa);
        utilizador->carteira[utilizador->numAcoes].numAcoes = numAcoes;
        utilizador->numAcoes++;
        return TRUE;
    }
    else {
        return FALSE;
    }
}

void RegistrarTransacao(ServerState* state, const TCHAR* nomeEmpresa, int numAcoes, double valor) {
    WaitForSingleObject(state->hMutex, INFINITE); // Aguarde o mutex para sincronizar o acesso
    SharedData* pSharedData = (SharedData*)MapViewOfFile(state->hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(SharedData));
    if (pSharedData != NULL) {
        // Atualiza a transação diretamente na memória compartilhada
        _tcscpy_s(pSharedData->ultimaTransacao.nomeEmpresa, 50, nomeEmpresa);
        pSharedData->ultimaTransacao.numAcoes = numAcoes;
        pSharedData->ultimaTransacao.valor = valor;
        //SetEvent(state->hEvent); // Sinalize o evento para notificar os clientes
        UnmapViewOfFile(pSharedData);
    }
    ReleaseMutex(state->hMutex); // Libere o mutex
}


void BuyShares(ServerState* state, const TCHAR* nomeEmpresa, int numAcoes, const TCHAR* username, TCHAR* response) {
    if (state->tradingPaused) {
        _stprintf_s(response, MSG_TAM, TEXT("Operações de compra estão suspensas. Tente novamente mais tarde.\n"));
        return;
    }
    for (int i = 0; i < state->numEmpresas; ++i) {
        if (_tcscmp(state->empresas[i].nomeEmpresa, nomeEmpresa) == 0) {
            if (state->empresas[i].numAcoes >= numAcoes) {
                for (int j = 0; j < state->numUtilizadores; ++j) {
                    if (_tcscmp(state->utilizadores[j].username, username) == 0) {
                        if (state->utilizadores[j].saldo >= state->empresas[i].precoAcao * numAcoes) {

                            BOOL sucesso = RegistrarCompra(&state->utilizadores[j], nomeEmpresa, numAcoes);
                            if (sucesso) {
                                state->empresas[i].numAcoes -= numAcoes;
                                state->utilizadores[j].saldo -= state->empresas[i].precoAcao * numAcoes;
                                state->empresas[i].precoAcao = state->empresas[i].precoAcao * pow(1 + 0.01, numAcoes);
                                _stprintf_s(response, MSG_TAM, TEXT("Compra realizada: %d ações de %s.\n"), numAcoes, nomeEmpresa);
                                RegistrarTransacao(state, nomeEmpresa, numAcoes, state->empresas[i].precoAcao * numAcoes); // Registra a transação
                                UpdateSharedData(state);
                                SetEvent(state->hEvent); // Sinaliza o evento de atualização
                                return;
                            }
                            else {
                                _stprintf_s(response, MSG_TAM, TEXT("Carteira cheia, compra não realizada!\n"));
                            }
                            return;
                        }
                        else {
                            _stprintf_s(response, MSG_TAM, TEXT("Saldo insuficiente. Compra não realizada.\n"));
                            return;
                        }
                    }
                }
            }
            else {
                _stprintf_s(response, MSG_TAM, TEXT("Ações insuficientes. Compra não realizada.\n"));
                return;
            }
        }
    }
    _stprintf_s(response, MSG_TAM, TEXT("Empresa não encontrada.\n"));
}

void SellShares(ServerState* state, const TCHAR* nomeEmpresa, int numAcoes, const TCHAR* username, TCHAR* response) {
    if (state->tradingPaused) {
        _stprintf_s(response, MSG_TAM, TEXT("Operações de venda estão suspensas. Tente novamente mais tarde.\n"));
        return;
    }
    for (int i = 0; i < state->numUtilizadores; ++i) {
        if (_tcscmp(state->utilizadores[i].username, username) == 0) {
            for (int j = 0; j < state->utilizadores[i].numAcoes; ++j) {
                if (_tcscmp(state->utilizadores[i].carteira[j].nomeEmpresa, nomeEmpresa) == 0) {
                    if (state->utilizadores[i].carteira[j].numAcoes >= numAcoes) {
                        for (int k = 0; k < state->numEmpresas; ++k) {
                            if (_tcscmp(state->empresas[k].nomeEmpresa, nomeEmpresa) == 0) {
                                BOOL sucesso = RegistrarVenda(&state->utilizadores[i], nomeEmpresa, numAcoes);
                                if (sucesso) {
                                    state->empresas[k].numAcoes += numAcoes;
                                    state->utilizadores[i].saldo += state->empresas[k].precoAcao * numAcoes;
                                    state->empresas[i].precoAcao = state->empresas[i].precoAcao * pow(1 - 0.01, numAcoes);
                                    _stprintf_s(response, MSG_TAM, TEXT("Venda realizada: %d ações de %s.\n"), numAcoes, nomeEmpresa);
                                    RegistrarTransacao(state, nomeEmpresa, numAcoes, state->empresas[k].precoAcao * numAcoes);
                                    UpdateSharedData(state);
                                    SetEvent(state->hEvent); // Sinaliza o evento de atualização
                                    return;
                                }
                                else {
                                    _stprintf_s(response, MSG_TAM, TEXT("Erro ao registar a venda.\n"));
                                }
                                return;
                            }
                        }
                    }
                    else {
                        _stprintf_s(response, MSG_TAM, TEXT("Ações insuficientes para venda. Venda não realizada.\n"));
                        return;
                    }
                }
            }
        }
    }
    _stprintf_s(response, MSG_TAM, TEXT("Ações não encontradas na carteira do utilizador ou empresa não encontrada.\n"));
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
    _tprintf(TEXT("addc <nome-empresa> <número-ações> <preço-acão> - Adicionar empresa\n"));
    _tprintf(TEXT("listc - Listar todas as empresas\n"));
    _tprintf(TEXT("stock <nome-empresa> <novo-preço> - Alterar preço das ações\n"));
    _tprintf(TEXT("users - Listar utilizadores\n"));
    _tprintf(TEXT("pause <segundos> - Pausar opera��es\n"));
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

void AddCompany(ServerState* state, const TCHAR* nomeEmpresa, int numAcoes, double precoAcao, TCHAR* response) {
    if (state->numEmpresas < MAX_EMPRESAS) {

        for (int i = 0; i < state->numEmpresas; i++) {
            if (_tcscmp(state->empresas[i].nomeEmpresa, nomeEmpresa) == 0) {
                _stprintf_s(response, MSG_TAM, TEXT("Já existe uma empresa com o mesmo nome.\n"));
                return;
            }
        }


        _tcsncpy_s(state->empresas[state->numEmpresas].nomeEmpresa, _countof(state->empresas[state->numEmpresas].nomeEmpresa), nomeEmpresa, _TRUNCATE);
        state->empresas[state->numEmpresas].numAcoes = numAcoes;
        state->empresas[state->numEmpresas].precoAcao = precoAcao;
        state->numEmpresas++;
        _stprintf_s(response, MSG_TAM, TEXT("Empresa %s adicionada com %d ações a %.2f cada.\n"), nomeEmpresa, numAcoes, precoAcao);
        UpdateSharedData(state);
        SetEvent(state->hEvent);
        return;
    }
    else {
        _stprintf_s(response, MSG_TAM, TEXT("Limite de empresas atingido.\n"));
    }
}

void ListCompanies(const ServerState* state, TCHAR* response) {
    TCHAR buffer[MSG_TAM];
    _stprintf_s(response, MSG_TAM, TEXT("Empresas na Bolsa:\n"));
    for (int i = 0; i < state->numEmpresas; ++i) {
        _stprintf_s(buffer, MSG_TAM, TEXT("Empresa: %s,Ações: %d, PreÇo: %.2f\n"), state->empresas[i].nomeEmpresa, state->empresas[i].numAcoes, state->empresas[i].precoAcao);
        _tcscat_s(response, MSG_TAM, buffer);
    }
}

void SetStockPrice(ServerState* state, const TCHAR* nomeEmpresa, double newPrice, TCHAR* response) {
    for (int i = 0; i < state->numEmpresas; ++i) {
        if (_tcscmp(state->empresas[i].nomeEmpresa, nomeEmpresa) == 0) {
            state->empresas[i].precoAcao = newPrice;
            _stprintf_s(response, MSG_TAM, TEXT("Preço da empresa %s alterado para %.2f.\n"), nomeEmpresa, newPrice);
            UpdateSharedData(state);
            SetEvent(state->hEvent); // Sinaliza o evento de atualização
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
            state->utilizadores[i].isOnline ? TEXT("Sim") : TEXT("N�o"));
        _tcscat_s(response, MSG_TAM, buffer);
    }
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
                // Se houver falha ao enviar a mensagem, fecha a conex�o
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
        _tprintf(TEXT("Comando inv�lido. Tente novamente.\n"));
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
                DWORD totalBytesRead = bytesRead;
                while (GetLastError() == ERROR_MORE_DATA) {
                    fSuccess = ReadFile(hPipe, buffer + totalBytesRead, sizeof(buffer) - totalBytesRead - sizeof(TCHAR), &bytesRead, &overl);
                    if (!fSuccess && GetLastError() == ERROR_IO_PENDING) {
                        WaitForSingleObject(overl.hEvent, INFINITE);
                        fSuccess = GetOverlappedResult(hPipe, &overl, &bytesRead, FALSE);
                    }
                    totalBytesRead += bytesRead;
                }
                buffer[totalBytesRead / sizeof(TCHAR)] = TEXT('\0');
            }
            else {
                PrintLastError(TEXT("ReadFile falhou"));
                break;
            }
        }

        buffer[bytesRead / sizeof(TCHAR)] = TEXT('\0');

        _tprintf(TEXT("Received message: %s\n"), buffer);

        if (_tcscmp(buffer, TEXT("listc")) == 0) {
            TCHAR response[MSG_TAM];
            ListCompanies(stateServ, response);
            _tcscpy_s(msgResponse.msg, MSG_TAM, response);

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
        else if (_tcsncmp(buffer, TEXT("buy "), 4) == 0) {
            TCHAR nomeEmpresa[50];
            int numAcoes;
            TCHAR username[50];

            if (_stscanf_s(buffer + 4, TEXT("%49s %d %49s"), nomeEmpresa, (unsigned)_countof(nomeEmpresa), &numAcoes, username, (unsigned)_countof(username)) == 3) {
                TCHAR response[MSG_TAM];
                BuyShares(stateServ, nomeEmpresa, numAcoes, username, response);
                _tcscpy_s(msgResponse.msg, MSG_TAM, response);

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
        else if (_tcsncmp(buffer, TEXT("sell "), 5) == 0) {
            TCHAR nomeEmpresa[50];
            int numAcoes;
            TCHAR username[50];

            if (_stscanf_s(buffer + 5, TEXT("%49s %d %49s"), nomeEmpresa, (unsigned)_countof(nomeEmpresa), &numAcoes, username, (unsigned)_countof(username)) == 3) {
                TCHAR response[MSG_TAM];
                SellShares(stateServ, nomeEmpresa, numAcoes, username, response);
                _tcscpy_s(msgResponse.msg, MSG_TAM, response);

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
        else if (_tcsncmp(buffer, TEXT("login "), 6) == 0) {
            TCHAR username[50];
            TCHAR password[50];

            if (_stscanf_s(buffer + 6, TEXT("%49s %49s"), username, (unsigned)_countof(username), password, (unsigned)_countof(password)) == 2) {
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
        else if (_tcsncmp(buffer, TEXT("balance "), 8) == 0){ 
            TCHAR username[50];

            if (_stscanf_s(buffer + 8, TEXT("%49s"), username, (unsigned)_countof(username)) == 1) {
                double saldo = CheckBalance(stateServ, username);
                if (saldo != -1) {
                    Msg msgResponse;
                    _sntprintf_s(msgResponse.msg, MSG_TAM, _TRUNCATE, TEXT("Saldo: %.2lf €"), saldo);

                    DWORD cWritten;
                    BOOL fSuccess = WriteFile(hPipe, &msgResponse, sizeof(Msg), &cWritten, &overl);
                    if (!fSuccess && GetLastError() == ERROR_IO_PENDING) {
                        WaitForSingleObject(overl.hEvent, INFINITE);
                        fSuccess = GetOverlappedResult(hPipe, &overl, &cWritten, FALSE);
                    }

                    if (!fSuccess) {
                        PrintLastError(TEXT("WriteFile falhou"));
                        break;
                    }
                }
            }
        }
        else {
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

    if (!InitializeSharedResources(&stateServ)) {
        return -1;
    }

    // Não precisa mais de mapeamento de memória aqui, já foi feito em InitializeSharedResources

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

    CleanupSharedResources(&stateServ);

    return 0;
}