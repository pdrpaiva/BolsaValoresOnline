#include "utils.h"
#include <stdio.h>
#include <io.h>
#include <fcntl.h>

void InitializeServerState(ServerState* state) {
    for (int i = 0; i < MAXCLIENTES; ++i) {
        state->clientPipes[i] = NULL;
        state->readEvents[i] = CreateEvent(NULL, TRUE, FALSE, NULL); // Ensure events are created for each client
    }
    state->writeReady = CreateEvent(NULL, TRUE, FALSE, NULL);
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

void adicionaCliente(ServerState* state, HANDLE cli) {
    for (int i = 0; i < MAXCLIENTES; ++i) {
        if (state->clientPipes[i] == NULL) {
            state->clientPipes[i] = cli;
            break;
        }
    }
}

void removeCliente(ServerState* state, HANDLE cli) {
    for (int i = 0; i < MAXCLIENTES; ++i) {
        if (state->clientPipes[i] == cli) {
            CloseHandle(state->clientPipes[i]);
            state->clientPipes[i] = NULL;
            CloseHandle(state->readEvents[i]); // Close the associated read event
            state->readEvents[i] = NULL;
            break;
        }
    }
}

int writeClienteASINC(ServerState* state, HANDLE hPipe, AppMessage msg) {
    DWORD cbWritten = 0;
    BOOL fSuccess = FALSE;

    OVERLAPPED OverlWr = { 0 };
    OverlWr.hEvent = state->writeReady;

    fSuccess = WriteFile(hPipe, &msg, sizeof(AppMessage), &cbWritten, &OverlWr);

    if (!fSuccess) {
        PrintLastError(TEXT("\nOcorreu algo na escrita para 1 cliente"));
        return 0;
    }

    WaitForSingleObject(state->writeReady, INFINITE);
    GetOverlappedResult(hPipe, &OverlWr, &cbWritten, TRUE);
    return (cbWritten == sizeof(AppMessage));
}

int broadcastClientes(ServerState* state, AppMessage msg) {
    int numerites = 0;
    for (int i = 0; i < MAXCLIENTES; i++) {
        if (state->clientPipes[i] != NULL) {
            if (writeClienteASINC(state, state->clientPipes[i], msg)) {
                ++numerites;
            }
            else {
                removeCliente(state, state->clientPipes[i]);
                _tprintf(TEXT("\nCliente suspeito removido."));
            }
        }
    }
    return numerites;
}

DWORD WINAPI InstanceThread(LPVOID lpvParam) {
    ServerState* serverState = (ServerState*)lpvParam;  // Cast do parâmetro para o tipo ServerState
    int clientIndex = -1;

    // Encontrar o índice do cliente baseado no HANDLE passado
    for (int i = 0; i < MAXCLIENTES; i++) {
        if (serverState->clientPipes[i] != NULL) {
            clientIndex = i;
            break;
        }
    }

    if (clientIndex == -1) {
        _tprintf(TEXT("Error: Client index not found.\n"));
        return 1; // Encerra a thread se o índice do cliente não for encontrado
    }

    HANDLE hPipe = serverState->clientPipes[clientIndex];
    HANDLE readEvent = serverState->readEvents[clientIndex];
    AppMessage msgRequest, msgResponse;
    DWORD bytesRead = 0;
    BOOL fSuccess;
    OVERLAPPED overl = { 0 };
    overl.hEvent = readEvent;

    // Loop de leitura e resposta
    while (1) {
        ZeroMemory(&msgRequest, sizeof(AppMessage));
        fSuccess = ReadFile(hPipe, &msgRequest, sizeof(AppMessage), &bytesRead, &overl);

        if (!fSuccess) {
            if (GetLastError() == ERROR_IO_PENDING) {
                WaitForSingleObject(readEvent, INFINITE);
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
            else {
                PrintLastError(TEXT("ReadFile failed"));
                break;
            }
        }

        // Processa a mensagem recebida
        _tprintf(TEXT("Received message: %s\n"), msgRequest.msg);

        // Simples eco como resposta
        msgResponse = msgRequest;

        // Broadcast da resposta para todos os clientes
        int numResponses = broadcastClientes(serverState, msgResponse);
        _tprintf(TEXT("Broadcast responses sent to %d clients.\n"), numResponses);
    }

    // Limpeza após o cliente se desconectar
    removeCliente(serverState, hPipe);
    DisconnectNamedPipe(hPipe);
    CloseHandle(hPipe);

    return 0;
}

int _tmain(void) {
    ServerState server;
    InitializeServerState(&server);
    LPTSTR lpszPipename = TEXT("\\\\.\\pipe\\MyPipe");

    _setmode(_fileno(stdout), _O_WTEXT);
    _setmode(_fileno(stdin), _O_WTEXT);
    _setmode(_fileno(stderr), _O_WTEXT);

    while (1) {
        _tprintf(TEXT("\nServidor - ciclo principal - criando named pipe - %s"), lpszPipename);

        HANDLE hPipe = CreateNamedPipe(
            lpszPipename,
            PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
            PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
            PIPE_UNLIMITED_INSTANCES,
            BUFSIZ,
            BUFSIZ,
            5000,
            NULL
        );

        if (hPipe == INVALID_HANDLE_VALUE) {
            PrintLastError(TEXT("CREATEnamepipe falhou"));
            return -1;
        }

        BOOL fConnected = ConnectNamedPipe(hPipe, NULL) ? TRUE : (GetLastError() == ERROR_PIPE_CONNECTED);
        if (fConnected) {
            _tprintf(TEXT("\nCliente conectado. Criando uma thread para ele."));
            HANDLE hThread = CreateThread(
                NULL,
                0,
                InstanceThread,
                (LPVOID)&server,  // Passando a estrutura do servidor para a thread
                0,
                NULL
            );

            if (hThread == NULL) {
                PrintLastError(TEXT("Erro na criação da thread"));
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

    return 0;
}


