#include <windows.h>
#include <tchar.h>
#include "../utils.h"  // Certifique-se de que o caminho para utils.h está correto

// Definições de IDs de menu e controle
#define IDM_ABOUT 100
#define IDM_EXIT 101
#define IDC_SCALEMIN 200
#define IDC_SCALEMAX 201
#define IDC_UPDATE 202
#define IDC_NUMEMPRESAS 203

// Declaração de funções
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
void DrawGraph(HWND hWnd, SharedData* pSharedData, int n, double scaleMin, double scaleMax);
void DrawLastTransaction(HDC hdc, SharedData* pSharedData);

SharedData* pSharedData;
HANDLE hMutex;
HANDLE hEvent;
HWND hScaleMin, hScaleMax, hNumEmpresas, hUpdateButton;

void AdicionarMenu(HWND hWnd) {
    HMENU hMenu = CreateMenu();
    HMENU hSubMenu = CreatePopupMenu();

    AppendMenu(hSubMenu, MF_STRING, IDM_ABOUT, TEXT("Sobre"));
    AppendMenu(hSubMenu, MF_STRING, IDM_EXIT, TEXT("Sair"));
    AppendMenu(hMenu, MF_STRING | MF_POPUP, (UINT_PTR)hSubMenu, TEXT("Menu"));

    SetMenu(hWnd, hMenu);
}

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
    wcex.lpszMenuName = NULL;
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

    AdicionarMenu(hWnd); // Adiciona o menu à janela

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
    TCHAR buffer[16];

    switch (message) {
    case WM_CREATE:
        SetTimer(hWnd, 1, 1000, NULL); // Adicionar um temporizador que dispara a cada segundo

        // Adicionar controles para ajustar a escala e o número de empresas
        CreateWindow(TEXT("STATIC"), TEXT("Escala Mínima:"), WS_VISIBLE | WS_CHILD, 620, 20, 120, 20, hWnd, NULL, NULL, NULL);
        hScaleMin = CreateWindow(TEXT("EDIT"), TEXT("0"), WS_VISIBLE | WS_CHILD | WS_BORDER, 620, 40, 120, 20, hWnd, (HMENU)IDC_SCALEMIN, NULL, NULL);

        CreateWindow(TEXT("STATIC"), TEXT("Escala Máxima:"), WS_VISIBLE | WS_CHILD, 620, 70, 120, 20, hWnd, NULL, NULL, NULL);
        hScaleMax = CreateWindow(TEXT("EDIT"), TEXT("100"), WS_VISIBLE | WS_CHILD | WS_BORDER, 620, 90, 120, 20, hWnd, (HMENU)IDC_SCALEMAX, NULL, NULL);

        CreateWindow(TEXT("STATIC"), TEXT("Nº de Empresas:"), WS_VISIBLE | WS_CHILD, 620, 120, 120, 20, hWnd, NULL, NULL, NULL);
        hNumEmpresas = CreateWindow(TEXT("EDIT"), TEXT("10"), WS_VISIBLE | WS_CHILD | WS_BORDER, 620, 140, 120, 20, hWnd, (HMENU)IDC_NUMEMPRESAS, NULL, NULL);

        hUpdateButton = CreateWindow(TEXT("BUTTON"), TEXT("Atualizar"), WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON, 620, 170, 120, 30, hWnd, (HMENU)IDC_UPDATE, NULL, NULL);
        break;

    case WM_COMMAND: {
        int wmId = LOWORD(wParam);
        // Parse the menu selections:
        switch (wmId) {
        case IDC_UPDATE:
            GetWindowText(hScaleMin, buffer, 16);
            scaleMin = _tstof(buffer);
            GetWindowText(hScaleMax, buffer, 16);
            scaleMax = _tstof(buffer);
            GetWindowText(hNumEmpresas, buffer, 16);
            n = _ttoi(buffer);
            InvalidateRect(hWnd, NULL, TRUE); // Atualizar a janela
            break;
        case IDM_ABOUT:
            MessageBox(hWnd, _T("Grupo:\nTomás Ferreira - 2021130424\nPedro Paiva - 2021134625\nDisciplina: Sistemas Operativos\nAno: 2023/2024"), _T("Sobre"), MB_OK);
            break;
        case IDM_EXIT:
            if (MessageBox(hWnd, _T("Tem a certeza que deseja sair?"), _T("Confirmar Saída"), MB_OKCANCEL | MB_ICONQUESTION) == IDOK) {
                DestroyWindow(hWnd);
            }
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
        DrawLastTransaction(hdc, pSharedData); // Desenhar a última transação
        EndPaint(hWnd, &ps);
    }
                 break;

    case WM_TIMER:
        InvalidateRect(hWnd, NULL, TRUE); // Invalidate the window to trigger a repaint
        break;

    case WM_DESTROY:
        KillTimer(hWnd, 1); // Parar o temporizador quando a janela for destruída
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

void DrawGraph(HWND hWnd, SharedData* pSharedData, int n, double scaleMin, double scaleMax) {
    RECT rect;
    GetClientRect(hWnd, &rect);
    rect.right -= 170; // Ajustar a área de desenho para a esquerda para dar espaço aos controles
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

    ReleaseMutex(hMutex);
    ReleaseDC(hWnd, hdc);
}

void DrawLastTransaction(HDC hdc, SharedData* pSharedData) {
    // Set the font and color for the text
    HFONT hFont = CreateFont(24, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, VARIABLE_PITCH, TEXT("Arial"));
    HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);
    SetTextColor(hdc, RGB(0, 0, 255));
    SetBkMode(hdc, TRANSPARENT);

    TCHAR lastTransaction[256];
    _stprintf_s(lastTransaction, 256, _T("Última Transação:\nEmpresa: %s\nAções: %d\nValor: %.2f€"),
        pSharedData->ultimaTransacao.nomeEmpresa,
        pSharedData->ultimaTransacao.numAcoes,
        pSharedData->ultimaTransacao.valor);

    RECT rect;
    GetClientRect(WindowFromDC(hdc), &rect);
    rect.left += 10;
    rect.top += 10;
    DrawText(hdc, lastTransaction, -1, &rect, DT_LEFT | DT_WORDBREAK);

    // Cleanup
    SelectObject(hdc, hOldFont);
    DeleteObject(hFont);
}