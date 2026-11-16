/* ------------------------------------------------------------------ */
/* Mouse mode: own XP-style arrow (Batocera hides the system pointer)  */
/* ------------------------------------------------------------------ */
static int  mouse_mode = 1;          /* ini: mouse=1 -> on at start */
static int  cursor_scale = 2;        /* ini: cursorsize=1..4 */
static int  mouse_speed = 1400;      /* ini: mousespeed (pixels per second at full stick) */
static int  mouse_left_down, mouse_right_down;
static HWND cursor_wnd;
static POINT cur_pos;                /* where our pointer is */
static double acc_x, acc_y, acc_wheel;
static DWORD last_mouse_tick, last_move_time;
static DWORD dpad_since;
static int  cursor_visible;
static UINT_PTR mouse_timer;
static DWORD cursor_tid;      /* the pointer has its own thread, so it stays smooth while Winamp is busy */
static HANDLE cursor_thread_h;
#define WM_CURSOR_PLACE (WM_APP + 1)
#define WM_CURSOR_STEP  (WM_APP + 2)
static DWORD last_raise;

/* classic Windows XP arrow, 12 x 19: '#' black, '.' white */
static const char *arrow[19] = {
    "#           ",
    "##          ",
    "#.#         ",
    "#..#        ",
    "#...#       ",
    "#....#      ",
    "#.....#     ",
    "#......#    ",
    "#.......#   ",
    "#........#  ",
    "#.........# ",
    "#......#####",
    "#...#..#    ",
    "#..##..#    ",
    "#.#  #..#   ",
    "##   #..#   ",
    "#     #..#  ",
    "      #..#  ",
    "       ##   ",
};

static LRESULT CALLBACK cursor_proc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(h, &ps);
        HBRUSH bk = (HBRUSH)GetStockObject(BLACK_BRUSH), wh = (HBRUSH)GetStockObject(WHITE_BRUSH);
        int x, y;
        for (y = 0; y < 19; y++)
            for (x = 0; x < 12; x++) {
                RECT r;
                char c = arrow[y][x];
                if (c == ' ') continue;
                r.left = x * cursor_scale; r.top = y * cursor_scale;
                r.right = r.left + cursor_scale; r.bottom = r.top + cursor_scale;
                FillRect(dc, &r, c == '#' ? bk : wh);
            }
        EndPaint(h, &ps);
        return 0;
    }
    case WM_NCHITTEST:    return HTTRANSPARENT;
    case WM_MOUSEACTIVATE: return MA_NOACTIVATE;
    }
    return DefWindowProcW(h, m, w, l);
}

static void create_cursor(void) {
    WNDCLASSW wc;
    HRGN rgn;
    int x, y;
    memset(&wc, 0, sizeof wc);
    wc.lpfnWndProc = cursor_proc;
    wc.hInstance = plugin.hDllInstance;
    wc.lpszClassName = L"DeLoreanCursor";
    RegisterClassW(&wc);
    cursor_wnd = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
                                 L"DeLoreanCursor", L"", WS_POPUP,
                                 0, 0, 12 * cursor_scale, 19 * cursor_scale, NULL, NULL, plugin.hDllInstance, NULL);
    if (!cursor_wnd) return;
    /* arrow-shaped window, so nothing around the arrow is drawn (works without a compositor) */
    rgn = CreateRectRgn(0, 0, 0, 0);
    for (y = 0; y < 19; y++)
        for (x = 0; x < 12; x++)
            if (arrow[y][x] != ' ') {
                HRGN px = CreateRectRgn(x * cursor_scale, y * cursor_scale, (x + 1) * cursor_scale, (y + 1) * cursor_scale);
                CombineRgn(rgn, rgn, px, RGN_OR);
                DeleteObject(px);
            }
    SetWindowRgn(cursor_wnd, rgn, TRUE);
}

/* The arrow window sits 1 pixel right/below the real hotspot, so it never
   covers the point that gets clicked. */
static void place_cursor(int show) {
    if (cursor_tid && GetCurrentThreadId() != cursor_tid) {
        PostThreadMessageW(cursor_tid, WM_CURSOR_PLACE, (WPARAM)show, 0);
        return;
    }
    if (!cursor_wnd) return;
    if (!show) {
        if (cursor_visible) ShowWindow(cursor_wnd, SW_HIDE);
        cursor_visible = 0;
        return;
    }
    if (!cursor_visible) {
        /* (re)show: hide first so the window gets mapped again on top of everything */
        ShowWindow(cursor_wnd, SW_HIDE);
        SetWindowPos(cursor_wnd, HWND_TOPMOST, cur_pos.x + 1, cur_pos.y + 1, 0, 0, SWP_NOSIZE | SWP_NOACTIVATE);
        ShowWindow(cursor_wnd, SW_SHOWNOACTIVATE);
    } else {
        SetWindowPos(cursor_wnd, HWND_TOPMOST, cur_pos.x + 1, cur_pos.y + 1, 0, 0, SWP_NOSIZE | SWP_NOACTIVATE);
    }
    cursor_visible = 1;
}

static void screen_size(int *w, int *h) {
    *w = GetSystemMetrics(SM_CXSCREEN);
    *h = GetSystemMetrics(SM_CYSCREEN);
}

static void mouse_move_to(int x, int y) {
    INPUT in;
    int w, h;
    screen_size(&w, &h);
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x > w - 1) x = w - 1;
    if (y > h - 1) y = h - 1;
    cur_pos.x = x; cur_pos.y = y;
    memset(&in, 0, sizeof in);
    in.type = INPUT_MOUSE;
    in.mi.dx = (LONG)(((LONGLONG)x * 65535 + (w - 1) / 2) / (w > 1 ? w - 1 : 1));
    in.mi.dy = (LONG)(((LONGLONG)y * 65535 + (h - 1) / 2) / (h > 1 ? h - 1 : 1));
    in.mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE;
    SendInput(1, &in, sizeof in);
    last_move_time = GetTickCount();
    place_cursor(mouse_mode);
}

static void mouse_button(DWORD flag) {
    INPUT in;
    memset(&in, 0, sizeof in);
    in.type = INPUT_MOUSE;
    in.mi.dwFlags = flag;
    if (flag == MOUSEEVENTF_LEFTDOWN) mouse_left_down = 1;
    if (flag == MOUSEEVENTF_LEFTUP) mouse_left_down = 0;
    if (flag == MOUSEEVENTF_RIGHTDOWN) mouse_right_down = 1;
    if (flag == MOUSEEVENTF_RIGHTUP) mouse_right_down = 0;
    SendInput(1, &in, sizeof in);
    last_move_time = GetTickCount();
    place_cursor(mouse_mode);
}

static void mouse_wheel(int delta) {
    INPUT in;
    memset(&in, 0, sizeof in);
    in.type = INPUT_MOUSE;
    in.mi.dwFlags = MOUSEEVENTF_WHEEL;
    in.mi.mouseData = (DWORD)delta;
    SendInput(1, &in, sizeof in);
}

static void mouse_dpad_step(DWORD btn) {
    int dx = 0, dy = 0;
    if (cursor_tid && GetCurrentThreadId() != cursor_tid) {
        PostThreadMessageW(cursor_tid, WM_CURSOR_STEP, (WPARAM)btn, 0);
        return;
    }
    if (btn == XB_UP) dy = -1;
    if (btn == XB_DOWN) dy = 1;
    if (btn == XB_LEFT) dx = -1;
    if (btn == XB_RIGHT) dx = 1;
    dpad_since = GetTickCount();
    mouse_move_to(cur_pos.x + dx, cur_pos.y + dy);
}

static void set_mouse_mode(int on) {
    mouse_mode = on;
    if (!on) {
        if (mouse_left_down) mouse_button(MOUSEEVENTF_LEFTUP);
        if (mouse_right_down) mouse_button(MOUSEEVENTF_RIGHTUP);
    }
    last_move_time = GetTickCount();
    place_cursor(on);
}

static double stick_speed(LONG v) {
    double a = v < 0 ? -(double)v : (double)v, s;
    const double dz = 7000.0;
    if (a <= dz) return 0.0;
    s = (a - dz) / (32767.0 - dz);
    if (s > 1.0) s = 1.0;
    s = s * s;                      /* fine control near the centre, fast at full tilt */
    return v < 0 ? -s : s;
}

static void CALLBACK mouse_tick(HWND h, UINT m, UINT_PTR id, DWORD t) {
    DWORD now = GetTickCount(), b = pad_state;
    double dt = last_mouse_tick ? (now - last_mouse_tick) / 1000.0 : 0.0;
    POINT real;
    (void)h; (void)m; (void)id; (void)t;
    if (dt > 0.1) dt = 0.1;
    last_mouse_tick = now;

    /* a real mouse moved the pointer: follow it */
    if (GetCursorPos(&real) && (real.x != cur_pos.x || real.y != cur_pos.y)) {
        cur_pos = real;
        last_move_time = now;
        if (mouse_mode) place_cursor(1);
    }
    if (!mouse_mode) return;

    /* left stick */
    acc_x += stick_speed(pad_lx) * mouse_speed * dt;
    acc_y -= stick_speed(pad_ly) * mouse_speed * dt;
    /* D-pad held: after 0.3 s glide at moderate speed */
    if ((b & (XB_UP | XB_DOWN | XB_LEFT | XB_RIGHT)) && (LONG)(now - dpad_since) > 300) {
        double sp = 250.0 * dt;
        if (b & XB_UP) acc_y -= sp;
        if (b & XB_DOWN) acc_y += sp;
        if (b & XB_LEFT) acc_x -= sp;
        if (b & XB_RIGHT) acc_x += sp;
    }
    if ((int)acc_x != 0 || (int)acc_y != 0) {
        int dx = (int)acc_x, dy = (int)acc_y;
        acc_x -= dx; acc_y -= dy;
        mouse_move_to(cur_pos.x + dx, cur_pos.y + dy);
    }
    /* right stick: scroll */
    acc_wheel += stick_speed(pad_ry) * 12.0 * dt;   /* notches per second */
    if (acc_wheel >= 1.0 || acc_wheel <= -1.0) {
        int n = (int)acc_wheel;
        acc_wheel -= n;
        mouse_wheel(n * WHEEL_DELTA);
        last_move_time = now;
    }
    /* menus or windows opened later must not cover the arrow */
    if (cursor_visible && now - last_raise > 300) {
        last_raise = now;
        if (GetTopWindow(NULL) != cursor_wnd) {
            cursor_visible = 0; place_cursor(1);
        }
    }
    /* hide the arrow after 10 s without movement (e.g. while watching Milkdrop) */
    if (cursor_visible && (LONG)(GetTickCount() - last_move_time) > 10000) place_cursor(0);
}

static DWORD WINAPI cursor_thread(LPVOID p) {
    MSG msg;
    int w, h;
    (void)p;
    create_cursor();
    screen_size(&w, &h);
    if (!GetCursorPos(&cur_pos)) { cur_pos.x = w / 2; cur_pos.y = h / 2; }
    last_move_time = GetTickCount();
    if (mouse_mode) mouse_move_to(w / 2, h / 2);
    mouse_timer = SetTimer(NULL, 0, 15, mouse_tick);
    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        if (msg.message == WM_CURSOR_PLACE) { place_cursor((int)msg.wParam); continue; }
        if (msg.message == WM_CURSOR_STEP)  { mouse_dpad_step((DWORD)msg.wParam); continue; }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    if (mouse_timer) KillTimer(NULL, mouse_timer);
    mouse_timer = 0;
    if (cursor_wnd) DestroyWindow(cursor_wnd);
    cursor_wnd = NULL;
    return 0;
}

static void mouse_init(void) {
    cursor_thread_h = CreateThread(NULL, 0, cursor_thread, NULL, 0, &cursor_tid);
}

static void mouse_quit(void) {
    if (!cursor_thread_h) return;
    PostThreadMessageW(cursor_tid, WM_QUIT, 0, 0);
    WaitForSingleObject(cursor_thread_h, 1000);
    CloseHandle(cursor_thread_h);
    cursor_thread_h = NULL;
}

/* ------------------------------------------------------------------ */
/* Wallpaper behind the Winamp windows                                 */
/* ------------------------------------------------------------------ */
typedef struct { UINT32 GdiplusVersion; void *DebugEventCallback; BOOL SuppressBackgroundThread; BOOL SuppressExternalCodecs; } GdipStartupInput;
typedef int (WINAPI *GdiplusStartup_t)(ULONG_PTR *, const GdipStartupInput *, void *);
typedef int (WINAPI *GdipLoadImageFromFile_t)(const WCHAR *, void **);
typedef int (WINAPI *GdipGetImageDim_t)(void *, UINT *);
typedef int (WINAPI *GdipCreateFromHDC_t)(HDC, void **);
typedef int (WINAPI *GdipSetInterpolationMode_t)(void *, int);
typedef int (WINAPI *GdipDrawImageRectRectI_t)(void *, void *, INT, INT, INT, INT, INT, INT, INT, INT, int, void *, void *, void *);
typedef int (WINAPI *GdipDelete_t)(void *);

static struct {
    HMODULE dll;
    ULONG_PTR token;
    GdipLoadImageFromFile_t load;
    GdipGetImageDim_t width, height;
    GdipCreateFromHDC_t from_hdc;
    GdipSetInterpolationMode_t interp;
    GdipDrawImageRectRectI_t draw;
    GdipDelete_t del_graphics, dispose;
} gp;

static wchar_t wp_dir[MAX_PATH] = L"Z:\\userdata\\roms\\winamp\\wallpaper";
static wchar_t wp_cfg[MAX_PATH];
static HWND    wp_wnd;
static HBITMAP wp_bmp;
static int     wp_on = 1, wp_slideshow = 0, wp_time = 10, wp_random = 0;
static wchar_t wp_image[MAX_PATH];
static wchar_t wp_current[MAX_PATH];
static FILETIME wp_cfg_time;
static DWORD   wp_last_change, wp_last_check;
static int     wp_index = -1;

static int gp_init(void) {
    GdipStartupInput in;
    GdiplusStartup_t start;
    if (gp.dll) return gp.draw != NULL;
    gp.dll = LoadLibraryA("gdiplus.dll");
    if (!gp.dll) return 0;
    start = (GdiplusStartup_t)GetProcAddress(gp.dll, "GdiplusStartup");
    gp.load = (GdipLoadImageFromFile_t)GetProcAddress(gp.dll, "GdipLoadImageFromFile");
    gp.width = (GdipGetImageDim_t)GetProcAddress(gp.dll, "GdipGetImageWidth");
    gp.height = (GdipGetImageDim_t)GetProcAddress(gp.dll, "GdipGetImageHeight");
    gp.from_hdc = (GdipCreateFromHDC_t)GetProcAddress(gp.dll, "GdipCreateFromHDC");
    gp.interp = (GdipSetInterpolationMode_t)GetProcAddress(gp.dll, "GdipSetInterpolationMode");
    gp.draw = (GdipDrawImageRectRectI_t)GetProcAddress(gp.dll, "GdipDrawImageRectRectI");
    gp.del_graphics = (GdipDelete_t)GetProcAddress(gp.dll, "GdipDeleteGraphics");
    gp.dispose = (GdipDelete_t)GetProcAddress(gp.dll, "GdipDisposeImage");
    if (!start || !gp.load || !gp.width || !gp.height || !gp.from_hdc || !gp.draw || !gp.del_graphics || !gp.dispose) {
        gp.draw = NULL;
        return 0;
    }
    memset(&in, 0, sizeof in);
    in.GdiplusVersion = 1;
    if (start(&gp.token, &in, NULL) != 0) { gp.draw = NULL; return 0; }
    return 1;
}

/* load an image and scale it to fill the screen (cut at the edges, never stretched) */
static HBITMAP load_wallpaper(const wchar_t *file) {
    void *img = NULL, *g = NULL;
    UINT iw = 0, ih = 0;
    int sw, sh, sx, sy, cw, ch;
    HDC sdc, mdc;
    HBITMAP bmp;
    HGDIOBJ old;
    if (!gp_init()) return NULL;
    if (gp.load(file, &img) != 0 || !img) return NULL;
    gp.width(img, &iw); gp.height(img, &ih);
    if (!iw || !ih) { gp.dispose(img); return NULL; }
    screen_size(&sw, &sh);
    /* crop the source to the screen's aspect ratio */
    if ((double)iw / ih > (double)sw / sh) {
        ch = ih; cw = (int)((double)ih * sw / sh); sx = (iw - cw) / 2; sy = 0;
    } else {
        cw = iw; ch = (int)((double)iw * sh / sw); sx = 0; sy = (ih - ch) / 2;
    }
    sdc = GetDC(NULL);
    mdc = CreateCompatibleDC(sdc);
    bmp = CreateCompatibleBitmap(sdc, sw, sh);
    ReleaseDC(NULL, sdc);
    old = SelectObject(mdc, bmp);
    if (gp.from_hdc(mdc, &g) == 0 && g) {
        if (gp.interp) gp.interp(g, 7 /* high quality bicubic */);
        gp.draw(g, img, 0, 0, sw, sh, sx, sy, cw, ch, 2 /* pixels */, NULL, NULL, NULL);
        gp.del_graphics(g);
    }
    SelectObject(mdc, old);
    DeleteDC(mdc);
    gp.dispose(img);
    return bmp;
}

static LRESULT CALLBACK wp_proc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(h, &ps);
        RECT r;
        GetClientRect(h, &r);
        if (wp_bmp) {
            HDC mdc = CreateCompatibleDC(dc);
            HGDIOBJ old = SelectObject(mdc, wp_bmp);
            BitBlt(dc, 0, 0, r.right, r.bottom, mdc, 0, 0, SRCCOPY);
            SelectObject(mdc, old);
            DeleteDC(mdc);
        } else {
            FillRect(dc, &r, (HBRUSH)GetStockObject(BLACK_BRUSH));
        }
        EndPaint(h, &ps);
        return 0;
    }
    case WM_ERASEBKGND:    return 1;
    case WM_MOUSEACTIVATE: return MA_NOACTIVATE;
    case WM_WINDOWPOSCHANGING: {
        /* always stay behind everything else */
        WINDOWPOS *wp = (WINDOWPOS *)l;
        wp->hwndInsertAfter = HWND_BOTTOM;
        wp->flags &= ~SWP_NOZORDER;
        break;
    }
    }
    return DefWindowProcW(h, m, w, l);
}

static int is_image(const wchar_t *n) {
    const wchar_t *d = wcsrchr(n, L'.');
    return d && (!_wcsicmp(d, L".jpg") || !_wcsicmp(d, L".jpeg") || !_wcsicmp(d, L".png") || !_wcsicmp(d, L".bmp"));
}

static void list_images(list_t *out) {
    wchar_t pat[MAX_PATH + 4];
    WIN32_FIND_DATAW fd;
    HANDLE h;
    _snwprintf(pat, MAX_PATH + 4, L"%s\\*", wp_dir);
    h = FindFirstFileW(pat, &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && is_image(fd.cFileName)) list_add(out, fd.cFileName);
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    qsort(out->items, out->n, sizeof(wchar_t *), cmp_path);
}

static void show_wallpaper_file(const wchar_t *name) {
    wchar_t full[MAX_PATH * 2];
    HBITMAP b;
    if (!name || !name[0]) return;
    if (!_wcsicmp(name, wp_current) && wp_bmp) return;
    _snwprintf(full, MAX_PATH * 2, L"%s\\%s", wp_dir, name);
    b = load_wallpaper(full);
    if (!b) return;
    if (wp_bmp) DeleteObject(wp_bmp);
    wp_bmp = b;
    lstrcpynW(wp_current, name, MAX_PATH);
    if (wp_wnd) InvalidateRect(wp_wnd, NULL, FALSE);
}

static void wp_pick(int advance) {
    list_t imgs = {0};
    int i;
    list_images(&imgs);
    if (imgs.n == 0) { free(imgs.items); return; }
    if (!wp_slideshow) {
        /* fixed image: image= or the first one */
        int found = -1;
        for (i = 0; i < imgs.n; i++) if (wp_image[0] && !_wcsicmp(imgs.items[i], wp_image)) found = i;
        wp_index = found >= 0 ? found : 0;
    } else if (advance || wp_index < 0) {
        if (wp_random && imgs.n > 1) {
            int n;
            do n = rand() % imgs.n; while (n == wp_index);
            wp_index = n;
        } else {
            /* keep the order even when files were added/removed */
            int cur = -1;
            for (i = 0; i < imgs.n; i++) if (!_wcsicmp(imgs.items[i], wp_current)) cur = i;
            wp_index = cur < 0 ? 0 : (cur + 1) % imgs.n;
        }
    }
    if (wp_index >= imgs.n) wp_index = 0;
    show_wallpaper_file(imgs.items[wp_index]);
    for (i = 0; i < imgs.n; i++) free(imgs.items[i]);
    free(imgs.items);
}

static void wp_read_config(void) {
    wp_on = GetPrivateProfileIntW(L"Wallpaper", L"wallpaper", 1, wp_cfg);
    wp_slideshow = GetPrivateProfileIntW(L"Wallpaper", L"slideshow", 0, wp_cfg);
    wp_time = GetPrivateProfileIntW(L"Wallpaper", L"time", 10, wp_cfg);
    if (wp_time < 1) wp_time = 1;
    wp_random = GetPrivateProfileIntW(L"Wallpaper", L"random", 0, wp_cfg);
    GetPrivateProfileStringW(L"Wallpaper", L"image", L"", wp_image, MAX_PATH, wp_cfg);
}

static void wp_apply(void) {
    if (!wp_wnd) return;
    if (!wp_on) {
        /* no wallpaper: plain black background (also covers the Wine desktop) */
        if (wp_bmp) { DeleteObject(wp_bmp); wp_bmp = NULL; }
        wp_current[0] = 0;
        InvalidateRect(wp_wnd, NULL, FALSE);
    } else {
        wp_pick(0);
    }
    if (!IsWindowVisible(wp_wnd)) {
        ShowWindow(wp_wnd, SW_SHOWNOACTIVATE);
        SetWindowPos(wp_wnd, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    }
}

/* called every 50 ms from tick() */
static void wp_step(void) {
    DWORD now = GetTickCount();
    if (now - wp_last_check > 2000) {            /* config.ini changed? -> apply live */
        WIN32_FILE_ATTRIBUTE_DATA a;
        wp_last_check = now;
        if (GetFileAttributesExW(wp_cfg, GetFileExInfoStandard, &a)) {
            if (CompareFileTime(&a.ftLastWriteTime, &wp_cfg_time) != 0) {
                wp_cfg_time = a.ftLastWriteTime;
                wp_read_config();
                wp_current[0] = 0;
                wp_apply();
                wp_last_change = now;
            }
        }
    }
    if (wp_on && wp_slideshow && now - wp_last_change >= (DWORD)wp_time * 1000) {
        wp_last_change = now;
        wp_pick(1);
    }
}

static void wallpaper_init(void) {
    WNDCLASSW wc;
    WIN32_FILE_ATTRIBUTE_DATA a;
    int w, h;
    _snwprintf(wp_cfg, MAX_PATH, L"%s\\config.ini", wp_dir);
    if (GetFileAttributesExW(wp_cfg, GetFileExInfoStandard, &a)) wp_cfg_time = a.ftLastWriteTime;
    wp_read_config();
    srand(GetTickCount());
    memset(&wc, 0, sizeof wc);
    wc.lpfnWndProc = wp_proc;
    wc.hInstance = plugin.hDllInstance;
    wc.lpszClassName = L"DeLoreanWallpaper";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClassW(&wc);
    screen_size(&w, &h);
    wp_wnd = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, L"DeLoreanWallpaper", L"Wallpaper",
                             WS_POPUP, 0, 0, w, h, NULL, NULL, plugin.hDllInstance, NULL);
    wp_last_change = GetTickCount();
    wp_apply();
}
