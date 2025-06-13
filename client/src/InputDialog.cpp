
#include "InputDialog.h"

namespace {
    // IDs for controls
    constexpr int IDC_EDT = 1001;
    constexpr int IDC_OK = IDOK;
    constexpr int IDC_CANCEL = IDCANCEL;

    // Stored result
    static std::string g_result;

    // Window-proc for our popup
    LRESULT CALLBACK DialogProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        static HWND hEdit;

        switch (msg) {
        case WM_CREATE:
{
    // figure out how big our client area actually is
    RECT rc;
    GetClientRect(hWnd, &rc);
    int dlgW = rc.right  - rc.left;
    int dlgH = rc.bottom - rc.top;

    // choose some margins
    const int margin = 10;
    const int editH = 20;
    const int btnW  = 60;
    const int btnH  = 24;

    // width of the edit control = dialog width minus left/right margins
    int editW = dlgW - 2*margin;

    // create the EDIT
    hEdit = CreateWindowEx(
        0, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
        margin, margin,    // x, y
        editW, editH,      // width, height
        hWnd, (HMENU)IDC_EDT,
        ((LPCREATESTRUCT)lParam)->hInstance,
        NULL
    );

    // create OK
    CreateWindow(
        L"BUTTON", L"OK",
        WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
        margin, margin + editH + margin,  // just below the edit
        btnW, btnH,
        hWnd, (HMENU)IDC_OK,
        ((LPCREATESTRUCT)lParam)->hInstance,
        NULL
    );

    // create Cancel
    CreateWindow(
        L"BUTTON", L"Local",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        dlgW - margin - btnW, margin + editH + margin, 
        btnW, btnH,
        hWnd, (HMENU)IDC_CANCEL,
        ((LPCREATESTRUCT)lParam)->hInstance,
        NULL
    );

    return 0;
}
        case WM_COMMAND:
            if (LOWORD(wParam) == IDC_OK) {
                wchar_t buf[256] = {};
                GetWindowText(hEdit, buf, _countof(buf));
				std::wstring wstr(buf);
                g_result.assign(wstr.begin(), wstr.end());               // store result
                DestroyWindow(hWnd);
                PostQuitMessage(0);
                return 0;
            }
            else if (LOWORD(wParam) == IDC_CANCEL) {
                g_result = "127.0.0.1";
                DestroyWindow(hWnd);
                PostQuitMessage(0);
                return 0;
            }
            break;

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        }
        return DefWindowProc(hWnd, msg, wParam, lParam);
    }
}

std::string ShowInputDialog(HINSTANCE hInst,
    const std::wstring& title,
    int width,
    int height)
{
    // 1) Register a unique window class (you can cache this so it only happens once)
    WNDCLASS wc = {};
    wc.lpfnWndProc = DialogProc;
    wc.hInstance = hInst;
    wc.lpszClassName = L"CustomInputDlg";
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    RegisterClass(&wc);

    // 2) Reset result
    g_result.clear();

    // 3) Create window
    HWND hWnd = CreateWindowEx(
        0,
        wc.lpszClassName,
        title.c_str(),
        (WS_OVERLAPPEDWINDOW & ~WS_SIZEBOX & ~WS_MAXIMIZEBOX),
        CW_USEDEFAULT, CW_USEDEFAULT,
        width, height,
        nullptr, nullptr,
        hInst, nullptr
    );
    ShowWindow(hWnd, SW_SHOW);

    // 4) Message loop
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // 5) Return whatever the user entered
    return g_result;
}
