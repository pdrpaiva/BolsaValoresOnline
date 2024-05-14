#include "utils.h"
#include <windows.h>
#include <stdio.h>
#include <io.h>
#include <fcntl.h>

void readTChars(TCHAR* p, int maxChars) {
    size_t len;
    _fgetts(p, maxChars, stdin);
    len = _tcslen(p);
    if (p[len - 1] == TEXT('\n')) {
        p[len - 1] = TEXT('\0');
    }
}

DWORD WINAPI ThreadClientReader(LPVOID lparam) {
    ClientState* state = (ClientState*)lparam;
    Msg fromServer;
    DWORD bytesRead = 0;
    BOOL fSuccess = FALSE;
    OVERLAPPED overlRd = { 0 };
    overlRd.hEvent = state->readEvent;

    while (state->deveContinuar) {
        ZeroMemory(&fromServer, sizeof(fromServer));
        ResetEvent(state->readEvent);
        fSuccess = ReadFile(
            state->hPipe,
            &fromServer,
            Msg_Sz,
            &bytesRead,
            &overlRd
        );

        WaitForSingleObject(state->readEvent, INFINITE);
        if (!fSuccess && GetLastError() != ERROR_IO_PENDING) {
            _tprintf(TEXT("\nError reading from pipe.\n"));
            break;
        }

        GetOverlappedResult(state->hPipe, &overlRd, &bytesRead, FALSE);
        if (bytesRead > 0) {
            _tprintf(TEXT("\nReceived message: %s\n"), fromServer.msg);
        }
        else {
            _tprintf(TEXT("\nNo more messages. Exiting.\n"));
            break;
        }
    }

    return 0;
}

int _tmain(int argc, LPTSTR argv[]) {
    ClientState clientState;
    clientState.hPipe = INVALID_HANDLE_VALUE;
    clientState.readEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    clientState.writeEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    clientState.deveContinuar = 1;
    clientState.readerAtivo = 1;

    if (clientState.readEvent == NULL || clientState.writeEvent == NULL) {
        _tprintf(TEXT("Failed to create one or more events.\n"));
        return 1;
    }

    BOOL fSuccess = FALSE;
    DWORD dwMode, cWritten;
    LPTSTR lpszPipename = TEXT("\\\\.\\pipe\\teste");
    Msg MsgToSend;
    HANDLE hThread;
    DWORD dwThreadId = 0;
    OVERLAPPED OverlWr = { 0 };

#ifdef UNICODE
    _setmode(_fileno(stdin), _O_WTEXT);
    _setmode(_fileno(stdout), _O_WTEXT);
    _setmode(_fileno(stderr), _O_WTEXT);
#endif

    while (1) {
        clientState.hPipe = CreateFile(
            lpszPipename,
            GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            NULL,
            OPEN_EXISTING,
            FILE_FLAG_OVERLAPPED,
            NULL
        );

        if (clientState.hPipe != INVALID_HANDLE_VALUE)
            break;

        if (GetLastError() != ERROR_PIPE_BUSY) {
            _tprintf(TEXT("Error: Pipe not available. GLE=%d\n"), GetLastError());
            return -1;
        }

        if (!WaitNamedPipe(lpszPipename, 3000)) {
            _tprintf(TEXT("Waited for 3 seconds, giving up.\n"));
            return -1;
        }
    }

    dwMode = PIPE_READMODE_MESSAGE;
    fSuccess = SetNamedPipeHandleState(
        clientState.hPipe,
        &dwMode,
        NULL,
        NULL
    );

    if (!fSuccess) {
        _tprintf(TEXT("SetNamedPipeHandleState failed. GLE=%d\n"), GetLastError());
        return -1;
    }

    hThread = CreateThread(
        NULL,
        0,
        ThreadClientReader,
        &clientState,
        0,
        &dwThreadId
    );

    if (hThread == NULL) {
        _tprintf(TEXT("Failed to create the reader thread. GLE=%d\n"), GetLastError());
        return -1;
    }

    _tprintf(TEXT("Connection established: Type 'exit' to quit.\n"));
    while (clientState.deveContinuar) {
        readTChars(MsgToSend.msg, MSGTXTSZ);
        if (_tcscmp(MsgToSend.msg, TEXT("exit")) == 0) {
            _tprintf(TEXT("Exiting...\n"));
            clientState.deveContinuar = 0;
            CloseHandle(clientState.hPipe);
            break;
        }

        ZeroMemory(&OverlWr, sizeof(OverlWr));
        OverlWr.hEvent = clientState.writeEvent;
        ResetEvent(clientState.writeEvent);

        fSuccess = WriteFile(
            clientState.hPipe,
            &MsgToSend,
            Msg_Sz,
            &cWritten,
            &OverlWr
        );

        WaitForSingleObject(clientState.writeEvent, INFINITE);
        if (!GetOverlappedResult(clientState.hPipe, &OverlWr, &cWritten, FALSE) || cWritten < Msg_Sz) {
            _tprintf(TEXT("WriteFile error or incomplete write. GLE=%d\n"), GetLastError());
        }
        else {
            _tprintf(TEXT("Message sent: '%s'\n"), MsgToSend.msg);
        }
    }

    WaitForSingleObject(hThread, INFINITE);

    CloseHandle(hThread);
    CloseHandle(clientState.readEvent);
    CloseHandle(clientState.writeEvent);
    CloseHandle(clientState.hPipe);

    return 0;
}
