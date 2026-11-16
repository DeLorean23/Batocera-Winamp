/*
 * delorean_start.exe - starts Winamp for Batocera inside its own Wine desktop
 * Created by DeLorean23
 *
 * The Wine desktop has the size of the screen. Inside it Winamp, the wallpaper
 * and the mouse pointer are stacked by Wine itself, so this works the same
 * with every Batocera display setup (X.org or Wayland).
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <wchar.h>

static void reg_dword(HKEY root, const wchar_t *key, const wchar_t *name, DWORD v) {
    HKEY k;
    if (RegCreateKeyExW(root, key, 0, NULL, 0, KEY_SET_VALUE, NULL, &k, NULL) == ERROR_SUCCESS) {
        RegSetValueExW(k, name, 0, REG_DWORD, (const BYTE *)&v, sizeof v);
        RegCloseKey(k);
    }
}

static void reg_str(HKEY root, const wchar_t *key, const wchar_t *name, const wchar_t *v) {
    HKEY k;
    if (RegCreateKeyExW(root, key, 0, NULL, 0, KEY_SET_VALUE, NULL, &k, NULL) == ERROR_SUCCESS) {
        RegSetValueExW(k, name, 0, REG_SZ, (const BYTE *)v, (DWORD)((wcslen(v) + 1) * sizeof(wchar_t)));
        RegCloseKey(k);
    }
}

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE prev, LPWSTR args, int show) {
    wchar_t dir[MAX_PATH], cmd[MAX_PATH * 2], *p;
    int w = GetSystemMetrics(SM_CXSCREEN), h = GetSystemMetrics(SM_CYSCREEN);
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    (void)inst; (void)prev; (void)args; (void)show;

    if (w < 640 || h < 480) { w = 1920; h = 1080; }

    /* no Wine taskbar / start button, black desktop behind everything */
    reg_dword(HKEY_CURRENT_USER, L"Software\\Wine\\Explorer\\Desktops", L"EnableShell", 0);
    reg_dword(HKEY_CURRENT_USER, L"Software\\Wine\\Explorer\\Desktops\\Winamp", L"EnableShell", 0);
    reg_str(HKEY_CURRENT_USER, L"Control Panel\\Colors", L"Background", L"0 0 0");

    GetModuleFileNameW(NULL, dir, MAX_PATH);
    p = wcsrchr(dir, L'\\');
    if (p) *p = 0;
    _snwprintf(cmd, MAX_PATH * 2, L"explorer.exe /desktop=Winamp,%dx%d \"%s\\winamp.exe\"", w, h, dir);

    memset(&si, 0, sizeof si);
    si.cb = sizeof si;
    if (!CreateProcessW(NULL, cmd, NULL, NULL, FALSE, 0, NULL, dir, &si, &pi)) return 1;
    WaitForSingleObject(pi.hProcess, INFINITE);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return 0;
}
