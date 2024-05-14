#ifndef UTILS_H
#define UTILS_H

#include <windows.h>
#include <tchar.h>

#define MSG_TAM 256
#define MAXCLIENTES 10
#define Msg_Size sizeof(Msg)

typedef struct {
    TCHAR msg[MSG_TAM];
} Msg;

typedef struct {
    HANDLE hPipe;
    HANDLE readEvent;
    HANDLE writeEvent;
    int deveContinuar;
    int readerAtivo;
} ClientState;

typedef struct {
    HANDLE clientPipes[MAXCLIENTES]; // Array de handles para os pipes dos clientes
    HANDLE writeReady;               // Handle para o evento de escrita pronta
    HANDLE readEvent;
} ServerState;

void readTChars(TCHAR* p, int maxChars);
void InitializeServerState(ServerState* state);
void PrintLastError(const TCHAR* msg);

#endif // UTILS_H
