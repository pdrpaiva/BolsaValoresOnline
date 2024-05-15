#include "../utils.h"
#include <windows.h>
#include <tchar.h>
#include <stdio.h>
#include <io.h>
#include <fcntl.h>

void PrintTopEmpresas(Empresa* empresas, int numEmpresas, int topN);
void PrintUltimaTransacao(Transacao* transacao);

DWORD WINAPI UpdateThread(LPVOID lpParam);

int _tmain(int argc, TCHAR* argv[]) {

#ifdef UNICODE
    _setmode(_fileno(stdin), _O_WTEXT);
    _setmode(_fileno(stdout), _O_WTEXT);
    _setmode(_fileno(stderr), _O_WTEXT);
#endif

    if (argc < 2) {
        _tprintf(_T("Usage: %s <N>\n"), argv[0]);
        return 1;
    }

    int topN = _ttoi(argv[1]);
    if (topN <= 0 || topN > MAX_TOP_EMPRESAS) {
        _tprintf(_T("N must be between 1 and %d.\n"), MAX_TOP_EMPRESAS);
        return 1;
    }

    HANDLE hMapFile = OpenFileMapping(FILE_MAP_ALL_ACCESS, FALSE, SHARED_MEM_NAME);
    if (hMapFile == NULL) {
        _tprintf(_T("Could not open file mapping object (%d).\n"), GetLastError());
        return 1;
    }

    SharedData* pData = (SharedData*)MapViewOfFile(hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(SharedData));
    if (pData == NULL) {
        _tprintf(_T("Could not map view of file (%d).\n"), GetLastError());
        CloseHandle(hMapFile);
        return 1;
    }

    HANDLE hMutex = OpenMutex(SYNCHRONIZE, FALSE, MUTEX_NAME);
    if (hMutex == NULL) {
        _tprintf(_T("Could not open mutex (%d).\n"), GetLastError());
        UnmapViewOfFile(pData);
        CloseHandle(hMapFile);
        return 1;
    }

    HANDLE hEvent = OpenEvent(SYNCHRONIZE, FALSE, EVENT_NAME);
    if (hEvent == NULL) {
        _tprintf(_T("Could not open event (%d).\n"), GetLastError());
        CloseHandle(hMutex);
        UnmapViewOfFile(pData);
        CloseHandle(hMapFile);
        return 1;
    }

    HANDLE hUpdateThread = CreateThread(NULL, 0, UpdateThread, pData, 0, NULL);
    if (hUpdateThread == NULL) {
        _tprintf(_T("Could not create update thread (%d).\n"), GetLastError());
        CloseHandle(hEvent);
        CloseHandle(hMutex);
        UnmapViewOfFile(pData);
        CloseHandle(hMapFile);
        return 1;
    }

    WaitForSingleObject(hUpdateThread, INFINITE);
    CloseHandle(hUpdateThread);
    CloseHandle(hEvent);
    CloseHandle(hMutex);
    UnmapViewOfFile(pData);
    CloseHandle(hMapFile);

    return 0;
}

DWORD WINAPI UpdateThread(LPVOID lpParam) {
    SharedData* pData = (SharedData*)lpParam;
    HANDLE hEvent = OpenEvent(SYNCHRONIZE, FALSE, EVENT_NAME);
    HANDLE hMutex = OpenMutex(SYNCHRONIZE, FALSE, MUTEX_NAME);

    while (1) {
        WaitForSingleObject(hEvent, INFINITE);
        WaitForSingleObject(hMutex, INFINITE);

        PrintTopEmpresas(pData->empresas, pData->numEmpresas, MAX_TOP_EMPRESAS);
        PrintUltimaTransacao(&pData->ultimaTransacao);

        ReleaseMutex(hMutex);
        ResetEvent(hEvent); // Reset the event to non-signaled state
        Sleep(1000);
    }

    return 0;
}

void PrintTopEmpresas(Empresa* empresas, int numEmpresas, int topN) {
    Empresa topEmpresas[MAX_EMPRESAS];
    int count = (numEmpresas < topN) ? numEmpresas : topN;
    memcpy(topEmpresas, empresas, sizeof(Empresa) * numEmpresas);

    for (int i = 0; i < count - 1; ++i) {
        for (int j = i + 1; j < count; ++j) {
            if (topEmpresas[j].precoAcao > topEmpresas[i].precoAcao) {
                Empresa temp = topEmpresas[i];
                topEmpresas[i] = topEmpresas[j];
                topEmpresas[j] = temp;
            }
        }
    }

    _tprintf(_T("\n--- Top %d Empresas ---\n"), topN);
    for (int i = 0; i < count; ++i) {
        _tprintf(_T("%s: %d ações, %.2f cada\n"), topEmpresas[i].nomeEmpresa, topEmpresas[i].numAcoes, topEmpresas[i].precoAcao);
    }
}

void PrintUltimaTransacao(Transacao* transacao) {
    _tprintf(_T("\n--- Última Transação ---\n"));
    _tprintf(_T("Empresa: %s\n"), transacao->nomeEmpresa);
    _tprintf(_T("Número de Ações: %d\n"), transacao->numAcoes);
    _tprintf(_T("Valor: %.2f\n"), transacao->valor);
}