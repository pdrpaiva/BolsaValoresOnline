#include <stdio.h>
#include <io.h>
#include <fcntl.h>
#include "../utils.h"

void InitializeServerState(ServerState* state) {
    for (int i = 0; i < MAXCLIENTES; ++i) {
        state->clientPipes[i] = NULL;
        state->readEvents[i] = CreateEvent(NULL, TRUE, FALSE, NULL);
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
            if (state->readEvents[i] != NULL) {
                CloseHandle(state->readEvents[i]);
                state->readEvents[i] = CreateEvent(NULL, TRUE, FALSE, NULL);  // Recreate event for future use
            }
            _tprintf(TEXT("\nClient %d disconnected."), i);
            break;
        }
    }
}

int writeClienteASINC(ServerState* state, HANDLE hPipe, Msg msg) {
    DWORD cbWritten = 0;
    BOOL fSuccess = FALSE;

    OVERLAPPED OverlWr = { 0 };
    OverlWr.hEvent = state->writeReady;

    fSuccess = WriteFile(hPipe, &msg, sizeof(Msg), &cbWritten, &OverlWr);

    if (!fSuccess && GetLastError() != ERROR_IO_PENDING) {
        PrintLastError(TEXT("\nError writing to client"));
        return 0;
    }

    WaitForSingleObject(state->writeReady, INFINITE);
    GetOverlappedResult(hPipe, &OverlWr, &cbWritten, TRUE);
    return (cbWritten == sizeof(Msg));
}

int broadcastClientes(ServerState* state, Msg msg) {
    int numerites = 0;
    for (int i = 0; i < MAXCLIENTES; i++) {
        if (state->clientPipes[i] != NULL) {
            if (writeClienteASINC(state, state->clientPipes[i], msg)) {
                ++numerites;
            }
            else {
                removeCliente(state, state->clientPipes[i]);
                _tprintf(TEXT("\nSuspect client removed."));
            }
        }
    }
    return numerites;
}

DWORD WINAPI InstanceThread(LPVOID lpvParam) {
    ServerState server;
    HANDLE hPipe = (HANDLE)lpvParam;
    Msg msgRequest, msgResponse;
    DWORD bytesRead = 0;
    BOOL fSuccess;
    OVERLAPPED overl = { 0 };
    overl.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

    // Loop de leitura e resposta
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

        // Processa a mensagem recebida
        _tprintf(TEXT("Received message: %s\n"), msgRequest.msg);

        // Simples eco como resposta
        msgResponse = msgRequest;

        if (_tcscmp(msgRequest.msg, TEXT("exit")) == 0) {
            _tprintf(TEXT("Client requested disconnect.\n"));
            removeCliente(&server, hPipe);  // Adicione a chamada aqui
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

int _tmain(void) {
    ServerState server;
    InitializeServerState(&server);
    LPTSTR lpszPipename = TEXT("\\\\.\\pipe\\teste");

    _setmode(_fileno(stdout), _O_WTEXT);
    _setmode(_fileno(stdin), _O_WTEXT);
    _setmode(_fileno(stderr), _O_WTEXT);

    while (1) {
        _tprintf(TEXT("\nServer - main loop - creating named pipe - %s"), lpszPipename);

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
            PrintLastError(TEXT("CreateNamedPipe failed"));
            return -1;
        }

        BOOL fConnected = ConnectNamedPipe(hPipe, NULL) ? TRUE : (GetLastError() == ERROR_PIPE_CONNECTED);
        if (fConnected) {
            _tprintf(TEXT("\nClient connected. Creating a thread for it."));
            HANDLE hThread = CreateThread(
                NULL,
                0,
                InstanceThread,
                (LPVOID)hPipe,
                0,
                NULL
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

    return 0;
}
