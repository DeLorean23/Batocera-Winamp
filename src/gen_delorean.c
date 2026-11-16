/*
 * gen_delorean.dll - Winamp general purpose plugin
 * Controller support for Winamp on Batocera + loads the music folder.
 * Created by DeLorean23
 *
 * Mouse mode (on at start, L3 switches it on/off):
 *   Left stick   Move the pointer (D-pad: fine steps)
 *   A            Left click (hold = drag)
 *   B            Right click
 *   X            Double click
 *   Right stick  Scroll
 *   A real mouse works at the same time.
 *
 * Wallpaper: roms/winamp/wallpaper (config.ini there)
 *
 * Controller (Xbox layout):
 *   A            Play / Pause
 *   B            Stop
 *   X            Shuffle on/off
 *   Y            Milkdrop on/off
 *   LB / RB      Previous / next track
 *   LT / RT      Rewind / forward 5 seconds
 *   D-pad up/dn  Volume up / down
 *   D-pad l/r    Milkdrop: previous / next preset (Milkdrop off: rewind / forward)
 *   Start        Milkdrop fullscreen on/off
 *   Select       Next skin
 *   R3           Repeat on/off
 *   L3           Mouse mode on/off
 *   Right stick up  Milkdrop: hard cut to another preset
 *   Select+Start Exit Winamp
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#define GPPHDR_VER 0x10
typedef struct {
    int version;
    char *description;
    int (*init)(void);
    void (*config)(void);
    void (*quit)(void);
    HWND hwndParent;
    HINSTANCE hDllInstance;
} winampGeneralPurposePlugin;

#define WM_WA_IPC          (WM_USER)
#define IPC_ISPLAYING      104
#define IPC_DELETE         101
#define IPC_STARTPLAY      102
#define IPC_SETPLAYLISTPOS 121
#define IPC_SETVOLUME      122
#define IPC_GETLISTLENGTH  124
#define IPC_SETSKINW       199
#define IPC_GETSKINW       1201
#define IPC_GET_SHUFFLE    250
#define IPC_GET_REPEAT     251
#define IPC_GETVISWND      612
#define IPC_ISVISRUNNING   613
#define IPC_REGISTER_WINAMP_IPCMESSAGE 65536
#define WM_ML_IPC          (WM_USER + 0x1000)
#define ML_IPC_DB_ADDORUPDATEFILEW 0x1711
#define ML_IPC_DB_SYNCDB   0x0709
typedef struct { wchar_t *fileName; int meta_mode; int gues_mode; } LMDB_FILE_ADD_INFOW;
#define IPC_ENQUEUEFILEW   1100

#define CMD_PREV     40044
#define CMD_PLAY     40045
#define CMD_PAUSE    40046
#define CMD_STOP     40047
#define CMD_NEXT     40048
#define CMD_VOLUP    40058
#define CMD_VOLDOWN  40059
#define CMD_REW5     40144
#define CMD_FFWD5    40148
#define CMD_REPEAT   40022
#define CMD_SHUFFLE  40023
#define CMD_VIS      40192

typedef struct {
    const wchar_t *filename;
    const wchar_t *title;
    const wchar_t *ext;
    int length;
} enqueueFileWithMetaStructW;

/* XInput */
#define XB_UP     0x0001
#define XB_DOWN   0x0002
#define XB_LEFT   0x0004
#define XB_RIGHT  0x0008
#define XB_START  0x0010
#define XB_BACK   0x0020
#define XB_L3     0x0040
#define XB_R3     0x0080
#define XB_LB     0x0100
#define XB_RB     0x0200
#define XB_A      0x1000
#define XB_B      0x2000
#define XB_X      0x4000
#define XB_Y      0x8000
#define XB_LT     0x10000  /* virtual */
#define XB_RT     0x20000  /* virtual */
#define XB_LS_UP    0x40000   /* left stick as buttons (virtual) */
#define XB_LS_DOWN  0x80000
#define XB_LS_LEFT  0x100000
#define XB_LS_RIGHT 0x200000
#define XB_RS_UP    0x400000  /* right stick (virtual) */
#define XB_RS_DOWN  0x800000
#define NBITS 24

typedef struct { WORD wButtons; BYTE bLeftTrigger, bRightTrigger; SHORT sThumbLX, sThumbLY, sThumbRX, sThumbRY; } XGAMEPAD;
typedef struct { DWORD dwPacketNumber; XGAMEPAD Gamepad; } XSTATE;
typedef DWORD (WINAPI *XInputGetState_t)(DWORD, XSTATE *);

static int  init(void);
static void config(void);
static void quit(void);

static winampGeneralPurposePlugin plugin = {
    GPPHDR_VER, "DeLorean23 Controller & Music Loader", init, config, quit, 0, 0
};

static XInputGetState_t pXInputGetState;
static volatile LONG pad_lx, pad_ly, pad_rx, pad_ry;   /* analog sticks, -32768..32767 */
static UINT_PTR timer_id;
static DWORD prev_buttons;
static DWORD held_since[32];
static DWORD last_repeat[32];
static int   ticks;
static int   loaded;
static int   select_used;      /* Select was part of a combo -> no skin change on release */
static wchar_t music_dir[MAX_PATH] = L"Z:\\userdata\\roms\\musik";
static int   autoplay = 1;
static int   add_to_library = 1;

static wchar_t ini_path[MAX_PATH];
static wchar_t test_file[MAX_PATH]; /* developer test: button mask from a file */

static LRESULT ipc(WPARAM w, LPARAM l) { return SendMessageW(plugin.hwndParent, WM_WA_IPC, w, l); }
static void cmd(int id) { SendMessageW(plugin.hwndParent, WM_COMMAND, id, 0); }

/* ------------------------------------------------------------------ */
/* Music folder                                                        */
/* ------------------------------------------------------------------ */
static const wchar_t *exts[] = {
    L"mp3", L"mp2", L"mp1", L"flac", L"ogg", L"oga", L"opus", L"wav", L"m4a", L"aac", L"mp4",
    L"wma", L"aif", L"aiff", L"mid", L"midi", L"mod", L"xm", L"it", L"s3m", L"mptm", L"m3u", L"m3u8", L"pls", NULL
};

static int is_music(const wchar_t *name) {
    const wchar_t *dot = wcsrchr(name, L'.');
    int i;
    if (!dot) return 0;
    for (i = 0; exts[i]; i++)
        if (!_wcsicmp(dot + 1, exts[i])) return 1;
    return 0;
}

typedef struct { wchar_t **items; int n, cap; } list_t;

static void list_add(list_t *l, const wchar_t *s) {
    if (l->n == l->cap) {
        l->cap = l->cap ? l->cap * 2 : 256;
        l->items = (wchar_t **)realloc(l->items, l->cap * sizeof(wchar_t *));
    }
    l->items[l->n++] = _wcsdup(s);
}

static int cmp_path(const void *a, const void *b) {
    return _wcsicmp(*(wchar_t *const *)a, *(wchar_t *const *)b);
}

static void scan(const wchar_t *dir, list_t *out, int depth) {
    wchar_t pat[MAX_PATH * 2];
    WIN32_FIND_DATAW fd;
    HANDLE h;
    list_t files = {0}, dirs = {0};
    int i;
    if (depth > 12) return;
    _snwprintf(pat, MAX_PATH * 2, L"%s\\*", dir);
    h = FindFirstFileW(pat, &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        wchar_t full[MAX_PATH * 2];
        if (fd.cFileName[0] == L'.') continue;
        _snwprintf(full, MAX_PATH * 2, L"%s\\%s", dir, fd.cFileName);
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) list_add(&dirs, full);
        else if (is_music(fd.cFileName)) list_add(&files, full);
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    qsort(files.items, files.n, sizeof(wchar_t *), cmp_path);
    qsort(dirs.items, dirs.n, sizeof(wchar_t *), cmp_path);
    for (i = 0; i < files.n; i++) { list_add(out, files.items[i]); free(files.items[i]); }
    for (i = 0; i < dirs.n; i++) { scan(dirs.items[i], out, depth + 1); free(dirs.items[i]); }
    free(files.items); free(dirs.items);
}

/* Files already in the Media Library (so they are not re-read every start) */
static wchar_t known_path[MAX_PATH];
static list_t known_new;
static int known_has(const wchar_t *f) {
    FILE *k;
    static wchar_t **cache; static int cn = -1;
    if (cn < 0) {
        wchar_t line[MAX_PATH * 2];
        cn = 0;
        k = _wfopen(known_path, L"rt, ccs=UTF-8");
        if (k) {
            int cap = 0;
            while (fgetws(line, MAX_PATH * 2, k)) {
                size_t l = wcslen(line);
                while (l && (line[l-1] == L'\n' || line[l-1] == L'\r')) line[--l] = 0;
                if (!l) continue;
                if (cn == cap) { cap = cap ? cap * 2 : 1024; cache = (wchar_t **)realloc(cache, cap * sizeof *cache); }
                cache[cn++] = _wcsdup(line);
            }
            fclose(k);
            qsort(cache, cn, sizeof *cache, cmp_path);
        }
    }
    return cn > 0 && bsearch(&f, cache, cn, sizeof *cache, cmp_path) != NULL;
}
static void save_known(void) {
    FILE *k;
    int i;
    if (!known_new.n) return;
    k = _wfopen(known_path, L"at, ccs=UTF-8");
    if (!k) return;
    for (i = 0; i < known_new.n; i++) { fwprintf(k, L"%ls\n", known_new.items[i]); free(known_new.items[i]); }
    fclose(k);
    free(known_new.items); memset(&known_new, 0, sizeof known_new);
}

/* Media Library: new files are added a few at a time so Winamp stays responsive */
static list_t ml_todo;
static int ml_pos;
static HWND ml_wnd(void) {
    static LRESULT id;
    if (!id) id = ipc((WPARAM)"LibraryGetWnd", IPC_REGISTER_WINAMP_IPCMESSAGE);
    return id ? (HWND)ipc((WPARAM)-1, id) : NULL;
}
static void ml_step(void) {
    HWND ml;
    int n = 0;
    if (ml_pos >= ml_todo.n) return;
    ml = ml_wnd();
    if (!ml) return;
    while (ml_pos < ml_todo.n && n++ < 3) {
        LMDB_FILE_ADD_INFOW fi;
        fi.fileName = ml_todo.items[ml_pos]; fi.meta_mode = 1; fi.gues_mode = 0;
        SendMessageW(ml, WM_ML_IPC, (WPARAM)&fi, ML_IPC_DB_ADDORUPDATEFILEW);
        free(ml_todo.items[ml_pos]); ml_todo.items[ml_pos] = NULL;
        ml_pos++;
    }
    if (ml_pos >= ml_todo.n) {
        SendMessageW(ml, WM_ML_IPC, 0, ML_IPC_DB_SYNCDB);
        save_known();
        free(ml_todo.items); memset(&ml_todo, 0, sizeof ml_todo); ml_pos = 0;
    }
}

static void load_music(void) {
    list_t all = {0};
    int i;
    DWORD attr = GetFileAttributesW(music_dir);
    if (attr == INVALID_FILE_ATTRIBUTES || !(attr & FILE_ATTRIBUTE_DIRECTORY)) return;
    scan(music_dir, &all, 0);
    if (all.n == 0) { free(all.items); return; }
    cmd(CMD_STOP);
    ipc(0, IPC_DELETE);
    for (i = 0; i < all.n; i++) {
        enqueueFileWithMetaStructW e;
        e.filename = all.items[i]; e.title = NULL; e.ext = NULL; e.length = -1;
        ipc((WPARAM)&e, IPC_ENQUEUEFILEW);
        if (add_to_library && !known_has(all.items[i])) {
            list_add(&ml_todo, all.items[i]);
            list_add(&known_new, all.items[i]);
        }
        free(all.items[i]);
    }
    free(all.items);
    ipc(0, IPC_SETPLAYLISTPOS);
    if (autoplay) {
        if (ipc(0, IPC_GET_SHUFFLE)) cmd(CMD_NEXT);  /* shuffle on: start with a random track */
        cmd(CMD_PLAY);
    }
}

/* ------------------------------------------------------------------ */
/* Skins                                                               */
/* ------------------------------------------------------------------ */
static wchar_t cur_skin[MAX_PATH];
static int cur_skin_known;

static void skin_init(void) {
    wchar_t cur[MAX_PATH] = L"";
    if (!cur_skin_known) {
        /* current skin = "skin=" in winamp.ini (read once, then tracked here) */
        const char *ini = (const char *)ipc(0, 334 /* IPC_GETINIFILE */);
        char a[MAX_PATH] = "";
        if (ini) GetPrivateProfileStringA("Winamp", "skin", "", a, MAX_PATH, ini);
        MultiByteToWideChar(CP_UTF8, 0, a, -1, cur, MAX_PATH);
        lstrcpynW(cur_skin, cur, MAX_PATH);
        cur_skin_known = 1;
    }
}

static void next_skin(void) {
    wchar_t skins_dir[MAX_PATH], pat[MAX_PATH + 4];
    wchar_t exe[MAX_PATH], *p;
    WIN32_FIND_DATAW fd;
    HANDLE h;
    list_t names = {0};
    int i, idx = -1;

    GetModuleFileNameW(NULL, exe, MAX_PATH);
    p = wcsrchr(exe, L'\\'); if (p) *p = 0;
    _snwprintf(skins_dir, MAX_PATH, L"%s\\Skins", exe);
    _snwprintf(pat, MAX_PATH + 4, L"%s\\*", skins_dir);

    list_add(&names, L"");  /* "" = built-in Winamp Classic */
    h = FindFirstFileW(pat, &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            const wchar_t *dot;
            if (fd.cFileName[0] == L'.') continue;
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) { list_add(&names, fd.cFileName); continue; }
            dot = wcsrchr(fd.cFileName, L'.');
            if (dot && (!_wcsicmp(dot, L".wsz") || !_wcsicmp(dot, L".wal") || !_wcsicmp(dot, L".zip")))
                list_add(&names, fd.cFileName);
        } while (FindNextFileW(h, &fd));
        FindClose(h);
    }
    qsort(names.items + 1, names.n - 1, sizeof(wchar_t *), cmp_path);

    skin_init();
    for (i = 0; i < names.n; i++)
        if (!_wcsicmp(names.items[i], cur_skin)) { idx = i; break; }
    idx = (idx + 1) % names.n;
    lstrcpynW(cur_skin, names.items[idx], MAX_PATH);
    ipc((WPARAM)names.items[idx], IPC_SETSKINW);
    for (i = 0; i < names.n; i++) free(names.items[i]);
    free(names.items);
}

/* ------------------------------------------------------------------ */
/* Milkdrop                                                            */
/* ------------------------------------------------------------------ */
static HWND vis_wnd(void) {
    HWND w = (HWND)ipc(0, IPC_GETVISWND);
    return (w && IsWindow(w)) ? w : NULL;
}
static int vis_running(void) { return ipc(0, IPC_ISVISRUNNING) ? 1 : 0; }

static void vis_key(UINT vk, int ch) {
    HWND w = vis_wnd();
    if (!w) return;
    if (ch) {
        PostMessageW(w, WM_CHAR, ch, 1);
    } else {
        UINT sc = MapVirtualKeyW(vk, 0);
        PostMessageW(w, WM_KEYDOWN, vk, 1 | (sc << 16));
        PostMessageW(w, WM_KEYUP, vk, 1 | (sc << 16) | (3u << 30));
    }
}

static void vis_fullscreen(void) {
    HWND w = vis_wnd();
    UINT sc = MapVirtualKeyW(VK_RETURN, 0);
    if (!w) return;
    PostMessageW(w, WM_SYSKEYDOWN, VK_RETURN, 1 | (sc << 16) | (1u << 29));
    PostMessageW(w, WM_SYSKEYUP, VK_RETURN, 1 | (sc << 16) | (1u << 29) | (3u << 30));
}

static void dlog(const char *fmt, ...);
static int pending_fullscreen;  /* ticks until fullscreen after starting vis */

/* Exit: stop Milkdrop first (closing Winamp with a running visualization can
   hang under Wine), then close Winamp. A watchdog ends the process if it is
   still there 15 seconds later, so Batocera always gets back to the menu. */
static volatile LONG exit_started;
static volatile LONG vis_cached;   /* updated by tick(), read by the controller thread */
static DWORD WINAPI exit_thread(LPVOID p) {
    (void)p;
    dlog("exit: vis=%ld", vis_cached);
    if (vis_cached) {
        PostMessageW(plugin.hwndParent, WM_COMMAND, CMD_VIS, 0);
        Sleep(1500);
    }
    PostMessageW(plugin.hwndParent, WM_CLOSE, 0, 0);
    Sleep(15000);
    TerminateProcess(GetCurrentProcess(), 0);   /* still here: end it */
    return 0;
}
/* called from the controller thread, so it works even if Winamp's main thread hangs */
static void request_exit(void) {
    if (InterlockedExchange(&exit_started, 1)) return;
    CloseHandle(CreateThread(NULL, 0, exit_thread, NULL, 0, NULL));
}

static volatile DWORD pad_state;
#include "mouse_wallpaper.inc"

/* ------------------------------------------------------------------ */
/* Controller                                                          */
/* ------------------------------------------------------------------ */
static DWORD read_pad(void) {
    DWORD b = 0;
    int i, got = 0;
    LONG lx = 0, ly = 0, rx = 0, ry = 0;
    if (pXInputGetState) {
        for (i = 0; i < 4; i++) {
            XSTATE s;
            if (pXInputGetState(i, &s) == ERROR_SUCCESS) {
                got = 1;
                b |= s.Gamepad.wButtons;
                if (s.Gamepad.bLeftTrigger > 100)  b |= XB_LT;
                if (s.Gamepad.bRightTrigger > 100) b |= XB_RT;
                if (abs(s.Gamepad.sThumbLX) > abs(lx)) lx = s.Gamepad.sThumbLX;
                if (abs(s.Gamepad.sThumbLY) > abs(ly)) ly = s.Gamepad.sThumbLY;
                if (abs(s.Gamepad.sThumbRX) > abs(rx)) rx = s.Gamepad.sThumbRX;
                if (abs(s.Gamepad.sThumbRY) > abs(ry)) ry = s.Gamepad.sThumbRY;
            }
        }
    }
    if (!got) {
        /* fallback: plain joystick (Xbox-style button order) */
        static const DWORD map[] = { XB_A, XB_B, XB_X, XB_Y, XB_LB, XB_RB, XB_BACK, XB_START, XB_L3, XB_R3 };
        for (i = 0; i < 2; i++) {
            JOYINFOEX j; int k;
            memset(&j, 0, sizeof j); j.dwSize = sizeof j; j.dwFlags = JOY_RETURNALL;
            if (joyGetPosEx(JOYSTICKID1 + i, &j) != JOYERR_NOERROR) continue;
            for (k = 0; k < 10; k++) if (j.dwButtons & (1u << k)) b |= map[k];
            if (j.dwPOV != JOY_POVCENTERED && j.dwPOV <= 36000) {
                DWORD p = j.dwPOV;
                if (p > 31500 || p < 4500) b |= XB_UP;
                if (p > 4500 && p < 13500) b |= XB_RIGHT;
                if (p > 13500 && p < 22500) b |= XB_DOWN;
                if (p > 22500 && p < 31500) b |= XB_LEFT;
            }
            lx = (LONG)j.dwXpos - 32768; ly = 32767 - (LONG)j.dwYpos;
            rx = (LONG)j.dwUpos - 32768; ry = 32767 - (LONG)j.dwRpos;
            if (abs(rx) < 3000 && abs(ry) < 3000) { rx = 0; ry = 0; }
        }
    }
    if (test_file[0]) {
        FILE *f = _wfopen(test_file, L"r");
        if (f) {
            unsigned long v = 0; long a1 = 0, a2 = 0, a3 = 0, a4 = 0;
            int n = fscanf(f, "%lx %ld %ld %ld %ld", &v, &a1, &a2, &a3, &a4);
            if (n >= 1) b |= v;
            if (n >= 5) { lx = a1; ly = a2; rx = a3; ry = a4; }
            fclose(f);
        }
    }
    if (ly > 20000)  b |= XB_LS_UP;
    if (ly < -20000) b |= XB_LS_DOWN;
    if (lx < -20000) b |= XB_LS_LEFT;
    if (lx > 20000)  b |= XB_LS_RIGHT;
    if (ry > 20000)  b |= XB_RS_UP;
    if (ry < -20000) b |= XB_RS_DOWN;
    pad_lx = lx; pad_ly = ly; pad_rx = rx; pad_ry = ry;
    return b;
}

static void dlog(const char *fmt, ...) {
    FILE *f; va_list ap;
    if (!test_file[0]) return;
    f = fopen("Z:\\tmp\\delorean.log", "a");
    if (!f) return;
    va_start(ap, fmt); vfprintf(f, fmt, ap); va_end(ap);
    fputc('\n', f); fclose(f);
}

static void on_press(DWORD btn, DWORD state) {
    int vis = vis_running();
    dlog("press %lx state %lx vis %d mouse %d", (unsigned long)btn, (unsigned long)state, vis, mouse_mode);
    if (btn == XB_L3) { set_mouse_mode(!mouse_mode); return; }
    if (mouse_mode) {
        switch (btn) {
        case XB_A: mouse_button(MOUSEEVENTF_LEFTDOWN); return;
        case XB_B: mouse_button(MOUSEEVENTF_RIGHTDOWN); return;
        case XB_X:
            mouse_button(MOUSEEVENTF_LEFTDOWN); mouse_button(MOUSEEVENTF_LEFTUP);
            mouse_button(MOUSEEVENTF_LEFTDOWN); mouse_button(MOUSEEVENTF_LEFTUP);
            return;
        case XB_UP: case XB_DOWN: case XB_LEFT: case XB_RIGHT:
            mouse_dpad_step(btn); return;
        case XB_LS_UP: case XB_LS_DOWN: case XB_LS_LEFT: case XB_LS_RIGHT:
        case XB_RS_UP: case XB_RS_DOWN:
            return;   /* sticks move the pointer / scroll */
        }
    } else {
        /* left stick works like the D-pad */
        if (btn == XB_LS_UP) btn = XB_UP;
        else if (btn == XB_LS_DOWN) btn = XB_DOWN;
        else if (btn == XB_LS_LEFT) btn = XB_LEFT;
        else if (btn == XB_LS_RIGHT) btn = XB_RIGHT;
    }
    switch (btn) {
    case XB_A:
        if (ipc(0, IPC_ISPLAYING) == 1) cmd(CMD_PAUSE); else cmd(CMD_PLAY);
        break;
    case XB_B:     cmd(CMD_STOP); break;
    case XB_X:     cmd(CMD_SHUFFLE); break;
    case XB_Y:     cmd(CMD_VIS); break;
    case XB_LB:    cmd(CMD_PREV); break;
    case XB_RB:    cmd(CMD_NEXT); break;
    case XB_LT:    cmd(CMD_REW5); break;
    case XB_RT:    cmd(CMD_FFWD5); break;
    case XB_UP:    cmd(CMD_VOLUP); break;
    case XB_DOWN:  cmd(CMD_VOLDOWN); break;
    case XB_LEFT:  if (vis) vis_key(VK_BACK, 0); else cmd(CMD_REW5); break;
    case XB_RIGHT: if (vis) vis_key(VK_SPACE, 0); else cmd(CMD_FFWD5); break;
    case XB_R3:    cmd(CMD_REPEAT); break;
    case XB_RS_UP: if (vis) vis_key(0, 'h'); break;
    case XB_START:
        if (state & XB_BACK) {             /* Select + Start = exit (handled in the controller thread) */
            select_used = 1;
            break;
        }
        if (vis) vis_fullscreen();
        else { cmd(CMD_VIS); pending_fullscreen = 30; }
        break;
    }
    if (btn != XB_BACK && (state & XB_BACK)) select_used = 1;
}

/* The controller is read in its own thread, so no button press gets lost while
   Winamp's main thread is busy (e.g. Milkdrop switching fullscreen). The
   presses are queued and handled in Winamp's main thread by tick(). */
typedef struct { DWORD state, prev; } pad_event;
static pad_event q[64];
static volatile LONG q_head, q_tail;
static CRITICAL_SECTION q_lock;
static volatile LONG pad_thread_run;
static HANDLE pad_thread;

static DWORD WINAPI pad_loop(LPVOID p) {
    DWORD last = 0;
    (void)p;
    while (pad_thread_run) {
        DWORD b = read_pad();
        pad_state = b;
        if ((b & XB_START) && (b & XB_BACK)) request_exit();
        if (b != last) {
            EnterCriticalSection(&q_lock);
            if (((q_head + 1) & 63) != q_tail) {
                q[q_head].state = b; q[q_head].prev = last;
                q_head = (q_head + 1) & 63;
            }
            LeaveCriticalSection(&q_lock);
            last = b;
        }
        Sleep(10);
    }
    return 0;
}

static void handle_change(DWORD b, DWORD prev, DWORD now) {
    DWORD pressed = b & ~prev, released = prev & ~b;
    int i;
    /* mouse buttons: release (also when mouse mode was switched off meanwhile) */
    if ((released & XB_A) && mouse_left_down) mouse_button(MOUSEEVENTF_LEFTUP);
    if ((released & XB_B) && mouse_right_down) mouse_button(MOUSEEVENTF_RIGHTUP);
    if (pressed & XB_BACK) select_used = 0;
    for (i = 0; i < NBITS; i++) {
        DWORD m2 = 1u << i;
        if (m2 == XB_BACK) continue;
        if (pressed & m2) {
            held_since[i] = now; last_repeat[i] = now;
            on_press(m2, b);
        }
    }
    if ((released & XB_BACK) && !select_used) next_skin();
}

static void CALLBACK tick(HWND h, UINT m, UINT_PTR id, DWORD t) {
    DWORD now, b;
    int i;
    (void)h; (void)m; (void)id; (void)t;
    ticks++;
    if (!loaded && ticks >= 20) {   /* ~1 s after start: Winamp is ready */
        loaded = 1;
        load_music();
    }
    vis_cached = vis_running();
    wp_step();
    if (loaded && ticks > 120 && ticks % 4 == 0) ml_step();   /* library: start after ~6 s, gently */
    if (pending_fullscreen > 0 && --pending_fullscreen == 0 && vis_running()) vis_fullscreen();

    now = GetTickCount();
    for (;;) {
        pad_event ev;
        EnterCriticalSection(&q_lock);
        if (q_tail == q_head) { LeaveCriticalSection(&q_lock); break; }
        ev = q[q_tail]; q_tail = (q_tail + 1) & 63;
        LeaveCriticalSection(&q_lock);
        handle_change(ev.state, ev.prev, now);
    }
    /* hold to repeat (volume / seek) */
    b = pad_state;
    for (i = 0; i < NBITS; i++) {
        DWORD m2 = 1u << i;
        if (mouse_mode && m2 != XB_LT && m2 != XB_RT) continue;   /* mouse mode: D-pad/stick move the pointer */
        if ((b & m2) && (m2 == XB_UP || m2 == XB_DOWN || m2 == XB_LT || m2 == XB_RT || m2 == XB_LS_UP || m2 == XB_LS_DOWN)) {
            if (now - held_since[i] > 400 && now - last_repeat[i] > 120) {
                last_repeat[i] = now;
                on_press(m2, b);
            }
        }
    }
    prev_buttons = b;
}

/* ------------------------------------------------------------------ */
static void read_settings(void) {
    wchar_t dll[MAX_PATH], *p, buf[MAX_PATH];
    GetModuleFileNameW(plugin.hDllInstance, dll, MAX_PATH);
    p = wcsrchr(dll, L'\\'); if (p) *p = 0;
    _snwprintf(ini_path, MAX_PATH, L"%s\\gen_delorean.ini", dll);
    GetPrivateProfileStringW(L"DeLorean", L"musicdir", music_dir, buf, MAX_PATH, ini_path);
    lstrcpynW(music_dir, buf, MAX_PATH);
    autoplay = GetPrivateProfileIntW(L"DeLorean", L"autoplay", 1, ini_path);
    add_to_library = GetPrivateProfileIntW(L"DeLorean", L"library", 1, ini_path);
    {
        /* list of files already in the Media Library: next to winamp.ini */
        const char *d = (const char *)SendMessageW(plugin.hwndParent, WM_WA_IPC, 0, 335 /* IPC_GETINIDIRECTORY */);
        wchar_t wd[MAX_PATH];
        if (d && *d) MultiByteToWideChar(CP_ACP, 0, d, -1, wd, MAX_PATH); else lstrcpynW(wd, dll, MAX_PATH);
        _snwprintf(known_path, MAX_PATH, L"%s\\gen_delorean_library.txt", wd);
    }
    GetPrivateProfileStringW(L"DeLorean", L"testfile", L"", test_file, MAX_PATH, ini_path);
    mouse_mode = GetPrivateProfileIntW(L"DeLorean", L"mouse", 1, ini_path) ? 1 : 0;
    cursor_scale = GetPrivateProfileIntW(L"DeLorean", L"cursorsize", 2, ini_path);
    if (cursor_scale < 1) cursor_scale = 1;
    if (cursor_scale > 4) cursor_scale = 4;
    mouse_speed = GetPrivateProfileIntW(L"DeLorean", L"mousespeed", 1400, ini_path);
    if (mouse_speed < 100) mouse_speed = 100;
    GetPrivateProfileStringW(L"DeLorean", L"wallpaperdir", wp_dir, buf, MAX_PATH, ini_path);
    lstrcpynW(wp_dir, buf, MAX_PATH);
}

static int init(void) {
    static const char *dlls[] = { "xinput1_4.dll", "xinput1_3.dll", "xinput9_1_0.dll", NULL };
    int i;
    read_settings();
    for (i = 0; dlls[i] && !pXInputGetState; i++) {
        HMODULE x = LoadLibraryA(dlls[i]);
        if (x) pXInputGetState = (XInputGetState_t)GetProcAddress(x, "XInputGetState");
    }
    InitializeCriticalSection(&q_lock);
    pad_thread_run = 1;
    pad_thread = CreateThread(NULL, 0, pad_loop, NULL, 0, NULL);
    timer_id = SetTimer(NULL, 0, 50, tick);
    wallpaper_init();
    mouse_init();
    return 0;
}

static void config(void) {
    MessageBoxW(plugin.hwndParent,
        L"DeLorean23 Controller & Music Loader\n\n"
        L"A = Play/Pause   B = Stop   X = Shuffle   Y = Milkdrop\n"
        L"LB/RB = Track   LT/RT = Seek   D-pad up/down = Volume\n"
        L"D-pad left/right = Milkdrop preset   Start = Fullscreen\n"
        L"Select = Next skin   R3 = Repeat   Select+Start = Exit\n"
        L"L3 = Mouse mode (left stick, A/B/X click, right stick scroll)\n\n"
        L"Settings: Plugins\\gen_delorean.ini",
        L"DeLorean23", MB_OK);
}

static void quit(void) {
    if (timer_id) KillTimer(NULL, timer_id);
    timer_id = 0;
    mouse_quit();
    if (wp_wnd) DestroyWindow(wp_wnd);
    wp_wnd = NULL;
    pad_thread_run = 0;
    if (pad_thread) { WaitForSingleObject(pad_thread, 500); CloseHandle(pad_thread); pad_thread = NULL; }
}

__declspec(dllexport) winampGeneralPurposePlugin *winampGetGeneralPurposePlugin(void) {
    return &plugin;
}
