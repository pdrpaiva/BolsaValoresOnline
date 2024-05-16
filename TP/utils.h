#ifndef UTILS_H
#define UTILS_H

#include <windows.h>
#include <tchar.h>

#define MSG_TAM 256
#define MAXCLIENTES 10
#define MAX_EMPRESAS 30
#define MAX_UTILIZADORES 20
#define Msg_Size sizeof(Msg)

#define MAX_TOP_EMPRESAS 10
#define SHARED_MEM_NAME _T("BolsaSharedMemory")
#define MUTEX_NAME _T("BolsaMutex")
#define EVENT_NAME _T("BolsaEvent")
#define CLOSE_EVENT_NAME _T("BolsaCloseEvent")

typedef struct {
    TCHAR msg[MSG_TAM];
} Msg;

typedef struct {
    HANDLE hPipe;
    HANDLE readEvent;
    HANDLE writeEvent;
    HANDLE shutdownEvent;
    BOOL deveContinuar;
    BOOL readerAtivo;
    BOOL ligado;
} ClientState;

typedef struct {
    TCHAR nomeEmpresa[50];
    int numAcoes;
    double precoAcao;
} Empresa;

typedef struct {
    TCHAR username[50];
    double saldo;
    BOOL isOnline;
} Utilizador;

typedef struct {
    Empresa empresas[MAX_EMPRESAS];
    Utilizador utilizadores[MAX_UTILIZADORES];
    HANDLE clientPipes[MAXCLIENTES]; // Array de handles para os pipes dos clientes
    HANDLE writeReady;               // Handle para o evento de escrita pronta
    HANDLE readEvent;
    HANDLE closeEvent;
    HANDLE closeMutex;
    HANDLE pipeAux;
    HANDLE currentPipe; // Adicionado para armazenar o handle do pipe atual
    int numEmpresas;
    int numUtilizadores;
    BOOL tradingPaused;
    BOOL running;
    BOOL closeFlag;
} ServerState;

//teste

typedef struct {
    TCHAR nomeEmpresa[50];
    int numAcoes;
    double valor;
} Transacao;

typedef struct {
    Empresa empresas[MAX_EMPRESAS];
    Transacao ultimaTransacao;
    int numEmpresas;
} SharedData;


//teste


//void readTChars(TCHAR* p, int maxChars) {
//    size_t len;
//    _fgetts(p, maxChars, stdin);
//    len = _tcslen(p);
//    if (p[len - 1] == TEXT('\n')) {
//        p[len - 1] = TEXT('\0');
//    }
//}
void InitializeServerState(ServerState* state);
void PrintLastError(const TCHAR* msg);
void PrintMenu();

void BolsaCommands(ServerState* stateServ, TCHAR* command);

void PrintMenuCliente();
void readTCharsWithTimeout(TCHAR* p, int maxChars, HANDLE shutdownEvent);
void readTChars(TCHAR* p, int maxChars);
void ClientCommands(ClientState* stateCli, TCHAR* command);

//void AddCompany(ServerState* state, const TCHAR* nomeEmpresa, int numAcoes, double precoAcao, TCHAR* response);
//void ListCompanies(const ServerState* state, TCHAR* response);
//void SetStockPrice(ServerState* state, const TCHAR* nomeEmpresa, double newPrice, TCHAR* response);
//void ListUsers(const ServerState* state, TCHAR* response);
//void PauseTrading(ServerState* state, int duration, TCHAR* response);
//void CloseSystem(ServerState* state, TCHAR* response);


#endif // UTILS_H