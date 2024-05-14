#ifndef UTILS_H
#define UTILS_H

#include <windows.h>
#include <tchar.h>

#define MSGTXTSZ 256
#define MAXCLIENTES 10
#define Msg_Sz sizeof(Msg)

typedef struct {
    TCHAR msg[MSGTXTSZ];
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
    HANDLE readEvents[MAXCLIENTES];  // Event handles for read operations
} ServerState;

void readTChars(TCHAR* p, int maxChars);
void InitializeServerState(ServerState* state);
void PrintLastError(const TCHAR* msg);

#endif // UTILS_H
