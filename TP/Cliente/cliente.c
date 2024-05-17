#include "../utils.h"
#include <windows.h>
#include <stdio.h>
#include <io.h>
#include <fcntl.h>

void readTCharsWithTimeout(TCHAR* p, int maxChars, HANDLE shutdownEvent) {
    _tprintf(TEXT("\n>> "));
    size_t len = 0;
    while (TRUE) {
        DWORD result = WaitForSingleObject(shutdownEvent, 100);
        if (result == WAIT_OBJECT_0) {
            return;
        }
        if (_kbhit()) {
            _fgetts(p, maxChars, stdin);
            len = _tcslen(p);
            if (p[len - 1] == TEXT('\n')) {
                p[len - 1] = TEXT('\0');
            }
            return;
        }
    }
}

DWORD WINAPI ThreadClientReader(LPVOID lparam) {
    ClientState* stateCli = (ClientState*)lparam;
    Msg fromServer;
    DWORD bytesRead = 0;
    BOOL fSuccess = FALSE;
    OVERLAPPED overlRd = { 0 };
    overlRd.hEvent = stateCli->readEvent;

    while (1) {
        ZeroMemory(&fromServer, sizeof(fromServer));
        ResetEvent(stateCli->readEvent);
        fSuccess = ReadFile(stateCli->hPipe, &fromServer, Msg_Size, &bytesRead, &overlRd);

        if (!fSuccess && GetLastError() == ERROR_IO_PENDING) {
            WaitForSingleObject(stateCli->readEvent, INFINITE);
            fSuccess = GetOverlappedResult(stateCli->hPipe, &overlRd, &bytesRead, FALSE);
        }

        if (fSuccess && bytesRead > 0) {
            _tprintf(TEXT("\nReceived message: %s\n"), fromServer.msg);

            if (_tcscmp(fromServer.msg, TEXT("login_success")) == 0) {
                stateCli->ligado = TRUE;
                _tprintf(TEXT("Login bem-sucedido. Cliente agora está ligado.\n"));
            }
            else if (_tcscmp(fromServer.msg, TEXT("Bolsa encerrada.")) == 0) {
                _tprintf(TEXT("Servidor está encerrando. Cliente irá desconectar...\n"));
                SetEvent(stateCli->shutdownEvent);
                break;
            }
            else if (_tcsncmp(fromServer.msg, TEXT("Empresas na Bolsa:"), 17) == 0) {
                _tprintf(TEXT("%s\n"), fromServer.msg);
            }
        }
        else {
            if (GetLastError() != ERROR_IO_PENDING && GetLastError() != ERROR_BROKEN_PIPE) {
                _tprintf(TEXT("\nError reading from pipe.\n"));
                break;
            }
            else if (GetLastError() == ERROR_BROKEN_PIPE) {
                break;
            }
        }
    }

    return 0;
}

void ClientCommands(ClientState* stateCli, TCHAR* command) {
    if (_tcsncmp(command, TEXT("login"), 5) == 0) {
        TCHAR username[50];
        TCHAR password[50];
        if (_stscanf_s(command, TEXT("login %49s %49s"), username, (unsigned)_countof(username), password, (unsigned)_countof(password)) == 2) {
            Msg MsgToSend;
            _stprintf_s(MsgToSend.msg, MSG_TAM, TEXT("login %s %s"), username, password);
            _tcscpy_s(stateCli->username, _countof(stateCli->username), username);

            DWORD cWritten;
            OVERLAPPED overl = { 0 };
            overl.hEvent = stateCli->writeEvent;
            ResetEvent(stateCli->writeEvent);

            WriteFile(stateCli->hPipe, &MsgToSend, Msg_Size, &cWritten, &overl);

            if (GetLastError() == ERROR_IO_PENDING) {
                WaitForSingleObject(stateCli->writeEvent, INFINITE);
                GetOverlappedResult(stateCli->hPipe, &overl, &cWritten, FALSE);
            }

            _tprintf(TEXT("Tentando login para o usuário: %s\n"), username);
        }
        else {
            _tprintf(TEXT("Erro no login. Utilize: login <username> <password>\n"));
        }
    }
    else if (stateCli->ligado) {
        if (_tcscmp(command, TEXT("comandos")) == 0) {
            PrintMenuCliente();
        }
        else if (_tcscmp(command, TEXT("listc")) == 0) {
            Msg MsgtoSend;
            _tcscpy_s(MsgtoSend.msg, MSG_TAM, TEXT("listc"));

            DWORD cWritten;
            OVERLAPPED overl = { 0 };
            overl.hEvent = stateCli->writeEvent;
            ResetEvent(stateCli->writeEvent);

            WriteFile(stateCli->hPipe, &MsgtoSend, Msg_Size, &cWritten, &overl);

            if (GetLastError() == ERROR_IO_PENDING) {
                WaitForSingleObject(stateCli->writeEvent, INFINITE);
                GetOverlappedResult(stateCli->hPipe, &overl, &cWritten, FALSE);
            }
            _tprintf(TEXT("Comando listc enviado.\n"));
        }
        else if (_tcsncmp(command, TEXT("buy"), 3) == 0) {
            TCHAR nomeEmpresa[50];
            int nAcoes;
            if (_stscanf_s(command, TEXT("buy %49s %d"), nomeEmpresa, (unsigned)_countof(nomeEmpresa), &nAcoes) == 2) {
                Msg MsgToSend;
                _stprintf_s(MsgToSend.msg, MSG_TAM, TEXT("buy %s %d %s"), nomeEmpresa, nAcoes, stateCli->username);

                DWORD cWritten;
                OVERLAPPED overl = { 0 };
                overl.hEvent = stateCli->writeEvent;
                ResetEvent(stateCli->writeEvent);

                WriteFile(stateCli->hPipe, &MsgToSend, Msg_Size, &cWritten, &overl);

                if (GetLastError() == ERROR_IO_PENDING) {
                    WaitForSingleObject(stateCli->writeEvent, INFINITE);
                    GetOverlappedResult(stateCli->hPipe, &overl, &cWritten, FALSE);
                }
                _tprintf(TEXT("Comando buy enviado.\n"));
            }
            else {
                _tprintf(TEXT("Erro no comando buy. Utilize: buy <nome-empresa> <número-ações>\n"));
            }
        }
        else if (_tcsncmp(command, TEXT("sell"), 4) == 0) {
            TCHAR nomeEmpresa[50];
            int nAcoes;
            if (_stscanf_s(command, TEXT("sell %49s %d"), nomeEmpresa, (unsigned)_countof(nomeEmpresa), &nAcoes) == 2) {
                Msg MsgToSend;
                _stprintf_s(MsgToSend.msg, MSG_TAM, TEXT("sell %s %d %s"), nomeEmpresa, nAcoes, stateCli->username);

                DWORD cWritten;
                OVERLAPPED overl = { 0 };
                overl.hEvent = stateCli->writeEvent;
                ResetEvent(stateCli->writeEvent);

                WriteFile(stateCli->hPipe, &MsgToSend, Msg_Size, &cWritten, &overl);

                if (GetLastError() == ERROR_IO_PENDING) {
                    WaitForSingleObject(stateCli->writeEvent, INFINITE);
                    GetOverlappedResult(stateCli->hPipe, &overl, &cWritten, FALSE);
                }
                _tprintf(TEXT("Comando sell enviado.\n"));
            }
            else {
                _tprintf(TEXT("Erro no comando sell. Utilize: sell <nome-empresa> <número-ações>\n"));
            }
        }
        else if (_tcscmp(command, TEXT("balance")) == 0) {
            Msg MsgToSend;
            _stprintf_s(MsgToSend.msg, MSG_TAM, TEXT("balance %s"),stateCli->username);

            DWORD cWritten;
            OVERLAPPED overl = { 0 };
            overl.hEvent = stateCli->writeEvent;
            ResetEvent(stateCli->writeEvent);

            WriteFile(stateCli->hPipe, &MsgToSend, Msg_Size, &cWritten, &overl);

            if (GetLastError() == ERROR_IO_PENDING) {
                WaitForSingleObject(stateCli->writeEvent, INFINITE);
                GetOverlappedResult(stateCli->hPipe, &overl, &cWritten, FALSE);
            }
            _tprintf(TEXT("Comando balance enviado.\n"));
        }
        else {
            _tprintf(TEXT("Comando inválido. Tente novamente.\n"));
        }
    }
    else {
        _tprintf(TEXT("Comando inválido. Por favor, faça login para ter acesso a todos os comandos.\n"));
    }

    if (_tcscmp(command, TEXT("exit")) == 0) {
        _tprintf(TEXT("Exiting...\n"));
        SetEvent(stateCli->shutdownEvent);
        CloseClientPipe(stateCli);
    }
}

void PrintMenuCliente() {
    _tprintf(TEXT("\n-------------------------- Comandos do Cliente --------------------------\n\n"));
    _tprintf(TEXT("- Listar todas as empresas  -   listc\n"));
    _tprintf(TEXT("- Comprar ações             -   buy <nome-empresa> <número-ações>\n"));
    _tprintf(TEXT("- Vender ações              -   sell <nome-empresa> <número-ações>\n"));
    _tprintf(TEXT("- Consultar saldo           -   balance\n"));
    _tprintf(TEXT("- Sair da aplicação         -   exit\n"));
    _tprintf(TEXT("\n-------------------------------------------------------------------------\n"));
}

void CloseClientPipe(ClientState* stateCli) {
    if (stateCli->hPipe != INVALID_HANDLE_VALUE) {
        CloseHandle(stateCli->hPipe);
        stateCli->hPipe = INVALID_HANDLE_VALUE;
    }
}

int _tmain(int argc, LPTSTR argv[]) {
    ClientState stateCli;
    stateCli.hPipe = INVALID_HANDLE_VALUE;
    stateCli.readEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    stateCli.writeEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    stateCli.shutdownEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    stateCli.deveContinuar = TRUE;
    stateCli.readerAtivo = TRUE;
    stateCli.ligado = FALSE;
    Msg MsgToSend;

    if (stateCli.readEvent == NULL || stateCli.writeEvent == NULL || stateCli.shutdownEvent == NULL) {
        _tprintf(TEXT("Failed to create one or more events.\n"));
        return 1;
    }

    BOOL fSuccess = FALSE;
    DWORD dwMode;
    LPTSTR lpszPipename = TEXT("\\\\.\\pipe\\teste");
    HANDLE hThread;
    DWORD dwThreadId = 0;

#ifdef UNICODE
    _setmode(_fileno(stdin), _O_WTEXT);
    _setmode(_fileno(stdout), _O_WTEXT);
    _setmode(_fileno(stderr), _O_WTEXT);
#endif

    while (1) {
        stateCli.hPipe = CreateFile(lpszPipename, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);

        if (stateCli.hPipe != INVALID_HANDLE_VALUE)
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
    fSuccess = SetNamedPipeHandleState(stateCli.hPipe, &dwMode, NULL, NULL);

    if (!fSuccess) {
        _tprintf(TEXT("SetNamedPipeHandleState failed. GLE=%d\n"), GetLastError());
        return -1;
    }

    hThread = CreateThread(NULL, 0, ThreadClientReader, &stateCli, 0, &dwThreadId);

    if (hThread == NULL) {
        _tprintf(TEXT("Failed to create the reader thread. GLE=%%d\n"));
        return -1;
    }

    _tprintf(TEXT("\nBem-vindo! Faça 'login' para começar ou 'exit' para sair...\n"));

    HANDLE events[] = { stateCli.shutdownEvent };

    while (1) {
        if (WaitForSingleObject(stateCli.shutdownEvent, 0) == WAIT_OBJECT_0) {
            break;
        }

        readTCharsWithTimeout(MsgToSend.msg, MSG_TAM, stateCli.shutdownEvent);

        if (_tcscmp(MsgToSend.msg, TEXT("exit")) == 0) {
            _tprintf(TEXT("Exiting...\n"));
            stateCli.deveContinuar = FALSE;
            SetEvent(stateCli.shutdownEvent);
            CloseClientPipe(&stateCli);
            break;
        }

        ClientCommands(&stateCli, MsgToSend.msg);
    }

    SetEvent(stateCli.shutdownEvent);
    WaitForSingleObject(hThread, INFINITE);
    CloseHandle(hThread);

    CloseHandle(stateCli.readEvent);
    CloseHandle(stateCli.writeEvent);
    CloseHandle(stateCli.shutdownEvent);
    CloseClientPipe(&stateCli);

    return 0;
}