// utils.h
#ifndef UTILS_H
#define UTILS_H

#include <windows.h>
#include <tchar.h>

#define MSGTXTSZ 60

typedef struct {
    TCHAR msg[MSGTXTSZ];
} Msg;

#define Msg_Sz sizeof(Msg)

typedef struct {
    HANDLE hPipe;
    HANDLE readEvent;
    HANDLE writeEvent;
    int deveContinuar;
    int readerAtivo;
} ClientState;

void readTChars(TCHAR* p, int maxChars);

#endif // UTILS_H
