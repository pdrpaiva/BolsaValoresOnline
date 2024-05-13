#include "utils.h"
#include <windows.h>
#include <tchar.h>
#include <stdio.h>
#include <io.h>
#include <fcntl.h>

#define MSGTXTSZ 60

typedef struct {

    TCHAR msg[MSGTXTSZ];

}Msg;

#define Msg_Sz sizeof(Msg)


void readTChars(TCHAR* p, int maxChars) {

    size_t len;
    _fgetts(p, maxChars, stdin);
    len = _tcslen(p);
    if (p[len - 1] == TEXT('\n'));
    p[len - 1] = '\0';

}




int deveContinuar = 0;
int ReaderAtivo = 0;
HANDLE ReadReady;
// estas variav�is s�o globais mas deviam passar por uma struct




DWORD WINAPI ThreadClientReader(LPVOID lparam) {

    Msg FromServer;
    DWORD bytesRead = 0;
    BOOL fSucess = FALSE;
    HANDLE hPipe = (HANDLE)lparam;

    OVERLAPPED OverlRd = { 0 };

    if (hPipe = NULL) {
        _tprintf(TEXT("\nThread Reader - o handle recebido no param da thread � inv�lido\n"));
        return -1;
    }

    // o evento podia ser criado aqui, no entanto � global
    // para permitir � main acordar ("quebrar") manualmente o Read
    // juntamente com o deveContinuar permite dizer � thread para sair

    OverlRd.hEvent = ReadReady;

    // informa cliente que est� tudo a andar
    ReaderAtivo = 1;
    _tprintf(TEXT("\nThread Reader - a receber mensagens\n"));

    while (deveContinuar) {  // o habitual

        //Obt�m mensagem do servidor
        //prepara leitura ASYNC

        ZeroMemory(&OverlRd, sizeof(OverlRd));
        OverlRd.hEvent = ReadReady;
        ResetEvent(ReadReady);


        fSucess = ReadFile(
            hPipe,          // handle para o pipe
            &FromServer,    // buffer para os dados a ler
            Msg_Sz,         // Tamanho msg a ler
            &bytesRead,     // N� de bytes a ler
            &OverlRd        // NULL -> � overlapped I/O
        );

        WaitForSingleObject(ReadReady, INFINITE);
        if (deveContinuar == 0) {
            _tprintf(TEXT("\nRecebido ordem na main para terminar\n"));
            break;
        }
        _tprintf(TEXT("\nConclu�do\n"));
    }
}





int _tmain(int argc, LPTSTR argv[]) {

    HANDLE hPipe;
    BOOL fSucess = FALSE;
    DWORD dwMode, cWritten;
    LPTSTR lpszPipename = TEXT("\\\\.\\pipe\\teste");

    Msg MsgToSend;
    HANDLE hThread;
    DWORD dwThreadId = 0;

#ifdef UNICODE
    _setmode(_fileno(stdin), _O_WTEXT);
    _setmode(_fileno(stdout), _O_WTEXT);
    _setmode(_fileno(stderr), _O_WTEXT);
#endif

    HANDLE hUserToken = NULL;
    int ret;

    //tenta abrir uma inst�ncia     Poder� ter que esperar
    while (1) {

        hPipe = CreateFile(
            lpszPipename,   // nome do Pipe
            GENERIC_READ |  // acesso read e write
            GENERIC_WRITE,
            0 | FILE_SHARE_READ | FILE_SHARE_WRITE,  //partilha
            NULL,                       // atributos seguran�a e default
            OPEN_EXISTING,    // � para abrir um pipe j� existente
            0 | FILE_FLAG_OVERLAPPED,  //atributos default
            NULL //sem ficheiro template
        );

        //Se correu tudo bem, j� tem o handle e sai
        if (hPipe != INVALID_HANDLE_VALUE)
            break;

        //Caso contr�rio, se � um erro que n�o ERROR_PIPE_BUSY
        //� porque houve algo errado e sai

       //outra fun��o



        // Se chegou aqui � porque todas as inst�ncias do pipe
        // est�o ocupadas. Aguardar que uma fique livre com timeout

        if (!WaitNamedPipe(lpszPipename, 3000)) {
            _tprintf(TEXT("Esperei por inst�ncia durante 30 segundos. Desisti!"));
            return -1;
        }
    }


    // Chegou aqui atrav�s do break -> j� tem uma inst�ncia
    // Importante: Mudar para mode PIPE_READMODE_MESSAGE
    // (no cliente, default = BYTE_MODE)

    dwMode = PIPE_READMODE_MESSAGE;
    fSucess = SetNamedPipeHandleState(
        hPipe, //handle para o pipe
        dwMode, //novo modo para o pipe
        NULL,   // n�o � para mudar max bytes
        NULL    // n�o � para mudar max timeout
    );

    if (!fSucess) {
        _tprintf(TEXT("SetNamedPipeHandleState falhou!"));
        return -1;
    }

    //Criar evento para leitura para uso da thread
    // � criado aqui no main e com handle global
    ReadReady = CreateEvent(
        NULL, //default Security
        TRUE, //Reset manual, por requisito do overlapped IO
        FALSE, // estado inicial
        NULL
    );

    if (ReadReady == NULL) {
        _tprintf(TEXT("\nCliente: N�o foi poss�vel criar o Evento Read"));
        return 1;
    }

    //Criar thread "ouvinte porque esta (a da main) s� vai escrever 
    _tprintf(TEXT("Liga��o estabelecida. Lan�ar thread reader"));

    hThread = CreateThread(
        NULL,     //Sem atributos de seguran�a
        0,
        ThreadClientReader,
        (LPVOID)hPipe,
        0,
        &dwThreadId
    );


    if (hThread == NULL) { // se a cria��o da thread der erro
        _tprintf(TEXT("Erro na cria��o da thread"));
        return -1;
    }


    HANDLE WriteReady;
    OVERLAPPED OverlWr = { 0 };

    WriteReady = CreateEvent(
        NULL,       // default security
        TRUE,       // reset manual, por requisito do overlapped IO
        FALSE,      // estado inicial
        NULL        // n�o precisa
    );
    if (WriteReady == NULL) {
        _tprintf(TEXT("\nCliente: n�o foi poss�vel criar o Evento"));
        return -1;
    }


    _tprintf(TEXT("\nLiga��o estabelecida: \"exit\" para sair"));
    while (1) {
        readTChars(MsgToSend.msg, MSGTXTSZ);
        if (_tcscmp(TEXT("exit"), MsgToSend.msg) == 0) {
            break;
        }

        _tprintf(TEXT("\nA enviar %d bytes: %s"), (int)Msg_Sz, MsgToSend.msg);

        ZeroMemory(&OverlWr, sizeof(OverlWr));
        OverlWr.hEvent = WriteReady;
        ResetEvent(WriteReady);


        fSucess = WriteFile(
            hPipe,      //handle para o pipe
            &MsgToSend, // message ponteiro
            Msg_Sz,      // tamanho da mensagem
            &cWritten,      // ptr p/ guardar num bytes escritos
            &OverlWr
        );

        WaitForSingleObject(WriteReady, INFINITE);
        _tprintf(TEXT("\nWrite Conclu�do"));

        GetOverlappedResult(hPipe, &OverlWr, &cWritten, FALSE);
        if (cWritten < Msg_Sz)
            _tprintf(TEXT("WriteFile talvez falhou"));

        _tprintf(TEXT("Mensagem Enviada"));
        if (ReaderAtivo == 0) {
            _tprintf(TEXT("\n Thread reader nao esta a executar"));
        }
    }

    _tprintf(TEXT("\n Encerrar thread Ouvinte"));
    deveContinuar = 0;
    SetEvent(ReadReady);

    if (ReaderAtivo) {
        WaitForSingleObject(hThread, 3000);
        _tprintf(TEXT("\n Thread Reader encerrada ou fartei de esperar"));
    }

    _tprintf(TEXT("\n Cliente vai terminar a liga��o e sair"));

    CloseHandle(WriteReady);
    CloseHandle(hPipe);
    return 0;

}

