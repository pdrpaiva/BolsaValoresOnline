#include <windows.h>
#include <tchar.h>
#include "resource.h"
#include "../utils.h"

LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK About(HWND, UINT, WPARAM, LPARAM);
void DrawGraph(HWND hWnd, SharedData* pSharedData, int n, double scaleMin, double scaleMax);

SharedData* pSharedData;
HANDLE hMutex;
HANDLE hEvent;

int APIENTRY _tWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPTSTR lpCmdLine, int nCmdShow) {
    MSG msg;
    HWND hWnd;
    WNDCLASSEX wcex;

    hMutex = OpenMutex(SYNCHRONIZE, FALSE, MUTEX_NAME);
    hEvent = OpenEvent(EVENT_MODIFY_STATE | SYNCHRONIZE, FALSE, EVENT_NAME);

    if (hMutex == NULL || hEvent == NULL) {
        MessageBox(NULL, _T("Falha ao abrir o mutex ou evento."), _T("Erro"), MB_OK);
        return 1;
    }

    HANDLE hMapFile = OpenFileMapping(FILE_MAP_ALL_ACCESS, FALSE, SHARED_MEM_NAME);
    if (hMapFile == NULL) {
        MessageBox(NULL, _T("Falha ao abrir o mapeamento de arquivo."), _T("Erro"), MB_OK);
        return 1;
    }

    pSharedData = (SharedData*)MapViewOfFile(hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(SharedData));
    if (pSharedData == NULL) {
        MessageBox(NULL, _T("Falha ao mapear a memória compartilhada."), _T("Erro"), MB_OK);
        return 1;
    }

    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_APPLICATION));
    wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName = MAKEINTRESOURCE(IDR_MYMENU);
    wcex.lpszClassName = _T("BoardGUI");
    wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_APPLICATION));

    if (!RegisterClassEx(&wcex)) {
        MessageBox(NULL, _T("Falha ao registrar a classe da janela."), _T("Erro"), MB_OK);
        return 1;
    }

    hWnd = CreateWindow(_T("BoardGUI"), _T("Board GUI"), WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 800, 600, NULL, NULL, hInstance, NULL);

    if (!hWnd) {
        MessageBox(NULL, _T("Falha ao criar a janela."), _T("Erro"), MB_OK);
        return 1;
    }

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    CloseHandle(hEvent);
    CloseHandle(hMutex);
    UnmapViewOfFile(pSharedData);
    CloseHandle(hMapFile);

    return (int)msg.wParam;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    static int n = 10;
    static double scaleMin = 0.0, scaleMax = 100.0;

    switch (message) {
    case WM_COMMAND: {
        int wmId = LOWORD(wParam);
        // Parse the menu selections:
        switch (wmId) {
        case IDM_ABOUT:
            DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
            break;
        case IDM_EXIT:
            DestroyWindow(hWnd);
            break;
        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
        }
    }
                   break;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        DrawGraph(hWnd, pSharedData, n, scaleMin, scaleMax);
        EndPaint(hWnd, &ps);
    }
                 break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    UNREFERENCED_PARAMETER(lParam);
    switch (message) {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}

void DrawGraph(HWND hWnd, SharedData* pSharedData, int n, double scaleMin, double scaleMax) {
    RECT rect;
    GetClientRect(hWnd, &rect);
    int width = rect.right - rect.left;
    int height = rect.bottom - rect.top;
    HDC hdc = GetDC(hWnd);

    WaitForSingleObject(hMutex, INFINITE);

    // Sort the companies by stock price in descending order
    for (int i = 0; i < pSharedData->numEmpresas - 1; ++i) {
        for (int j = i + 1; j < pSharedData->numEmpresas; ++j) {
            if (pSharedData->empresas[j].precoAcao > pSharedData->empresas[i].precoAcao) {
                Empresa temp = pSharedData->empresas[i];
                pSharedData->empresas[i] = pSharedData->empresas[j];
                pSharedData->empresas[j] = temp;
            }
        }
    }

    // Draw bars for the top N companies
    int barWidth = width / (n * 2);
    for (int i = 0; i < n && i < pSharedData->numEmpresas; ++i) {
        int barHeight = (int)(((pSharedData->empresas[i].precoAcao - scaleMin) / (scaleMax - scaleMin)) * height);
        RECT barRect = { i * 2 * barWidth, height - barHeight, (i * 2 + 1) * barWidth, height };
        FillRect(hdc, &barRect, (HBRUSH)GetStockObject(BLACK_BRUSH));
        TextOut(hdc, i * 2 * barWidth, height - barHeight - 20, pSharedData->empresas[i].nomeEmpresa, _tcslen(pSharedData->empresas[i].nomeEmpresa));
    }

    // Display the most recent transaction
    TCHAR lastTransaction[100];
    _stprintf_s(lastTransaction, 100, _T("Última transação: %s - %d ações - %.2f€"),
        pSharedData->ultimaTransacao.nomeEmpresa,
        pSharedData->ultimaTransacao.numAcoes,
        pSharedData->ultimaTransacao.valor);
    TextOut(hdc, 10, 10, lastTransaction, _tcslen(lastTransaction));

    ReleaseMutex(hMutex);
    ReleaseDC(hWnd, hdc);
}
