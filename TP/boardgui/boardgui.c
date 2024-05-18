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

// Estrutura para armazenar o estado da aplicação
typedef struct {
    SharedData* pSharedData;
    HANDLE hMutex;
    HANDLE hEvent;
    HWND hScaleMin;
    HWND hScaleMax;
    HWND hNumEmpresas;
    HWND hUpdateButton;
    int n;
    double scaleMin;
    double scaleMax;
    TCHAR transactionType[10]; // Campo para armazenar o tipo de transação
} AppState;

// Declaração de funções
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
void DrawGraph(HWND hWnd, AppState* appState);
void DrawLastTransaction(HDC hdc, AppState* appState);
void AdicionarMenu(HWND hWnd);

int APIENTRY _tWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPTSTR lpCmdLine, int nCmdShow) {
    MSG msg;
    HWND hWnd;
    WNDCLASSEX wcex;
    AppState appState;

    appState.hMutex = OpenMutex(SYNCHRONIZE, FALSE, MUTEX_NAME);
    appState.hEvent = OpenEvent(EVENT_MODIFY_STATE | SYNCHRONIZE, FALSE, EVENT_NAME);

    if (appState.hMutex == NULL || appState.hEvent == NULL) {
        MessageBox(NULL, _T("Falha ao abrir o mutex ou evento."), _T("Erro"), MB_OK);
        return 1;
    }

    HANDLE hMapFile = OpenFileMapping(FILE_MAP_ALL_ACCESS, FALSE, SHARED_MEM_NAME);
    if (hMapFile == NULL) {
        MessageBox(NULL, _T("Falha ao abrir o mapeamento de arquivo."), _T("Erro"), MB_OK);
        return 1;
    }

    appState.pSharedData = (SharedData*)MapViewOfFile(hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(SharedData));
    if (appState.pSharedData == NULL) {
        MessageBox(NULL, _T("Falha ao mapear a memória compartilhada."), _T("Erro"), MB_OK);
        return 1;
    }

    appState.n = 10;
    appState.scaleMin = 0.0;
    appState.scaleMax = 100.0;
    _tcscpy_s(appState.transactionType, 10, _T("")); // Inicializa o campo transactionType com uma string vazia

    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = sizeof(AppState*);
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
        CW_USEDEFAULT, CW_USEDEFAULT, 800, 600, NULL, NULL, hInstance, &appState);

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

    CloseHandle(appState.hEvent);
    CloseHandle(appState.hMutex);
    UnmapViewOfFile(appState.pSharedData);
    CloseHandle(hMapFile);

    return (int)msg.wParam;
}

void AdicionarMenu(HWND hWnd) {
    HMENU hMenu = CreateMenu();
    HMENU hSubMenu = CreatePopupMenu();

    AppendMenu(hSubMenu, MF_STRING, IDM_ABOUT, TEXT("Sobre"));
    AppendMenu(hSubMenu, MF_STRING, IDM_EXIT, TEXT("Sair"));
    AppendMenu(hMenu, MF_STRING | MF_POPUP, (UINT_PTR)hSubMenu, TEXT("Menu"));

    SetMenu(hWnd, hMenu);
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    AppState* appState;
    if (message == WM_CREATE) {
        CREATESTRUCT* pCreate = (CREATESTRUCT*)lParam;
        appState = (AppState*)pCreate->lpCreateParams;
        SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)appState);
    }
    else {
        appState = (AppState*)GetWindowLongPtr(hWnd, GWLP_USERDATA);
    }

    TCHAR buffer[16];

    switch (message) {
    case WM_CREATE:
        SetTimer(hWnd, 1, 1000, NULL); // Adicionar um temporizador que dispara a cada segundo

        // Adicionar controles para ajustar a escala e o número de empresas
        CreateWindow(TEXT("STATIC"), TEXT("Escala Mínima:"), WS_VISIBLE | WS_CHILD, 620, 20, 120, 20, hWnd, NULL, NULL, NULL);
        appState->hScaleMin = CreateWindow(TEXT("EDIT"), TEXT("0"), WS_VISIBLE | WS_CHILD | WS_BORDER, 620, 40, 120, 20, hWnd, (HMENU)IDC_SCALEMIN, NULL, NULL);

        CreateWindow(TEXT("STATIC"), TEXT("Escala Máxima:"), WS_VISIBLE | WS_CHILD, 620, 70, 120, 20, hWnd, NULL, NULL, NULL);
        appState->hScaleMax = CreateWindow(TEXT("EDIT"), TEXT("100"), WS_VISIBLE | WS_CHILD | WS_BORDER, 620, 90, 120, 20, hWnd, (HMENU)IDC_SCALEMAX, NULL, NULL);

        CreateWindow(TEXT("STATIC"), TEXT("Nº de Empresas:"), WS_VISIBLE | WS_CHILD, 620, 120, 120, 20, hWnd, NULL, NULL, NULL);
        appState->hNumEmpresas = CreateWindow(TEXT("EDIT"), TEXT("10"), WS_VISIBLE | WS_CHILD | WS_BORDER, 620, 140, 120, 20, hWnd, (HMENU)IDC_NUMEMPRESAS, NULL, NULL);

        appState->hUpdateButton = CreateWindow(TEXT("BUTTON"), TEXT("Atualizar"), WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON, 620, 170, 120, 30, hWnd, (HMENU)IDC_UPDATE, NULL, NULL);
        break;

    case WM_COMMAND: {
        int wmId = LOWORD(wParam);
        // Parse the menu selections:
        switch (wmId) {
        case IDC_UPDATE:
            GetWindowText(appState->hScaleMin, buffer, 16);
            appState->scaleMin = _tstof(buffer);
            GetWindowText(appState->hScaleMax, buffer, 16);
            appState->scaleMax = _tstof(buffer);
            GetWindowText(appState->hNumEmpresas, buffer, 16);
            appState->n = _ttoi(buffer);
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
        DrawGraph(hWnd, appState);
        DrawLastTransaction(hdc, appState); // Desenhar a última transação
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

void DrawGraph(HWND hWnd, AppState* appState) {
    RECT rect;
    GetClientRect(hWnd, &rect);
    rect.right -= 170; // Ajustar a área de desenho para a esquerda para dar espaço aos controles
    int width = rect.right - rect.left;
    int height = rect.bottom - rect.top;
    HDC hdc = GetDC(hWnd);

    WaitForSingleObject(appState->hMutex, INFINITE);

    // Sort the companies by stock price in descending order
    for (int i = 0; i < appState->pSharedData->numEmpresas - 1; ++i) {
        for (int j = i + 1; j < appState->pSharedData->numEmpresas; ++j) {
            if (appState->pSharedData->empresas[j].precoAcao > appState->pSharedData->empresas[i].precoAcao) {
                Empresa temp = appState->pSharedData->empresas[i];
                appState->pSharedData->empresas[i] = appState->pSharedData->empresas[j];
                appState->pSharedData->empresas[j] = temp;
            }
        }
    }

    // Draw bars for the top N companies
    int barWidth = width / (appState->n * 2);
    for (int i = 0; i < appState->n && i < appState->pSharedData->numEmpresas; ++i) {
        int barHeight = (int)(((appState->pSharedData->empresas[i].precoAcao - appState->scaleMin) / (appState->scaleMax - appState->scaleMin)) * height);
        RECT barRect = { i * 2 * barWidth, height - barHeight, (i * 2 + 1) * barWidth, height };

        // Change the color of the bars to green
        HBRUSH hBrush = CreateSolidBrush(RGB(0, 128, 0));
        FillRect(hdc, &barRect, hBrush);
        DeleteObject(hBrush);

        // Draw the company name
        TextOut(hdc, i * 2 * barWidth, height - barHeight - 40, appState->pSharedData->empresas[i].nomeEmpresa, _tcslen(appState->pSharedData->empresas[i].nomeEmpresa));

        // Draw the stock price below the company name
        TCHAR price[16];
        _stprintf_s(price, 16, _T("%.2f"), appState->pSharedData->empresas[i].precoAcao);
        TextOut(hdc, i * 2 * barWidth, height - barHeight - 20, price, _tcslen(price));
    }

    ReleaseMutex(appState->hMutex);
    ReleaseDC(hWnd, hdc);
}

void DrawLastTransaction(HDC hdc, AppState* appState) {
    SharedData* pSharedData = appState->pSharedData;

    // Set the font and color for the text
    HFONT hFont = CreateFont(24, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, VARIABLE_PITCH, TEXT("Arial"));
    HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);
    SetTextColor(hdc, RGB(0, 0, 255));
    SetBkMode(hdc, TRANSPARENT);

    // Determine if the transaction was a purchase or sale
    if (pSharedData->ultimaTransacao.numAcoes > 0) {
        _tcscpy_s(appState->transactionType, 10, _T("Compra"));
    }
    else if (pSharedData->ultimaTransacao.numAcoes < 0) {
        _tcscpy_s(appState->transactionType, 10, _T("Venda"));
    }
    else {
        _tcscpy_s(appState->transactionType, 10, _T(""));
    }

    int numAcoes = abs(pSharedData->ultimaTransacao.numAcoes);

    TCHAR lastTransaction[256];
    _stprintf_s(lastTransaction, 256, _T("Última Transação: %s\nEmpresa: %s\nAções: %d\nValor: %.2f€"),
        appState->transactionType,
        pSharedData->ultimaTransacao.nomeEmpresa,
        numAcoes,
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
