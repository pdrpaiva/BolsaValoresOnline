#include <windows.h>
#include <fcntl.h>
#include <tchar.h>
#include <stdio.h>
#include "../utils.h"

void PrintLastError(const TCHAR* msg) {
    DWORD eNum;
    TCHAR sysMsg[256];
    TCHAR* p;

    eNum = GetLastError();
    FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL, eNum,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        sysMsg, 256, NULL);

    p = sysMsg;
    while ((*p > 31) || (*p == 9))
        ++p;
    do { *p-- = 0; } while ((p >= sysMsg) && ((*p == '.') || (*p < 33)));

    _tprintf(TEXT("\n%s: %s (Error %d)\n"), msg, sysMsg, eNum);
}

void PrintTopCompaniesAndLastTransaction(SharedData* pSharedData, HANDLE hMutex, int N) {
    WaitForSingleObject(hMutex, INFINITE);

    // Ordenar as empresas por preço de ação (ordem decrescente)
    for (int i = 0; i < pSharedData->numEmpresas - 1; ++i) {
        for (int j = i + 1; j < pSharedData->numEmpresas; ++j) {
            if (pSharedData->empresas[j].precoAcao > pSharedData->empresas[i].precoAcao) {
                Empresa temp = pSharedData->empresas[i];
                pSharedData->empresas[i] = pSharedData->empresas[j];
                pSharedData->empresas[j] = temp;
            }
        }
    }

    // Mostrar as N empresas mais valiosas
    _tprintf(TEXT("\nTop %d Empresas:\n"), N);
    for (int i = 0; i < N && i < pSharedData->numEmpresas; ++i) {
        _tprintf(TEXT("%d. %s - Preço: %.2f €\n"), i + 1, pSharedData->empresas[i].nomeEmpresa, pSharedData->empresas[i].precoAcao);
    }

    // Mostrar a última transação
    _tprintf(TEXT("\nÚltima Transação:\n"));
    if (_tcslen(pSharedData->ultimaTransacao.nomeEmpresa) == 0 &&
        pSharedData->ultimaTransacao.numAcoes == 0 &&
        pSharedData->ultimaTransacao.valor == 0.0) {
        _tprintf(TEXT("Sem transações.\n"));
    }
    else {
        if (pSharedData->ultimaTransacao.numAcoes >= 0) {
            _tprintf(TEXT("COMPRA | Empresa: %s | Número de Ações: %d | Valor: %.2f €\n"),
                pSharedData->ultimaTransacao.nomeEmpresa,
                pSharedData->ultimaTransacao.numAcoes,
                pSharedData->ultimaTransacao.valor);
        }
        else {
            _tprintf(TEXT("VENDA | Empresa: %s | Número de Ações: %d | Valor: %.2f €\n"),
                pSharedData->ultimaTransacao.nomeEmpresa,
                abs(pSharedData->ultimaTransacao.numAcoes),
                pSharedData->ultimaTransacao.valor);
        }
    }
    _tprintf(TEXT("\n------------------------------------------------------------\n"));

    ReleaseMutex(hMutex);
}

int _tmain(int argc, TCHAR* argv[]) {
    if (argc < 2) {
        _tprintf(TEXT("Uso: %s <N>\n"), argv[0]);
        return 1;
    }

    int N = _ttoi(argv[1]);
    if (N < 1 || N > MAX_TOP_EMPRESAS) {
        _tprintf(TEXT("N deve estar entre 1 e %d\n"), MAX_TOP_EMPRESAS);
        return 1;
    }

#ifdef UNICODE
    _setmode(_fileno(stdin), _O_WTEXT);
    _setmode(_fileno(stdout), _O_WTEXT);
    _setmode(_fileno(stderr), _O_WTEXT);
#endif

    HANDLE hMapFile = OpenFileMapping(FILE_MAP_ALL_ACCESS, FALSE, SHARED_MEM_NAME);
    if (hMapFile == NULL) {
        PrintLastError(TEXT("OpenFileMapping failed"));
        return 1;
    }

    SharedData* pSharedData = (SharedData*)MapViewOfFile(hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(SharedData));
    if (pSharedData == NULL) {
        PrintLastError(TEXT("MapViewOfFile failed"));
        CloseHandle(hMapFile);
        return 1;
    }

    HANDLE hMutex = OpenMutex(SYNCHRONIZE, FALSE, MUTEX_NAME);
    if (hMutex == NULL) {
        PrintLastError(TEXT("OpenMutex failed"));
        UnmapViewOfFile(pSharedData);
        CloseHandle(hMapFile);
        return 1;
    }

    HANDLE hEvent = OpenEvent(EVENT_MODIFY_STATE | SYNCHRONIZE, FALSE, EVENT_NAME);
    if (hEvent == NULL) {
        PrintLastError(TEXT("OpenEvent failed"));
        CloseHandle(hMutex);
        UnmapViewOfFile(pSharedData);
        CloseHandle(hMapFile);
        return 1;
    }

    _tprintf(TEXT("\n--------------------- Bolsa de Valores ---------------------\n\n"));

    while (1) {
        WaitForSingleObject(hEvent, INFINITE); // Espera pelo evento
        PrintTopCompaniesAndLastTransaction(pSharedData, hMutex, N);
        ResetEvent(hEvent); 
    }

    CloseHandle(hEvent);
    CloseHandle(hMutex);
    UnmapViewOfFile(pSharedData);
    CloseHandle(hMapFile);

    return 0;
}
