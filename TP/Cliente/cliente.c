#include "../utils.h"
#include <windows.h>
#include <stdio.h>
#include <io.h>
#include <fcntl.h>

//void readTChars(TCHAR* p, int maxChars) {
//    size_t len;
//    _fgetts(p, maxChars, stdin);
//    len = _tcslen(p);
//    if (p[len - 1] == TEXT('\n')) {
//        p[len - 1] = TEXT('\0');
//    }
//}

DWORD WINAPI ThreadClientReader(LPVOID lparam) {
    // Casting do parâmetro para um ponteiro para ClientState
    ClientState* stateCli = (ClientState*)lparam;

    // Variável para armazenar a mensagem recebida do servidor
    Msg fromServer;

    // Variáveis para acompanhar o número de bytes lidos e o sucesso da operação de leitura
    DWORD bytesRead = 0;
    BOOL fSuccess = FALSE;

    // Estrutura de dados para operações assíncronas de I/O
    OVERLAPPED overlRd = { 0 };

    // Configura o evento associado à operação de leitura assíncrona
    overlRd.hEvent = stateCli->readEvent;

    // Loop principal da thread
    while (stateCli->deveContinuar) {
        // Limpa a estrutura da mensagem recebida
        ZeroMemory(&fromServer, sizeof(fromServer));

        // Reinicia o evento associado à operação de leitura
        ResetEvent(stateCli->readEvent);

        // Inicia a operação de leitura assíncrona
        fSuccess = ReadFile(
            stateCli->hPipe,
            &fromServer,
            Msg_Size,
            &bytesRead,
            &overlRd
        );

        // Aguarda até que o evento associado à operação de leitura seja sinalizado
        WaitForSingleObject(stateCli->readEvent, INFINITE);

        // Verifica se a operação de leitura foi bem-sucedida ou está pendente
        if (!fSuccess && GetLastError() != ERROR_IO_PENDING) {
            // Imprime uma mensagem de erro se a operação de leitura falhar
            _tprintf(TEXT("\nError reading from pipe.\n"));
            break; // Sai do loop se houver um erro
        }

        // Obtém o resultado da operação de leitura assíncrona
        GetOverlappedResult(stateCli->hPipe, &overlRd, &bytesRead, FALSE);

        // Verifica se foram lidos bytes da mensagem
        if (bytesRead > 0) {
            // Imprime a mensagem recebida do servidor
            //_tprintf(TEXT("\nReceived message: %s\n"), fromServer.msg);
        }
        else {
            // Imprime uma mensagem se não houver mais mensagens para ler
            _tprintf(TEXT("\nNo more messages. Exiting.\n"));
            break; // Sai do loop se não houver mais mensagens
        }
    }

    // Retorna 0 para indicar o término da thread
    return 0;
}

void ClientCommands(ClientState* stateCli, TCHAR* command) {
    if (stateCli->ligado) {
        if (_tcscmp(command, TEXT("comandos")) == 0) {
            PrintMenuCliente();
        }
        else if (_tcscmp(command, TEXT("listc")) == 0) {
            //TCHAR response[MSG_TAM];
            //ListCompanies(stateServ, response);
            _tprintf(TEXT("listc\n"));
        }
        else if (_tcsncmp(command, TEXT("buy"), 3) == 0) {
            TCHAR nomeEmpresa[50];
            int nAcoes;
            if (_stscanf_s(command, TEXT("buy %49s %d"), nomeEmpresa, (unsigned)_countof(nomeEmpresa), &nAcoes) == 2) {
                _tprintf(TEXT("buy\n"));
            }
        }
        else if (_tcscmp(command, TEXT("sell"), 4) == 0) {
            TCHAR nomeEmpresa[50];
            int nAcoes;
            if (_stscanf_s(command, TEXT("sell %49s %d"), nomeEmpresa, (unsigned)_countof(nomeEmpresa), &nAcoes) == 2) {
                _tprintf(TEXT("sell\n"));
            }
        }
        else if (_tcscmp(command, TEXT("balance")) == 0) {
            //TCHAR response[MSG_TAM];
            //ListCompanies(stateServ, response);
            _tprintf(TEXT("balance\n"));
        }
        else {
            _tprintf(TEXT("Comando inválido. Tente novamente.\n"));
        }
    }
    else {
        if (_tcsncmp(command, TEXT("login"), 5) == 0) {
            TCHAR username[50];
            TCHAR password[50];
            if (_stscanf_s(command, TEXT("login %49s %49s"), username, (unsigned)_countof(username), password, (unsigned)_countof(password)) == 2) {
                _tprintf(TEXT("Login realizado com sucesso.\n"));
                stateCli->ligado = TRUE;
                PrintMenuCliente();
            }
            else {
                _tprintf(TEXT("Erro no login. login <username > <password>\n"));
            }
        }
        else {
            _tprintf(TEXT("Comando inválido. Por favor, faça login para ter acesso a todos os comandos.\n"));
        }
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

int _tmain(int argc, LPTSTR argv[]) {
    // Declara e inicializa o estado do cliente
    ClientState stateCli;
    stateCli.hPipe = INVALID_HANDLE_VALUE;
    stateCli.readEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    stateCli.writeEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    stateCli.deveContinuar = TRUE;
    stateCli.readerAtivo = TRUE;
    stateCli.ligado = FALSE;

    // Verifica se a criação dos eventos foi bem-sucedida
    if (stateCli.readEvent == NULL || stateCli.writeEvent == NULL) {
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

    // Loop principal do cliente
    while (1) {
        // Tenta se conectar ao pipe nomeado do servidor
        stateCli.hPipe = CreateFile(
            lpszPipename,                               // Nome do pipe
            GENERIC_READ | GENERIC_WRITE,               // Direitos de acesso
            FILE_SHARE_READ | FILE_SHARE_WRITE,         // Compartilhamento de arquivo
            NULL,                                       // Atributos de segurança (padrão)
            OPEN_EXISTING,                              // Abre o pipe existente
            FILE_FLAG_OVERLAPPED,                       // Habilita operações assíncronas
            NULL                                        // Modo de criação (padrão)
        );

        // Verifica se a conexão foi estabelecida com sucesso
        if (stateCli.hPipe != INVALID_HANDLE_VALUE)
            break;

        // Verifica se ocorreu um erro ao tentar abrir o pipe
        if (GetLastError() != ERROR_PIPE_BUSY) {
            _tprintf(TEXT("Error: Pipe not available. GLE=%d\n"), GetLastError());
            return -1;
        }

        // Aguarda até que o pipe esteja disponível para conexão
        if (!WaitNamedPipe(lpszPipename, 3000)) {
            _tprintf(TEXT("Waited for 3 seconds, giving up.\n"));
            return -1;
        }
    }

    // Configura o modo de leitura do pipe
    dwMode = PIPE_READMODE_MESSAGE;
    fSuccess = SetNamedPipeHandleState(
        stateCli.hPipe,
        &dwMode,
        NULL,
        NULL
    );

    // Verifica se a configuração do modo de leitura foi bem-sucedida
    if (!fSuccess) {
        _tprintf(TEXT("SetNamedPipeHandleState failed. GLE=%d\n"), GetLastError());
        return -1;
    }

    // Cria uma nova thread para lidar com a leitura assíncrona das mensagens do servidor
    hThread = CreateThread(
        NULL,
        0,
        ThreadClientReader,
        &stateCli,
        0,
        &dwThreadId
    );

    // Verifica se a criação da thread foi bem-sucedida
    if (hThread == NULL) {
        _tprintf(TEXT("Failed to create the reader thread. GLE=%d\n"), GetLastError());
        return -1;
    }

    _tprintf(TEXT("\nBem-vindo! Faça 'login usr pw' para começar ou 'exit' para sair...\n"));

    // Loop principal do cliente para enviar mensagens ao servidor
    while (stateCli.deveContinuar) {
        // Lê a entrada do usuário
        readTChars(MsgToSend.msg, MSG_TAM);
        // Verifica se o usuário deseja sair do programa
        if (_tcscmp(MsgToSend.msg, TEXT("exit")) == 0) {
            _tprintf(TEXT("Exiting...\n"));
            // Define a flag de controle para encerrar o loop principal
            stateCli.deveContinuar = FALSE;

            // Fecha o handle do pipe
            CloseHandle(stateCli.hPipe);
            break;
        }
        ClientCommands(&stateCli, MsgToSend.msg);

        // Reseta a estrutura OVERLAPPED para uma nova operação de escrita assíncrona
        ZeroMemory(&OverlWr, sizeof(OverlWr));
        OverlWr.hEvent = stateCli.writeEvent;
        ResetEvent(stateCli.writeEvent);

        // Inicia uma operação de escrita assíncrona
        fSuccess = WriteFile(
            stateCli.hPipe,
            &MsgToSend,
            Msg_Size,
            &cWritten,
            &OverlWr
        );

        // Aguarda o término da operação de escrita
        WaitForSingleObject(stateCli.writeEvent, INFINITE);

        // Verifica se a operação de escrita foi concluída com sucesso
        if (!GetOverlappedResult(stateCli.hPipe, &OverlWr, &cWritten, FALSE) || cWritten < Msg_Size) {
            _tprintf(TEXT("WriteFile error or incomplete write. GLE=%d\n"), GetLastError());
        }
        else {
            //_tprintf(TEXT("Message sent: '%s'\n"), MsgToSend.msg);
        }
    }

    // Aguarda o término da thread de leitura
    WaitForSingleObject(hThread, INFINITE);

    // Fecha o handle da thread
    CloseHandle(hThread);

    // Fecha os handles dos eventos e do pipe
    CloseHandle(stateCli.readEvent);
    CloseHandle(stateCli.writeEvent);
    CloseHandle(stateCli.hPipe);

    return 0;
}