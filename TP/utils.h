#ifndef UTILS_H
#define UTILS_H

#include <windows.h>
#include <tchar.h>

#define MSG_TAM 2000
#define MAXCLIENTES 10
#define MAX_EMPRESAS 30
#define MAX_UTILIZADORES 20
#define Msg_Size sizeof(Msg)
#define MAX_ACOES 5

#define MAX_TOP_EMPRESAS 10
#define SHARED_MEM_NAME _T("BolsaSharedMemory")
#define MUTEX_NAME _T("BolsaMutex")
#define EVENT_NAME _T("BolsaEvent")
#define CLOSE_EVENT_NAME _T("BolsaCloseEvent")

typedef struct {
    TCHAR msg[MSG_TAM];
} Msg;

typedef struct {
    TCHAR nomeEmpresa[50];
    int numAcoes;
} CarteiraAcoes;

typedef struct {
    HANDLE hPipe;
    HANDLE readEvent;
    HANDLE writeEvent;
    HANDLE shutdownEvent;
    BOOL deveContinuar;
    BOOL readerAtivo;
    BOOL ligado;
    TCHAR username[50];
} ClientState;

typedef struct {
    TCHAR nomeEmpresa[50];
    int numAcoes;
    double precoAcao;
} Empresa;

typedef struct {
    TCHAR username[50];
    TCHAR password[50];
    double saldo;
    BOOL isOnline;
    CarteiraAcoes carteira[MAX_ACOES];
    int numAcoes;
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

void InitializeServerState(ServerState* state);
void PrintLastError(const TCHAR* msg);
void PrintMenu();
void BolsaCommands(ServerState* stateServ, TCHAR* command);
void PrintMenuCliente();
void readTCharsWithTimeout(TCHAR* p, int maxChars, HANDLE shutdownEvent);
void readTChars(TCHAR* p, int maxChars);
void ClientCommands(ClientState* stateCli, TCHAR* command);
BOOL verificaLogin(const ServerState* stateServ, const TCHAR* username, const TCHAR* password);
void CloseClientPipe(ClientState* stateCli);
void PrintLastError(const TCHAR* msg);
void SellShares(ServerState* state, const TCHAR* nomeEmpresa, int numAcoes, TCHAR* response);
double GetBalance(const ServerState* stateServ, const TCHAR* username);
void BuyShares(ServerState* state, const TCHAR* nomeEmpresa, int numAcoes, const TCHAR* username, TCHAR* response);
void RegistrarCompra(Utilizador* utilizador, const TCHAR* nomeEmpresa, int numAcoes);
double CheckBalance(ServerState* state, TCHAR* username);

#endif // UTILS_H