#include <stdio.h>
#include <io.h>
#include <fcntl.h>
#include "../utils.h"

void InitializeServerState(ServerState* stateServ) {
    // Inicializa o estado do servidor
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
    _tprintf(TEXT("\n--- Comandos da Bolsa ---\n\n"));
    _tprintf(TEXT("addc <nome-empresa> <n�mero-a��es> <pre�o-a��o> - Adicionar empresa\n"));
    _tprintf(TEXT("listc - Listar todas as empresas\n"));
    _tprintf(TEXT("stock <nome-empresa> <novo-pre�o> - Alterar pre�o das a��es\n"));
    _tprintf(TEXT("users - Listar utilizadores\n"));
    _tprintf(TEXT("pause <segundos> - Pausar opera��es\n"));
    _tprintf(TEXT("close - Encerrar sistema\n"));
    _tprintf(TEXT("Digite um comando:\n"));
}


void PrintLastError(const TCHAR* msg) {
    // Declara��o de vari�veis para armazenar informa��es sobre o erro
    DWORD eNum;
    TCHAR sysMsg[256];
    TCHAR* p;

    // Obt�m o c�digo de erro do �ltimo erro ocorrido
    eNum = GetLastError();

    // Formata a mensagem de erro correspondente ao c�digo de erro
    FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL, eNum,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        sysMsg, 256, NULL);

    // Remove caracteres de formata��o desnecess�rios da mensagem de erro
    p = sysMsg;
    while ((*p > 31) || (*p == 9))
        ++p;
    do { *p-- = 0; } while ((p >= sysMsg) && ((*p == '.') || (*p < 33)));

    _tprintf(TEXT("\n%s: %s (Error %d)\n"), msg, sysMsg, eNum);
}

void adicionaCliente(ServerState* stateServ, HANDLE hCli) {
    // Adiciona o handle do pipe do cliente ao array de handles dos pipes dos clientes
    for (int i = 0; i < MAXCLIENTES; ++i) {
        if (stateServ->clientPipes[i] == NULL) {
            stateServ->clientPipes[i] = hCli;
            break;
        }
    }
}

void removeCliente(ServerState* stateServ, HANDLE hCli) {
    // Remove o handle do pipe do cliente do array de handles dos pipes dos clientes
    for (int i = 0; i < MAXCLIENTES; ++i) {
        if (stateServ->clientPipes[i] == hCli) {
            // Fecha o handle do pipe do cliente
            CloseHandle(stateServ->clientPipes[i]);
            // Define NULL na posi��o correspondente do array
            stateServ->clientPipes[i] = NULL;
            // Se o evento de leitura existir, fecha e recria
            if (stateServ->readEvent != NULL) {
                CloseHandle(stateServ->readEvent);
                stateServ->readEvent = CreateEvent(NULL, TRUE, FALSE, NULL);  // Recreate event for future use
            }
            // Imprime uma mensagem indicando que o cliente foi desconectado
            _tprintf(TEXT("\nClient %d disconnected."), i);
            break;
        }
    }
}
/*
int writeClienteASINC(ServerState* stateServ, HANDLE hPipe, Msg msg) {
    DWORD cbWritten = 0;
    BOOL fSuccess = FALSE;

    OVERLAPPED OverlWr = { 0 };
    OverlWr.hEvent = stateServ->writeReady;

    fSuccess = WriteFile(hPipe, &msg, sizeof(Msg), &cbWritten, &OverlWr);

    if (!fSuccess && GetLastError() != ERROR_IO_PENDING) {
        PrintLastError(TEXT("\nError writing to client"));
        return 0;
    }

    WaitForSingleObject(stateServ->writeReady, INFINITE);
    GetOverlappedResult(hPipe, &OverlWr, &cbWritten, TRUE);
    return (cbWritten == sizeof(Msg));
}

int broadcastClientes(ServerState* stateServ, Msg msg) {
    int numerites = 0;
    for (int i = 0; i < MAXCLIENTES; i++) {
        if (stateServ->clientPipes[i] != NULL) {
            if (writeClienteASINC(stateServ, stateServ->clientPipes[i], msg)) {
                ++numerites;
            }
            else {
                removeCliente(stateServ, stateServ->clientPipes[i]);
                _tprintf(TEXT("\nSuspect client removed."));
            }
        }
    }
    return numerites;
}
*/

void ProcessAdminCommand(ServerState* stateServ, TCHAR* command) {
    if (_tcsncmp(command, TEXT("addc"), 4) == 0) {
        TCHAR nomeEmpresa[50];
        int numAcoes;
        double precoAcao;
        if (_stscanf_s(command, TEXT("addc %s %d %lf"), nomeEmpresa, &numAcoes, &precoAcao) == 3) {
            TCHAR response[MSG_TAM];
            //AddCompany(stateServ, nomeEmpresa, numAcoes, precoAcao, response);
            //_tprintf(TEXT("%s\n"), response);
        }
    }
    else if (_tcscmp(command, TEXT("listc")) == 0) {
        TCHAR response[MSG_TAM];
        //ListCompanies(stateServ, response);
        //_tprintf(TEXT("%s\n"), response);
        _tprintf(TEXT("goiabao"));
    }
    else if (_tcsncmp(command, TEXT("stock"), 5) == 0) {
        TCHAR nomeEmpresa[50];
        double newPrice;
        if (_stscanf_s(command, TEXT("stock %s %lf"), nomeEmpresa, &newPrice) == 2) {
            TCHAR response[MSG_TAM];
            //SetStockPrice(stateServ, nomeEmpresa, newPrice, response);
            //_tprintf(TEXT("%s\n"), response);
        }
    }
    else if (_tcscmp(command, TEXT("users")) == 0) {
        TCHAR response[MSG_TAM];
        //ListUsers(stateServ, response);
        //_tprintf(TEXT("%s\n"), response);
    }
    else if (_tcsncmp(command, TEXT("pause"), 5) == 0) {
        int duration;
        if (_stscanf_s(command, TEXT("pause %d"), &duration) == 1) {
            TCHAR response[MSG_TAM];
            //PauseTrading(stateServ, duration, response);
            //_tprintf(TEXT("%s\n"), response);
        }
    }
    else if (_tcscmp(command, TEXT("close")) == 0) {
        TCHAR response[MSG_TAM];
        //CloseSystem(stateServ, response);
        //_tprintf(TEXT("%s\n"), response);
        ExitProcess(0); // Sai do programa completamente
    }
    else {
        _tprintf(TEXT("Comando inv�lido. Tente novamente.\n"));
    }
}

DWORD WINAPI InstanceThread(LPVOID lpvParam) {
    // Declara��o das vari�veis locais
    ServerState stateServ;
    HANDLE hPipe = (HANDLE)lpvParam;
    Msg msgRequest, msgResponse;
    DWORD bytesRead = 0;
    BOOL fSuccess;
    OVERLAPPED overl = { 0 };
    overl.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

    // Loop de leitura e resposta
    while (1) {
        // Zera a mem�ria da mensagem de requisi��o
        ZeroMemory(&msgRequest, sizeof(Msg));
        // Reseta o evento OVERLAPPED
        ResetEvent(overl.hEvent);
        // L� uma mensagem do pipe de entrada
        fSuccess = ReadFile(hPipe, &msgRequest, sizeof(Msg), &bytesRead, &overl);

        // Verifica se a leitura foi bem-sucedida
        if (!fSuccess && GetLastError() == ERROR_IO_PENDING) {
            // Aguarda o t�rmino da opera��o ass�ncrona
            WaitForSingleObject(overl.hEvent, INFINITE);
            // Verifica se a opera��o foi conclu�da com sucesso
            if (!GetOverlappedResult(hPipe, &overl, &bytesRead, FALSE) || bytesRead == 0) {
                // Verifica se o cliente desconectou
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

        // Verifica se o cliente solicitou a desconex�o
        if (_tcscmp(msgRequest.msg, TEXT("exit")) == 0) {
            _tprintf(TEXT("Client requested disconnect.\n"));
            // Remove o cliente do estado do servidor
            removeCliente(&stateServ, hPipe); 
            break;
        }

        // Inicia uma opera��o de escrita ass�ncrona para enviar a resposta ao cliente
        fSuccess = WriteFile(hPipe, &msgResponse, sizeof(Msg), NULL, &overl);
        if (!fSuccess && GetLastError() == ERROR_IO_PENDING) {
            // Aguarda o t�rmino da opera��o ass�ncrona
            WaitForSingleObject(overl.hEvent, INFINITE);
            // Verifica se a opera��o foi conclu�da com sucesso
            if (!GetOverlappedResult(hPipe, &overl, NULL, FALSE)) {
                PrintLastError(TEXT("WriteFile failed"));
            }
        }
    }

    // Fecha o handle do evento OVERLAPPED
    CloseHandle(overl.hEvent);
    // Desconecta o pipe
    DisconnectNamedPipe(hPipe);
    // Fecha o handle do pipe
    CloseHandle(hPipe);

    return 0;
}

int _tmain(void) {
    // Declara��o das vari�veis locais
    ServerState stateServ;

    // Inicializa o estado do servidor
    InitializeServerState(&stateServ);

    PrintMenu();

    _setmode(_fileno(stdout), _O_WTEXT);
    _setmode(_fileno(stdin), _O_WTEXT);
    _setmode(_fileno(stderr), _O_WTEXT);

    HANDLE hPipe, hThread;
    //nome do pipe
    LPTSTR lpszPipename = TEXT("\\\\.\\pipe\\teste");
    DWORD threadID;
    BOOL fConnected;
    TCHAR command[256];

    // Loop principal do servidor
    while (1) {
        _tprintf(TEXT("\nServer - main loop - creating named pipe - %s"), lpszPipename);

        // Cria o named pipe
        hPipe = CreateNamedPipe(
            lpszPipename,
            PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,  // Modo de acesso e flags
            PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,  // Tipo de pipe e modo de leitura
            PIPE_UNLIMITED_INSTANCES,  // N�mero m�ximo de inst�ncias
            BUFSIZ, BUFSIZ,  // Tamanho do buffer de entrada e sa�da
            5000,  // Tempo de espera para conex�o (5 segundos)
            NULL  // Par�metros de seguran�a
        );

        // Verifica se a cria��o do pipe foi bem-sucedida
        if (hPipe == INVALID_HANDLE_VALUE) {
            PrintLastError(TEXT("CreateNamedPipe failed"));
            return -1;
        }

        // Verifica se j� existe uma conex�o pendente
        fConnected = ConnectNamedPipe(hPipe, NULL) ? TRUE : (GetLastError() == ERROR_PIPE_CONNECTED);
        
        // Se houver uma conex�o pendente, cria uma nova thread para lidar com ela
        if (fConnected) {
            _tprintf(TEXT("\nClient connected. Creating a thread for it."));
            
            hThread = CreateThread(
                NULL, 0,  // Atributos de seguran�a e tamanho da pilha (padr�o)
                InstanceThread,  // Fun��o da thread
                (LPVOID)hPipe,  // Par�metro da thread (handle do pipe)
                0, NULL  // Flags de cria��o e identificador da thread (padr�o)
            );

            // Verifica se a cria��o da thread foi bem-sucedida
            if (hThread == NULL) {
                PrintLastError(TEXT("Thread creation failed"));
                return -1;
            }
            else {
                // Fecha o handle da thread
                CloseHandle(hThread);
            }
        }
        else {
            // Se n�o houver conex�o pendente, fecha o handle do pipe
            CloseHandle(hPipe);
        }

        // Processa um comando administrativo, se dispon�vel
        _tprintf(TEXT("\nDigite um comando administrativo: "));
        readTChars(command, 256);
        if (_tcslen(command) > 0) {  // Verifica se algum comando foi digitado
            ProcessAdminCommand(&stateServ, command);
        }
    }

    return 0;
}
