/* kb_lay — convert selected text between EN/RU keyboard layouts.
 *
 * Tiny user-session background process (not a Session-0 Windows service:
 * services cannot receive desktop hotkeys or use the interactive clipboard).
 *
 * Compile as UTF-8: MSVC /utf-8, gcc -finput-charset=UTF-8
 */
#define WINVER 0x0601
#define _WIN32_WINNT 0x0601
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#ifdef KB_LAY_TEST
#include <stdio.h>
#endif

/* US QWERTY <-> Windows Russian (JCUKEN), including shifted row. */
static const wchar_t EN[] =
    L"`1234567890-="
    L"qwertyuiop[]\\"
    L"asdfghjkl;'"
    L"zxcvbnm,./"
    L"~!@#$%^&*()_+"
    L"QWERTYUIOP{}|"
    L"ASDFGHJKL:\""
    L"ZXCVBNM<>?";

static const wchar_t RU[] =
    L"ё1234567890-="
    L"йцукенгшщзхъ\\"
    L"фывапролджэ"
    L"ячсмитьбю."
    L"Ё!\"№;%:?*()_+"
    L"ЙЦУКЕНГШЩЗХЪ/"
    L"ФЫВАПРОЛДЖЭ"
    L"ЯЧСМИТЬБЮ,";

_Static_assert(sizeof(EN) == sizeof(RU), "EN/RU keymap length mismatch");

static int is_lat(wchar_t c)
{
    return (c >= L'A' && c <= L'Z') || (c >= L'a' && c <= L'z');
}

static int is_cyr(wchar_t c)
{
    return c >= 0x0400 && c <= 0x04FF;
}

/* 1 = EN->RU, 0 = RU->EN, -1 = nothing to do. */
static int decide_to_ru(const wchar_t *s)
{
    int lat = 0, cyr = 0;
    for (; *s; s++) {
        if (is_lat(*s))
            lat++;
        else if (is_cyr(*s))
            cyr++;
    }
    if (lat == 0 && cyr == 0)
        return -1;
    return lat >= cyr;
}

static wchar_t map_one(wchar_t c, int to_ru)
{
    const wchar_t *from = to_ru ? EN : RU;
    const wchar_t *to = to_ru ? RU : EN;
    const wchar_t *p = wcschr(from, c);
    if (!p)
        return c;
    return to[p - from];
}

/* In-place. Returns 1 if any character changed. */
static int convert_inplace(wchar_t *s)
{
    int to_ru = decide_to_ru(s);
    int changed = 0;
    if (to_ru < 0)
        return 0;
    for (; *s; s++) {
        wchar_t n = map_one(*s, to_ru);
        if (n != *s) {
            *s = n;
            changed = 1;
        }
    }
    return changed;
}

#ifdef KB_LAY_TEST

static int g_fail;

static void expect(const wchar_t *in, const wchar_t *want, const char *tag)
{
    wchar_t buf[256];
    size_t n = wcslen(in);
    if (n >= 256) {
        fprintf(stderr, "FAIL %s: input too long\n", tag);
        g_fail++;
        return;
    }
    memcpy(buf, in, (n + 1) * sizeof(wchar_t));
    convert_inplace(buf);
    if (wcscmp(buf, want) != 0) {
        fprintf(stderr, "FAIL %s\n", tag);
        g_fail++;
    }
}

int main(void)
{
    if (wcslen(EN) != wcslen(RU)) {
        fprintf(stderr, "FAIL keymap wcslen\n");
        return 1;
    }

    expect(L"ghbdtn", L"привет", "ghbdtn");
    expect(L"привет", L"ghbdtn", "привет");
    expect(L"Hello", L"Руддщ", "Hello");
    expect(L"Руддщ", L"Hello", "Руддщ");
    expect(L"Ghbdtn", L"Привет", "Ghbdtn");
    expect(L"q", L"й", "q");
    expect(L"й", L"q", "й");
    expect(L"123", L"123", "digits");
    expect(L"", L"", "empty");
    expect(L"ok 42", L"щл 42", "lat+digits");
    expect(L"QWERTY", L"ЙЦУКЕН", "QWERTY");

    if (g_fail) {
        fprintf(stderr, "%d test(s) failed\n", g_fail);
        return 1;
    }
    puts("ok");
    return 0;
}

#else /* !KB_LAY_TEST */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>

#define CLASS_NAME L"kb_lay.Hidden"
#define MUTEX_NAME L"Local\\kb_lay.instance"
#define RUN_VALUE  L"kb_lay"
#define WM_TRAY    (WM_APP + 1)
#define ID_HOTKEY  1
#define IDM_INSTALL   10
#define IDM_UNINSTALL 11
#define IDM_EXIT      12
#define MAX_CONVERT   1000000

static HINSTANCE g_inst;
static HWND g_hwnd;
static UINT g_mods = MOD_NOREPEAT;
static UINT g_vk = VK_PAUSE;
static wchar_t g_hotkey_spec[64] = L"pause";
static int g_busy;

static void wcopy(wchar_t *dst, size_t cap, const wchar_t *src)
{
    size_t n = wcslen(src);
    if (n >= cap)
        n = cap - 1;
    memcpy(dst, src, n * sizeof(wchar_t));
    dst[n] = 0;
}

static void wcat(wchar_t *dst, size_t cap, const wchar_t *src)
{
    size_t n = wcslen(dst);
    size_t m = wcslen(src);
    if (n + m + 1 > cap)
        return;
    memcpy(dst + n, src, (m + 1) * sizeof(wchar_t));
}

static void to_lower_ascii(wchar_t *s)
{
    for (; *s; s++) {
        if (*s >= L'A' && *s <= L'Z')
            *s = (wchar_t)(*s - L'A' + L'a');
    }
}

static int parse_vk_name(const wchar_t *tok, UINT *vk)
{
    unsigned n;
    const wchar_t *d;
    if (!wcscmp(tok, L"pause") || !wcscmp(tok, L"break")) {
        *vk = VK_PAUSE;
        return 1;
    }
    if (!wcscmp(tok, L"scrolllock") || !wcscmp(tok, L"scroll")) {
        *vk = VK_SCROLL;
        return 1;
    }
    if (!wcscmp(tok, L"capslock") || !wcscmp(tok, L"caps")) {
        *vk = VK_CAPITAL;
        return 1;
    }
    if (tok[0] == L'f' && tok[1]) {
        n = 0;
        for (d = tok + 1; *d >= L'0' && *d <= L'9'; d++)
            n = n * 10u + (unsigned)(*d - L'0');
        if (*d || n < 1 || n > 24)
            return 0;
        *vk = VK_F1 + n - 1;
        return 1;
    }
    if (!tok[1] && tok[0] >= L'a' && tok[0] <= L'z') {
        *vk = (UINT)(tok[0] - L'a' + L'A');
        return 1;
    }
    if (!tok[1] && tok[0] >= L'0' && tok[0] <= L'9') {
        *vk = (UINT)tok[0];
        return 1;
    }
    return 0;
}

static int parse_hotkey(const wchar_t *spec, UINT *mods, UINT *vk)
{
    wchar_t buf[64];
    wchar_t *tok, *plus, *end;
    wcopy(buf, 64, spec);
    to_lower_ascii(buf);
    *mods = MOD_NOREPEAT;
    *vk = 0;
    tok = buf;
    while (tok && *tok) {
        plus = wcschr(tok, L'+');
        if (plus)
            *plus = 0;
        while (*tok == L' ')
            tok++;
        end = tok + wcslen(tok);
        while (end > tok && end[-1] == L' ')
            *--end = 0;
        if (!wcscmp(tok, L"ctrl") || !wcscmp(tok, L"control"))
            *mods |= MOD_CONTROL;
        else if (!wcscmp(tok, L"alt"))
            *mods |= MOD_ALT;
        else if (!wcscmp(tok, L"shift"))
            *mods |= MOD_SHIFT;
        else if (!wcscmp(tok, L"win") || !wcscmp(tok, L"windows"))
            *mods |= MOD_WIN;
        else if (!parse_vk_name(tok, vk))
            return 0;
        tok = plus ? plus + 1 : NULL;
    }
    return *vk != 0;
}

static void key_batch_ctrl(WORD vk)
{
    INPUT in[4];
    memset(in, 0, sizeof(in));
    in[0].type = INPUT_KEYBOARD;
    in[0].ki.wVk = VK_CONTROL;
    in[0].ki.wScan = (WORD)MapVirtualKeyW(VK_CONTROL, MAPVK_VK_TO_VSC);
    in[1].type = INPUT_KEYBOARD;
    in[1].ki.wVk = vk;
    in[1].ki.wScan = (WORD)MapVirtualKeyW(vk, MAPVK_VK_TO_VSC);
    in[2] = in[1];
    in[2].ki.dwFlags = KEYEVENTF_KEYUP;
    in[3] = in[0];
    in[3].ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(4, in, sizeof(INPUT));
}

static int clip_open(void)
{
    int i;
    for (i = 0; i < 25; i++) {
        if (OpenClipboard(g_hwnd))
            return 1;
        Sleep(8);
    }
    return 0;
}

static wchar_t *clip_dup_opened(void)
{
    HANDLE h = GetClipboardData(CF_UNICODETEXT);
    const wchar_t *p;
    size_t n;
    wchar_t *out;
    if (h) {
        p = GlobalLock(h);
        if (!p)
            return NULL;
        n = wcslen(p);
        out = NULL;
        if (n > 0 && n <= MAX_CONVERT) {
            out = (wchar_t *)malloc((n + 1) * sizeof(wchar_t));
            if (out)
                memcpy(out, p, (n + 1) * sizeof(wchar_t));
        }
        GlobalUnlock(h);
        return out;
    }
    h = GetClipboardData(CF_TEXT);
    if (h) {
        const char *a = (const char *)GlobalLock(h);
        int w;
        if (!a)
            return NULL;
        w = MultiByteToWideChar(CP_ACP, 0, a, -1, NULL, 0);
        out = NULL;
        if (w > 1 && w <= MAX_CONVERT + 1) {
            out = (wchar_t *)malloc((size_t)w * sizeof(wchar_t));
            if (out)
                MultiByteToWideChar(CP_ACP, 0, a, -1, out, w);
        }
        GlobalUnlock(h);
        return out;
    }
    return NULL;
}

static wchar_t *clip_get(void)
{
    wchar_t *s;
    if (!clip_open())
        return NULL;
    s = clip_dup_opened();
    CloseClipboard();
    return s;
}

static int clip_set(const wchar_t *s)
{
    size_t n = wcslen(s);
    HGLOBAL h;
    wchar_t *p;
    if (n > MAX_CONVERT)
        return 0;
    h = GlobalAlloc(GMEM_MOVEABLE, (n + 1) * sizeof(wchar_t));
    if (!h)
        return 0;
    p = (wchar_t *)GlobalLock(h);
    if (!p) {
        GlobalFree(h);
        return 0;
    }
    memcpy(p, s, (n + 1) * sizeof(wchar_t));
    GlobalUnlock(h);
    if (!clip_open()) {
        GlobalFree(h);
        return 0;
    }
    EmptyClipboard();
    if (!SetClipboardData(CF_UNICODETEXT, h)) {
        GlobalFree(h);
        CloseClipboard();
        return 0;
    }
    CloseClipboard();
    return 1;
}

static void set_layout(int ru)
{
    HKL hkl = LoadKeyboardLayoutW(ru ? L"00000419" : L"00000409", KLF_SUBSTITUTE_OK);
    HWND fg = GetForegroundWindow();
    if (hkl && fg)
        PostMessageW(fg, WM_INPUTLANGCHANGEREQUEST, 0, (LPARAM)hkl);
}

static void convert_selection(void)
{
    DWORD seq;
    wchar_t *cur = NULL;
    int i, to_ru;
    HWND fg;

    if (g_busy)
        return;
    g_busy = 1;

    fg = GetForegroundWindow();
    if (fg)
        SetForegroundWindow(fg);

    seq = GetClipboardSequenceNumber();
    key_batch_ctrl('C');

    for (i = 0; i < 40; i++) {
        Sleep(15);
        if (GetClipboardSequenceNumber() != seq)
            break;
    }
    if (GetClipboardSequenceNumber() == seq) {
        g_busy = 0;
        return;
    }

    cur = clip_get();
    if (!cur || !cur[0]) {
        free(cur);
        g_busy = 0;
        return;
    }

    to_ru = decide_to_ru(cur);
    if (to_ru < 0 || !convert_inplace(cur) || !clip_set(cur)) {
        free(cur);
        g_busy = 0;
        return;
    }

    key_batch_ctrl('V');
    set_layout(to_ru);

    free(cur);
    g_busy = 0;
}

static HWND find_instance(void)
{
    return FindWindowW(CLASS_NAME, NULL);
}

static void quit_instance(void)
{
    HWND w = find_instance();
    if (w) {
        PostMessageW(w, WM_CLOSE, 0, 0);
        Sleep(150);
    }
}

static int install_dir(wchar_t *dir, size_t cap, wchar_t *exe, size_t ecap)
{
    DWORD n = GetEnvironmentVariableW(L"LOCALAPPDATA", dir, (DWORD)cap);
    if (n == 0 || n >= cap)
        return 0;
    wcat(dir, cap, L"\\kb_lay");
    wcopy(exe, ecap, dir);
    wcat(exe, ecap, L"\\kb_lay.exe");
    return 1;
}

static void build_run_cmd(wchar_t *cmd, size_t cap, const wchar_t *exe)
{
    wcopy(cmd, cap, L"\"");
    wcat(cmd, cap, exe);
    wcat(cmd, cap, L"\"");
    if (g_hotkey_spec[0] && wcscmp(g_hotkey_spec, L"pause") != 0) {
        wcat(cmd, cap, L" --hotkey=");
        wcat(cmd, cap, g_hotkey_spec);
    }
}

static int write_run_key(const wchar_t *cmd)
{
    HKEY k;
    LONG e = RegCreateKeyExW(
        HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
        0, NULL, 0, KEY_SET_VALUE, NULL, &k, NULL);
    if (e != ERROR_SUCCESS)
        return 0;
    e = RegSetValueExW(
        k, RUN_VALUE, 0, REG_SZ,
        (const BYTE *)cmd, (DWORD)((wcslen(cmd) + 1) * sizeof(wchar_t)));
    RegCloseKey(k);
    return e == ERROR_SUCCESS;
}

static void do_install(void)
{
    wchar_t src[MAX_PATH], dir[MAX_PATH], dest[MAX_PATH], cmd[MAX_PATH + 80];
    DWORD n;
    int same;

    n = GetModuleFileNameW(NULL, src, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) {
        MessageBoxW(NULL, L"Cannot resolve own path.", L"kb_lay", MB_ICONERROR);
        return;
    }
    if (!install_dir(dir, MAX_PATH, dest, MAX_PATH)) {
        MessageBoxW(NULL, L"LOCALAPPDATA not set.", L"kb_lay", MB_ICONERROR);
        return;
    }
    CreateDirectoryW(dir, NULL);
    same = lstrcmpiW(src, dest) == 0;
    if (!same && !CopyFileW(src, dest, FALSE)) {
        MessageBoxW(NULL, L"Failed to copy kb_lay.exe to LocalAppData.", L"kb_lay", MB_ICONERROR);
        return;
    }
    build_run_cmd(cmd, MAX_PATH + 80, dest);
    if (!write_run_key(cmd)) {
        MessageBoxW(NULL, L"Failed to write HKCU Run key.", L"kb_lay", MB_ICONERROR);
        return;
    }
    if (!same) {
        STARTUPINFOW si;
        PROCESS_INFORMATION pi;
        quit_instance();
        memset(&si, 0, sizeof(si));
        si.cb = sizeof(si);
        if (CreateProcessW(NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
            CloseHandle(pi.hThread);
            CloseHandle(pi.hProcess);
        }
    }
    MessageBoxW(
        NULL,
        L"Installed. Select text and press the hotkey (default: Pause) to convert EN/RU.",
        L"kb_lay",
        MB_OK | MB_ICONINFORMATION);
}

static void do_uninstall(void)
{
    wchar_t dir[MAX_PATH], dest[MAX_PATH];
    HKEY k;
    if (RegOpenKeyExW(
            HKEY_CURRENT_USER,
            L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
            0, KEY_SET_VALUE, &k) == ERROR_SUCCESS) {
        RegDeleteValueW(k, RUN_VALUE);
        RegCloseKey(k);
    }
    quit_instance();
    if (install_dir(dir, MAX_PATH, dest, MAX_PATH)) {
        DeleteFileW(dest);
        RemoveDirectoryW(dir);
    }
    MessageBoxW(NULL, L"Removed from startup.", L"kb_lay", MB_OK | MB_ICONINFORMATION);
}

static void show_help(void)
{
    MessageBoxW(
        NULL,
        L"kb_lay — convert selected text EN/RU (Windows US <-> Russian).\r\n\r\n"
        L"Select text, press Pause (default).\r\n\r\n"
        L"kb_lay\r\n"
        L"kb_lay --install [--hotkey=ctrl+alt+q]\r\n"
        L"kb_lay --uninstall\r\n"
        L"kb_lay --quit\r\n"
        L"kb_lay --hotkey=pause|scrolllock|f12|ctrl+shift+q\r\n",
        L"kb_lay",
        MB_OK | MB_ICONINFORMATION);
}

static void tray_add(void)
{
    NOTIFYICONDATAW nid;
    memset(&nid, 0, sizeof(nid));
    nid.cbSize = sizeof(nid);
    nid.hWnd = g_hwnd;
    nid.uID = 1;
    nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    nid.uCallbackMessage = WM_TRAY;
    nid.hIcon = LoadIconW(NULL, MAKEINTRESOURCEW(32512));
    wcopy(nid.szTip, 128, L"kb_lay  [");
    wcat(nid.szTip, 128, g_hotkey_spec);
    wcat(nid.szTip, 128, L"]  convert selection EN/RU");
    Shell_NotifyIconW(NIM_ADD, &nid);
}

static void tray_del(void)
{
    NOTIFYICONDATAW nid;
    memset(&nid, 0, sizeof(nid));
    nid.cbSize = sizeof(nid);
    nid.hWnd = g_hwnd;
    nid.uID = 1;
    Shell_NotifyIconW(NIM_DELETE, &nid);
}

static void show_menu(void)
{
    POINT pt;
    HMENU m = CreatePopupMenu();
    if (!m)
        return;
    GetCursorPos(&pt);
    AppendMenuW(m, MF_STRING, IDM_INSTALL, L"Install at startup");
    AppendMenuW(m, MF_STRING, IDM_UNINSTALL, L"Uninstall");
    AppendMenuW(m, MF_SEPARATOR, 0, NULL);
    AppendMenuW(m, MF_STRING, IDM_EXIT, L"Exit");
    SetForegroundWindow(g_hwnd);
    TrackPopupMenu(m, TPM_RIGHTBUTTON, pt.x, pt.y, 0, g_hwnd, NULL);
    DestroyMenu(m);
    PostMessageW(g_hwnd, WM_NULL, 0, 0);
}

static LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_HOTKEY:
        if (wp == ID_HOTKEY)
            convert_selection();
        return 0;
    case WM_TRAY:
        if (lp == WM_RBUTTONUP || lp == WM_CONTEXTMENU)
            show_menu();
        return 0;
    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case IDM_INSTALL:
            do_install();
            break;
        case IDM_UNINSTALL:
            do_uninstall();
            break;
        case IDM_EXIT:
            DestroyWindow(hwnd);
            break;
        }
        return 0;
    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        UnregisterHotKey(hwnd, ID_HOTKEY);
        tray_del();
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcW(hwnd, msg, wp, lp);
    }
}

static int run_resident(void)
{
    WNDCLASSEXW wc;
    MSG msg;
    HANDLE mux;
    wchar_t err[160];

    mux = CreateMutexW(NULL, TRUE, MUTEX_NAME);
    if (mux && GetLastError() == ERROR_ALREADY_EXISTS) {
        CloseHandle(mux);
        return 0;
    }

    memset(&wc, 0, sizeof(wc));
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = wnd_proc;
    wc.hInstance = g_inst;
    wc.lpszClassName = CLASS_NAME;
    if (!RegisterClassExW(&wc))
        return 1;

    g_hwnd = CreateWindowExW(
        0, CLASS_NAME, L"kb_lay", WS_OVERLAPPED,
        0, 0, 0, 0, HWND_MESSAGE, NULL, g_inst, NULL);
    if (!g_hwnd)
        return 1;

    if (!RegisterHotKey(g_hwnd, ID_HOTKEY, g_mods, g_vk)) {
        wcopy(err, 160, L"Hotkey '");
        wcat(err, 160, g_hotkey_spec);
        wcat(err, 160, L"' is already in use. Try --hotkey=scrolllock");
        MessageBoxW(NULL, err, L"kb_lay", MB_ICONERROR);
        DestroyWindow(g_hwnd);
        return 1;
    }

    tray_add();

    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    if (mux)
        CloseHandle(mux);
    return 0;
}

int WINAPI WinMain(HINSTANCE inst, HINSTANCE prev, LPSTR raw, int show)
{
    int argc = 0, i;
    LPWSTR *argv;
    int do_install_ = 0, do_uninst = 0, do_quit = 0, do_help = 0;

    (void)prev;
    (void)raw;
    (void)show;
    g_inst = inst;

    argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv) {
        for (i = 1; i < argc; i++) {
            if (!lstrcmpiW(argv[i], L"--install") || !lstrcmpiW(argv[i], L"/install"))
                do_install_ = 1;
            else if (!lstrcmpiW(argv[i], L"--uninstall") || !lstrcmpiW(argv[i], L"/uninstall"))
                do_uninst = 1;
            else if (!lstrcmpiW(argv[i], L"--quit") || !lstrcmpiW(argv[i], L"/quit"))
                do_quit = 1;
            else if (!lstrcmpiW(argv[i], L"--help") || !lstrcmpiW(argv[i], L"-h") ||
                     !lstrcmpiW(argv[i], L"/?"))
                do_help = 1;
            else if (!wcsncmp(argv[i], L"--hotkey=", 9)) {
                wcopy(g_hotkey_spec, 64, argv[i] + 9);
                if (!parse_hotkey(g_hotkey_spec, &g_mods, &g_vk)) {
                    MessageBoxW(NULL, L"Invalid --hotkey", L"kb_lay", MB_ICONERROR);
                    LocalFree(argv);
                    return 1;
                }
            }
        }
        LocalFree(argv);
    }

    if (do_help) {
        show_help();
        return 0;
    }
    if (do_quit) {
        quit_instance();
        return 0;
    }
    if (do_uninst) {
        do_uninstall();
        return 0;
    }
    if (do_install_) {
        do_install();
        return 0;
    }
    return run_resident();
}

#endif /* !KB_LAY_TEST */
