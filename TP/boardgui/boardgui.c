#include <windows.h>
#include <tchar.h>
#include "../utils.h"

// Nome da classe da janela
TCHAR szProgName[] = TEXT("BoardGUI");

// Funções de callback
LRESULT CALLBACK TrataEventos(HWND, UINT, WPARAM, LPARAM);
void DrawBarGraph(HDC hdc, RECT rect, SharedData* pSharedData, int numEmpresas);

// Variáveis globais
SharedData* pSharedData;
HANDLE hMapFile, hMutex, hEvent;
int numEmpresas = 5;
int lowerLimit = 0;
int upperLimit = 1000;

// Função principal do programa
int WINAPI _tWinMain(HINSTANCE hInst, HINSTANCE hPrevInst, LPTSTR lpCmdLine, int nCmdShow) {
    HWND hWnd;
    MSG lpMsg;
    WNDCLASSEX wcApp;

    // Definição das características da janela
    wcApp.cbSize = sizeof(WNDCLASSEX);
    wcApp.hInstance = hInst;
    wcApp.lpszClassName = szProgName;
    wcApp.lpfnWndProc = TrataEventos;
    wcApp.style = CS_HREDRAW | CS_VREDRAW;
    wcApp.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wcApp.hIconSm = LoadIcon(NULL, IDI_INFORMATION);
    wcApp.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcApp.lpszMenuName = NULL;
    wcApp.cbClsExtra = 0;
    wcApp.cbWndExtra = 0;
    wcApp.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);

    // Registar a classe "wcApp" no Windows
    if (!RegisterClassEx(&wcApp)) {
        MessageBox(NULL, TEXT("Falha ao registrar a classe da janela."), TEXT("Erro"), MB_OK);
        return 0;
    }

    // Criar a janela
    hWnd = CreateWindow(szProgName, TEXT("Board GUI"), WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
        HWND_DESKTOP, NULL, hInst, NULL);

    if (!hWnd) {
        MessageBox(NULL, TEXT("Falha ao criar a janela."), TEXT("Erro"), MB_OK);
        return 0;
    }

    // Mostrar e atualizar a janela
    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    // Abrir memória partilhada e objetos de sincronização
    hMapFile = OpenFileMapping(FILE_MAP_ALL_ACCESS, FALSE, SHARED_MEM_NAME);
    if (hMapFile == NULL) {
        MessageBox(hWnd, TEXT("OpenFileMapping failed"), TEXT("Error"), MB_OK);
        return 1;
    }

    pSharedData = (SharedData*)MapViewOfFile(hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(SharedData));
    if (pSharedData == NULL) {
        MessageBox(hWnd, TEXT("MapViewOfFile failed"), TEXT("Error"), MB_OK);
        CloseHandle(hMapFile);
        return 1;
    }

    hMutex = OpenMutex(SYNCHRONIZE, FALSE, MUTEX_NAME);
    if (hMutex == NULL) {
        MessageBox(hWnd, TEXT("OpenMutex failed"), TEXT("Error"), MB_OK);
        UnmapViewOfFile(pSharedData);
        CloseHandle(hMapFile);
        return 1;
    }

    hEvent = OpenEvent(SYNCHRONIZE, FALSE, EVENT_NAME);
    if (hEvent == NULL) {
        MessageBox(hWnd, TEXT("OpenEvent failed"), TEXT("Error"), MB_OK);
        CloseHandle(hMutex);
        UnmapViewOfFile(pSharedData);
        CloseHandle(hMapFile);
        return 1;
    }

    // Loop de mensagens
    while (GetMessage(&lpMsg, NULL, 0, 0) > 0) {
        TranslateMessage(&lpMsg);
        DispatchMessage(&lpMsg);
    }

    // Fechar handles
    CloseHandle(hEvent);
    CloseHandle(hMutex);
    UnmapViewOfFile(pSharedData);
    CloseHandle(hMapFile);

    return (int)lpMsg.wParam;
}

// Função de tratamento de eventos da janela
LRESULT CALLBACK TrataEventos(HWND hWnd, UINT messg, WPARAM wParam, LPARAM lParam) {
    HDC hdc;
    PAINTSTRUCT ps;
    RECT rect;

    switch (messg) {
    case WM_PAINT:
        hdc = BeginPaint(hWnd, &ps);
        GetClientRect(hWnd, &rect);
        DrawBarGraph(hdc, rect, pSharedData, numEmpresas);
        EndPaint(hWnd, &ps);
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, messg, wParam, lParam);
    }
    return 0;
}

// Função para desenhar o gráfico de barras
void DrawBarGraph(HDC hdc, RECT rect, SharedData* pSharedData, int numEmpresas) {
    WaitForSingleObject(hMutex, INFINITE);

    // Verificar se os dados estão disponíveis
    if (pSharedData->numEmpresas == 0) {
        MessageBox(NULL, TEXT("Nenhuma empresa encontrada."), TEXT("Informação"), MB_OK);
        ReleaseMutex(hMutex);
        return;
    }

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

    // Desenhar o gráfico de barras
    int barWidth = (rect.right - rect.left) / numEmpresas;
    for (int i = 0; i < numEmpresas && i < pSharedData->numEmpresas; ++i) {
        int barHeight = (int)((pSharedData->empresas[i].precoAcao - lowerLimit) / (upperLimit - lowerLimit) * (rect.bottom - rect.top));
        RECT barRect = {
            rect.left + i * barWidth,
            rect.bottom - barHeight,
            rect.left + (i + 1) * barWidth,
            rect.bottom
        };
        FillRect(hdc, &barRect, (HBRUSH)GetStockObject(GRAY_BRUSH));
        TCHAR empresaNome[50];
        _stprintf_s(empresaNome, 50, TEXT("%s"), pSharedData->empresas[i].nomeEmpresa);
        DrawText(hdc, empresaNome, -1, &barRect, DT_CENTER | DT_BOTTOM);
    }

    // Mostrar a última transação
    TCHAR transacao[100];
    _stprintf_s(transacao, 100, TEXT("Última Transação: %s - %d ações - %.2f €"),
        pSharedData->ultimaTransacao.nomeEmpresa,
        pSharedData->ultimaTransacao.numAcoes,
        pSharedData->ultimaTransacao.valor);
    TextOut(hdc, 10, 10, transacao, _tcslen(transacao));

    ReleaseMutex(hMutex);
}
