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
// estas variavéis são globais mas deviam passar por uma struct




DWORD WINAPI ThreadClientReader(LPVOID lparam) {

    Msg FromServer;
    DWORD bytesRead = 0;
    BOOL fSucess = FALSE;
    HANDLE hPipe = (HANDLE)lparam;

    OVERLAPPED OverlRd = { 0 };

    if (hPipe = NULL) {
        _tprintf(TEXT("\nThread Reader - o handle recebido no param da thread é inválido\n"));
        return -1;
    }

    // o evento podia ser criado aqui, no entanto é global
    // para permitir à main acordar ("quebrar") manualmente o Read
    // juntamente com o deveContinuar permite dizer à thread para sair

    OverlRd.hEvent = ReadReady;

    // informa cliente que está tudo a andar
    ReaderAtivo = 1;
    _tprintf(TEXT("\nThread Reader - a receber mensagens\n"));

    while (deveContinuar) {  // o habitual

        //Obtém mensagem do servidor
        //prepara leitura ASYNC

        ZeroMemory(&OverlRd, sizeof(OverlRd));
        OverlRd.hEvent = ReadReady;
        ResetEvent(ReadReady);


        fSucess = ReadFile(
            hPipe,          // handle para o pipe
            &FromServer,    // buffer para os dados a ler
            Msg_Sz,         // Tamanho msg a ler
            &bytesRead,     // Nº de bytes a ler
            &OverlRd        // NULL -> é overlapped I/O
        );

        WaitForSingleObject(ReadReady, INFINITE);
        if (deveContinuar == 0) {
            _tprintf(TEXT("\nRecebido ordem na main para terminar\n"));
            break;
        }
        _tprintf(TEXT("\nConcluído\n"));
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

    //tenta abrir uma instância     Poderá ter que esperar
    while (1) {

        hPipe = CreateFile(
            lpszPipename,   // nome do Pipe
            GENERIC_READ |  // acesso read e write
            GENERIC_WRITE,
            0 | FILE_SHARE_READ | FILE_SHARE_WRITE,  //partilha
            NULL,                       // atributos segurança e default
            OPEN_EXISTING,    // É para abrir um pipe já existente
            0 | FILE_FLAG_OVERLAPPED,  //atributos default
            NULL //sem ficheiro template
        );

        //Se correu tudo bem, já tem o handle e sai
        if (hPipe != INVALID_HANDLE_VALUE)
            break;

        //Caso contrário, se é um erro que não ERROR_PIPE_BUSY
        //É porque houve algo errado e sai

       //outra função



        // Se chegou aqui é porque todas as instâncias do pipe
        // estão ocupadas. Aguardar que uma fique livre com timeout

        if (!WaitNamedPipe(lpszPipename, 3000)) {
            _tprintf(TEXT("Esperei por instância durante 30 segundos. Desisti!"));
            return -1;
        }
    }


    // Chegou aqui através do break -> já tem uma instância
    // Importante: Mudar para mode PIPE_READMODE_MESSAGE
    // (no cliente, default = BYTE_MODE)

    dwMode = PIPE_READMODE_MESSAGE;
    fSucess = SetNamedPipeHandleState(
        hPipe, //handle para o pipe
        dwMode, //novo modo para o pipe
        NULL,   // não é para mudar max bytes
        NULL    // não é para mudar max timeout
    );

    if (!fSucess) {
        _tprintf(TEXT("SetNamedPipeHandleState falhou!"));
        return -1;
    }

    //Criar evento para leitura para uso da thread
    // é criado aqui no main e com handle global
    ReadReady = CreateEvent(
        NULL, //default Security
        TRUE, //Reset manual, por requisito do overlapped IO
        FALSE, // estado inicial
        NULL
    );

    if (ReadReady == NULL) {
        _tprintf(TEXT("\nCliente: Não foi possível criar o Evento Read"));
        return 1;
    }

    //Criar thread "ouvinte porque esta (a da main) só vai escrever 
    _tprintf(TEXT("Ligação estabelecida. Lançar thread reader"));

    hThread = CreateThread(
        NULL,     //Sem atributos de segurança
        0,
        ThreadClientReader,
        (LPVOID)hPipe,
        0,
        &dwThreadId
    );


    if (hThread == NULL) { // se a criação da thread der erro
        _tprintf(TEXT("Erro na criação da thread"));
        return -1;
    }


    HANDLE WriteReady;
    OVERLAPPED OverlWr = { 0 };

    WriteReady = CreateEvent(
        NULL,       // default security
        TRUE,       // reset manual, por requisito do overlapped IO
        FALSE,      // estado inicial
        NULL        // não precisa
    );
    if (WriteReady == NULL) {
        _tprintf(TEXT("\nCliente: não foi possível criar o Evento"));
        return -1;
    }


    _tprintf(TEXT("\nLigação estabelecida: \"exit\" para sair"));
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
        _tprintf(TEXT("\nWrite Concluído"));

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

    _tprintf(TEXT("\n Cliente vai terminar a ligação e sair"));

    CloseHandle(WriteReady);
    CloseHandle(hPipe);
    return 0;

}

