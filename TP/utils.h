#ifndef UTILS_H
#define UTILS_H

#include <windows.h>
#include <tchar.h>

#define MSG_TAM 256
#define MAXCLIENTES 10
#define MAX_EMPRESAS 30
#define MAX_UTILIZADORES 20
#define Msg_Size sizeof(Msg)

typedef struct {
    TCHAR msg[MSG_TAM];
} Msg;

typedef struct {
    HANDLE hPipe;
    HANDLE readEvent;
    HANDLE writeEvent;
    BOOL deveContinuar;
    BOOL readerAtivo;
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
    int numEmpresas;
    int numUtilizadores;
    BOOL tradingPaused;
    HANDLE clientPipes[MAXCLIENTES]; // Array de handles para os pipes dos clientes
    HANDLE writeReady;               // Handle para o evento de escrita pronta
    HANDLE readEvent;
} ServerState;


void readTChars(TCHAR* p, int maxChars);
void InitializeServerState(ServerState* state);
void PrintLastError(const TCHAR* msg);
void PrintMenu();

void BolsaCommands(ServerState* stateServ, TCHAR* command);

//void AddCompany(ServerState* state, const TCHAR* nomeEmpresa, int numAcoes, double precoAcao, TCHAR* response);
//void ListCompanies(const ServerState* state, TCHAR* response);
//void SetStockPrice(ServerState* state, const TCHAR* nomeEmpresa, double newPrice, TCHAR* response);
//void ListUsers(const ServerState* state, TCHAR* response);
//void PauseTrading(ServerState* state, int duration, TCHAR* response);
//void CloseSystem(ServerState* state, TCHAR* response);


#endif // UTILS_H
