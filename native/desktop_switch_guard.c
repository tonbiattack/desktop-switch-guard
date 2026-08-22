#include <windows.h>
#include <shellapi.h>
#include <stdbool.h>

#define APP_CLASS_NAME L"DesktopSwitchGuardWindow"
#define APP_TITLE L"Desktop Switch Guard"
#define TRAY_ICON_ID 1
#define WM_TRAYICON (WM_APP + 1)
#define IDM_TOGGLE_LOCK 1001
#define IDM_EXIT 1002

static HINSTANCE g_instance = NULL;
static HWND g_window = NULL;
static HHOOK g_keyboard_hook = NULL;
static bool g_lock_enabled = true;
static bool g_suppressed_keys[256] = { false };

static bool IsVirtualDesktopTrigger(DWORD virtual_key)
{
    return virtual_key == VK_LEFT ||
           virtual_key == VK_RIGHT ||
           virtual_key == 'D' ||
           virtual_key == VK_F4;
}

static bool IsKeyDown(int virtual_key)
{
    return (GetAsyncKeyState(virtual_key) & 0x8000) != 0;
}

static bool ShouldBlockVirtualDesktopShortcut(DWORD virtual_key)
{
    const bool control_is_down = IsKeyDown(VK_LCONTROL) || IsKeyDown(VK_RCONTROL);
    const bool windows_is_down = IsKeyDown(VK_LWIN) || IsKeyDown(VK_RWIN);

    return IsVirtualDesktopTrigger(virtual_key) && control_is_down && windows_is_down;
}

static void UpdateTrayIcon(void)
{
    NOTIFYICONDATAW notification_data = { 0 };
    notification_data.cbSize = sizeof(notification_data);
    notification_data.hWnd = g_window;
    notification_data.uID = TRAY_ICON_ID;
    notification_data.uFlags = NIF_ICON | NIF_TIP;
    notification_data.hIcon = LoadIconW(NULL, IDI_APPLICATION);

    lstrcpynW(
        notification_data.szTip,
        g_lock_enabled ? L"Desktop Switch Guard — ロック中" : L"Desktop Switch Guard — 解除中",
        ARRAYSIZE(notification_data.szTip));

    Shell_NotifyIconW(NIM_MODIFY, &notification_data);
}

static void SetLockEnabled(bool enabled)
{
    g_lock_enabled = enabled;
    UpdateTrayIcon();
}

static void ShowTrayMenu(void)
{
    POINT cursor_position;
    GetCursorPos(&cursor_position);

    HMENU menu = CreatePopupMenu();
    if (menu == NULL)
    {
        return;
    }

    AppendMenuW(
        menu,
        MF_STRING,
        IDM_TOGGLE_LOCK,
        g_lock_enabled ? L"仮想デスクトップ切替のロックを解除(&L)" : L"仮想デスクトップ切替をロックする(&L)");
    AppendMenuW(menu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(menu, MF_STRING, IDM_EXIT, L"終了(&X)");

    SetForegroundWindow(g_window);
    TrackPopupMenu(
        menu,
        TPM_BOTTOMALIGN | TPM_LEFTALIGN | TPM_RIGHTBUTTON,
        cursor_position.x,
        cursor_position.y,
        0,
        g_window,
        NULL);
    DestroyMenu(menu);
}

static void AddTrayIcon(void)
{
    NOTIFYICONDATAW notification_data = { 0 };
    notification_data.cbSize = sizeof(notification_data);
    notification_data.hWnd = g_window;
    notification_data.uID = TRAY_ICON_ID;
    notification_data.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    notification_data.uCallbackMessage = WM_TRAYICON;
    notification_data.hIcon = LoadIconW(NULL, IDI_APPLICATION);
    lstrcpynW(notification_data.szTip, L"Desktop Switch Guard — ロック中", ARRAYSIZE(notification_data.szTip));

    Shell_NotifyIconW(NIM_ADD, &notification_data);
}

static void RemoveTrayIcon(void)
{
    NOTIFYICONDATAW notification_data = { 0 };
    notification_data.cbSize = sizeof(notification_data);
    notification_data.hWnd = g_window;
    notification_data.uID = TRAY_ICON_ID;

    Shell_NotifyIconW(NIM_DELETE, &notification_data);
}

static LRESULT CALLBACK KeyboardHookProcedure(int code, WPARAM message, LPARAM data)
{
    if (code == HC_ACTION)
    {
        const KBDLLHOOKSTRUCT *keyboard_data = (const KBDLLHOOKSTRUCT *)data;
        const DWORD virtual_key = keyboard_data->vkCode;

        if (message == WM_KEYDOWN || message == WM_SYSKEYDOWN)
        {
            if (g_lock_enabled && ShouldBlockVirtualDesktopShortcut(virtual_key))
            {
                if (virtual_key < ARRAYSIZE(g_suppressed_keys))
                {
                    g_suppressed_keys[virtual_key] = true;
                }
                return 1;
            }
        }
        else if (message == WM_KEYUP || message == WM_SYSKEYUP)
        {
            if (virtual_key < ARRAYSIZE(g_suppressed_keys) && g_suppressed_keys[virtual_key])
            {
                g_suppressed_keys[virtual_key] = false;
                return 1;
            }
        }
    }

    return CallNextHookEx(g_keyboard_hook, code, message, data);
}

static LRESULT CALLBACK WindowProcedure(HWND window, UINT message, WPARAM w_param, LPARAM l_param)
{
    switch (message)
    {
        case WM_TRAYICON:
            if (l_param == WM_RBUTTONUP || l_param == WM_CONTEXTMENU)
            {
                ShowTrayMenu();
                return 0;
            }
            if (l_param == WM_LBUTTONUP)
            {
                SetLockEnabled(!g_lock_enabled);
                return 0;
            }
            break;

        case WM_COMMAND:
            switch (LOWORD(w_param))
            {
                case IDM_TOGGLE_LOCK:
                    SetLockEnabled(!g_lock_enabled);
                    return 0;

                case IDM_EXIT:
                    DestroyWindow(window);
                    return 0;
            }
            break;

        case WM_DESTROY:
            if (g_keyboard_hook != NULL)
            {
                UnhookWindowsHookEx(g_keyboard_hook);
                g_keyboard_hook = NULL;
            }
            RemoveTrayIcon();
            PostQuitMessage(0);
            return 0;
    }

    return DefWindowProcW(window, message, w_param, l_param);
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE previous_instance, PWSTR command_line, int command_show)
{
    (void)previous_instance;
    (void)command_line;
    (void)command_show;

    g_instance = instance;

    WNDCLASSW window_class = { 0 };
    window_class.lpfnWndProc = WindowProcedure;
    window_class.hInstance = g_instance;
    window_class.lpszClassName = APP_CLASS_NAME;
    window_class.hCursor = LoadCursorW(NULL, IDC_ARROW);

    if (RegisterClassW(&window_class) == 0)
    {
        MessageBoxW(NULL, L"アプリケーションの初期化に失敗しました。", APP_TITLE, MB_OK | MB_ICONERROR);
        return 1;
    }

    g_window = CreateWindowExW(
        WS_EX_TOOLWINDOW,
        APP_CLASS_NAME,
        APP_TITLE,
        WS_OVERLAPPED,
        0,
        0,
        0,
        0,
        NULL,
        NULL,
        g_instance,
        NULL);

    if (g_window == NULL)
    {
        MessageBoxW(NULL, L"常駐ウィンドウを作成できませんでした。", APP_TITLE, MB_OK | MB_ICONERROR);
        return 1;
    }

    g_keyboard_hook = SetWindowsHookExW(WH_KEYBOARD_LL, KeyboardHookProcedure, g_instance, 0);
    if (g_keyboard_hook == NULL)
    {
        MessageBoxW(NULL, L"キーボード監視を開始できませんでした。", APP_TITLE, MB_OK | MB_ICONERROR);
        DestroyWindow(g_window);
        return 1;
    }

    AddTrayIcon();

    MSG message;
    while (GetMessageW(&message, NULL, 0, 0) > 0)
    {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    return 0;
}
