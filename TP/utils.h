#ifndef UTILS_H
#define UTILS_H

#include <windows.h>
#include <tchar.h>

#define MSG_TAM 2000
#define MAXCLIENTES 10
#define MAX_EMPRESAS 30
#define MAX_UTILIZADORES 20
#define MAX_ACOES 5
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
    TCHAR nomeEmpresa[50];
    int numAcoes;
    double precoAcao;
} Empresa;

typedef struct {
    TCHAR nomeEmpresa[50];
    int numAcoes;
    double valor;
} Transacao;

typedef struct {
    TCHAR nomeEmpresa[50];
    int numAcoes;
} CarteiraAcoes;

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
    Transacao ultimaTransacao;
    int numEmpresas;
} SharedData;

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
    BOOL running;
    BOOL closeFlag;

    BOOL tradingPaused;
    int pauseDuration;

    Transacao transacao;
    HANDLE hMapFile;
    HANDLE hMutex;
    HANDLE hEvent;
} ServerState;

//--------------------------------------//--------------------------------------//--------------------------------------//

// Cliente:

void PrintMenuCliente();
void readTCharsWithTimeout(TCHAR* p, int maxChars, HANDLE shutdownEvent);
void ClientCommands(ClientState* stateCli, TCHAR* command);
void CloseClientPipe(ClientState* stateCli);

//--------------------------------------//--------------------------------------//--------------------------------------//

// Servidor:

void InitializeServerState(ServerState* state);
void PrintLastError(const TCHAR* msg);
void PrintMenu();
void BolsaCommands(ServerState* stateServ, TCHAR* command);
BOOL verificaLogin(const ServerState* stateServ, const TCHAR* username, const TCHAR* password);
void SellShares(ServerState* state, const TCHAR* nomeEmpresa, int numAcoes, TCHAR* response);
double GetBalance(const ServerState* stateServ, const TCHAR* username);
void BuyShares(ServerState* state, const TCHAR* nomeEmpresa, int numAcoes, const TCHAR* username, TCHAR* response);
BOOL RegistrarCompra(Utilizador* utilizador, const TCHAR* nomeEmpresa, int numAcoes);
double CheckBalance(ServerState* state, TCHAR* username);
void PauseTrading(ServerState* state, int duration, TCHAR* response);
void NotifyClients(ServerState* state, const TCHAR* message);
void RegistrarTransacao(ServerState* state, const TCHAR* nomeEmpresa, int numAcoes, double valor);
BOOL RegistrarVenda(Utilizador* utilizador, const TCHAR* nomeEmpresa, int numAcoes);
//--------------------------------------//--------------------------------------//--------------------------------------//

//Bolsa : 

BOOL InitializeSharedResources(ServerState* state);
void CleanupSharedResources(ServerState* state);
void UpdateSharedData(ServerState* state);

//--------------------------------------//--------------------------------------//--------------------------------------//

#endif // UTILS_H
