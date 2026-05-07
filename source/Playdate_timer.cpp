// Native Win32 port of the uploaded Playdate Lua timer app.
// v7: double-buffered painting, 400x240 inner display, configurable fonts, topmost/opacity/icon resources.
// Build on Linux/macOS with clang/lld-link, no Windows SDK/import libs required:
//   clang -target x86_64-pc-windows-msvc -ffreestanding -fno-builtin -fno-stack-protector -Oz -c playdate_timer_win.cpp -o playdate_timer_win.obj
//   lld-link /subsystem:windows /entry:entry /nodefaultlib /out:PlaydateTimer.exe playdate_timer_win.obj
//
// Controls:
//   W/S/A/D = Up/Down/Left/Right
//   E       = Playdate A
//   B       = Playdate B
//   C       = crank clockwise 360 degrees
//   V       = crank counter-clockwise 360 degrees

// extern "C" unsigned __int64 __readgsqword(unsigned long Offset);
#if defined(_MSC_VER)
    #include <intrin.h>
#elif defined(__GNUC__) && defined(__x86_64__)
    #include <stdint.h>

    static inline unsigned long long __readgsqword(unsigned long Offset) {
        unsigned long long value;
        __asm__ __volatile__(
            "movq %%gs:(%1), %0"
            : "=r"(value)
            : "r"((uintptr_t)Offset)
        );
        return value;
    }
#else
    #error This code requires 64-bit Windows.
#endif

// =====================
// Basic types / constants
// =====================
typedef unsigned char      u8;
typedef unsigned short     u16;
typedef unsigned int       u32;
typedef unsigned long long u64;
typedef signed long long   i64;
typedef long long          isize;
typedef int                BOOL;
typedef unsigned int       UINT;
typedef unsigned long      DWORD;
typedef long               LONG;
typedef unsigned short     WORD;
typedef unsigned long long WPARAM;
typedef long long          LPARAM;
typedef long long          LRESULT;
typedef void*              HANDLE;
typedef void*              HINSTANCE;
typedef void*              HWND;
typedef void*              HDC;
typedef void*              HBRUSH;
typedef void*              HPEN;
typedef void*              HFONT;
typedef void*              HGDIOBJ;
typedef void*              HBITMAP;
typedef void*              HRGN;
typedef void*              HICON;
typedef void*              HCURSOR;
typedef void*              HMENU;
typedef const char*        LPCSTR;
typedef void*              LPVOID;
typedef unsigned int       COLORREF;
typedef unsigned short     WCHAR;

#define NULLPTR nullptr
#define TRUE 1
#define FALSE 0

#define WM_DESTROY     0x0002
#define WM_PAINT       0x000F
#define WM_KEYDOWN     0x0100
#define WM_KEYUP       0x0101
#define WM_TIMER       0x0113
#define WM_CREATE      0x0001
#define WM_ACTIVATE    0x0006
#define WM_SETICON     0x0080
#define WM_ERASEBKGND  0x0014
#define WM_LBUTTONDOWN 0x0201
#define WM_NCLBUTTONDOWN 0x00A1

#define VK_ESCAPE 0x1B
#define WA_INACTIVE    0
#define ICON_SMALL     0
#define ICON_BIG       1

#define WS_CAPTION      0x00C00000L
#define WS_SYSMENU      0x00080000L
#define WS_MINIMIZEBOX  0x00020000L
#define WS_OVERLAPPEDWINDOW_NOSIZE (WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX)
#define WS_POPUP        0x80000000L
#define WS_EX_TOPMOST   0x00000008L
#define WS_EX_APPWINDOW 0x00040000L
#define WS_EX_LAYERED   0x00080000L
#define CW_USEDEFAULT   ((int)0x80000000u)
#define SW_SHOW         5
#define IDC_ARROW       ((LPCSTR)32512)
#define HTCAPTION       2
#define SND_ASYNC       0x0001
#define SND_NODEFAULT   0x0002
#define SND_FILENAME    0x00020000
#define FR_PRIVATE      0x10
#define LWA_ALPHA       0x00000002
#define SRCCOPY         ((DWORD)0x00CC0020)
#define IMAGE_ICON      1
#define LR_LOADFROMFILE 0x00000010
#define APP_OPACITY_ACTIVE   255
#define APP_OPACITY_INACTIVE 200
#define DWMWA_WINDOW_CORNER_PREFERENCE 33
#define DWMWCP_ROUND 2

#define TRANSPARENT_BK  1
#define PS_SOLID        0
#define FW_NORMAL       400
#define FW_BOLD         700
#define DEFAULT_CHARSET 1
#define OUT_DEFAULT_PRECIS 0
#define CLIP_DEFAULT_PRECIS 0
#define DEFAULT_QUALITY 0
#define NONANTIALIASED_QUALITY 3
#define CLEARTYPE_QUALITY 5
#define FIXED_PITCH     1
#define FF_MODERN       0x30

#define COLOR_BLACK RGBc(0,0,0)
#define COLOR_WHITE RGBc(255,255,255)
// Light mode: background #d6d3cb, text #312e28
#define COLOR_LIGHT_BG RGBc(214,211,203)
#define COLOR_LIGHT_INK RGBc(49,46,40)
// Dark mode: background #312e28, text #d6d3cb
#define COLOR_DARK_BG RGBc(49,46,40)
#define COLOR_DARK_INK RGBc(214,211,203)

static inline COLORREF RGBc(u8 r, u8 g, u8 b) { return ((COLORREF)r) | ((COLORREF)g << 8) | ((COLORREF)b << 16); }

struct POINT { LONG x; LONG y; };
struct SIZE { LONG cx; LONG cy; };
struct RECT { LONG left; LONG top; LONG right; LONG bottom; };
struct SYSTEMTIME { WORD wYear; WORD wMonth; WORD wDayOfWeek; WORD wDay; WORD wHour; WORD wMinute; WORD wSecond; WORD wMilliseconds; };
struct PAINTSTRUCT { HDC hdc; BOOL fErase; RECT rcPaint; BOOL fRestore; BOOL fIncUpdate; u8 rgbReserved[32]; };
struct MSG { HWND hwnd; UINT message; WPARAM wParam; LPARAM lParam; DWORD time; POINT pt; };

typedef LRESULT (*WNDPROC_T)(HWND, UINT, WPARAM, LPARAM);
struct WNDCLASSEXA {
    UINT cbSize;
    UINT style;
    WNDPROC_T lpfnWndProc;
    int cbClsExtra;
    int cbWndExtra;
    HINSTANCE hInstance;
    HICON hIcon;
    HCURSOR hCursor;
    HBRUSH hbrBackground;
    LPCSTR lpszMenuName;
    LPCSTR lpszClassName;
    HICON hIconSm;
};

// =====================
// PEB / PE export resolver
// =====================
struct LIST_ENTRY { LIST_ENTRY* Flink; LIST_ENTRY* Blink; };
struct UNICODE_STRING { u16 Length; u16 MaximumLength; WCHAR* Buffer; };
struct PEB_LDR_DATA { u8 Reserved1[16]; LIST_ENTRY InLoadOrderModuleList; };
struct LDR_DATA_TABLE_ENTRY {
    LIST_ENTRY InLoadOrderLinks;
    LIST_ENTRY InMemoryOrderLinks;
    LIST_ENTRY InInitializationOrderLinks;
    void* DllBase;
    void* EntryPoint;
    u32 SizeOfImage;
    UNICODE_STRING FullDllName;
    UNICODE_STRING BaseDllName;
};
struct PEB { u8 Reserved1[24]; PEB_LDR_DATA* Ldr; };

static int c_strlen(const char* s) {
    int n = 0;
    if (!s) return 0;
    while (s[n]) n++;
    return n;
}

static int c_streq(const char* a, const char* b) {
    int i = 0;
    while (a[i] && b[i]) { if (a[i] != b[i]) return 0; i++; }
    return a[i] == b[i];
}

static char ascii_lower(char c) {
    if (c >= 'A' && c <= 'Z') return (char)(c + 32);
    return c;
}

static int unicode_basename_eq_ascii_ci(UNICODE_STRING* us, const char* ascii) {
    if (!us || !us->Buffer || !ascii) return 0;
    int alen = c_strlen(ascii);
    int wlen = us->Length / 2;
    if (alen != wlen) return 0;
    for (int i = 0; i < alen; i++) {
        char wc = (char)(us->Buffer[i] & 0xFF);
        if (ascii_lower(wc) != ascii_lower(ascii[i])) return 0;
    }
    return 1;
}

static void* get_module_base(const char* dllName) {
    PEB* peb = (PEB*)__readgsqword(0x60);
    if (!peb || !peb->Ldr) return NULLPTR;
    LIST_ENTRY* head = &peb->Ldr->InLoadOrderModuleList;
    for (LIST_ENTRY* it = head->Flink; it && it != head; it = it->Flink) {
        LDR_DATA_TABLE_ENTRY* ent = (LDR_DATA_TABLE_ENTRY*)it;
        if (unicode_basename_eq_ascii_ci(&ent->BaseDllName, dllName)) return ent->DllBase;
    }
    return NULLPTR;
}

static int has_dot(const char* s) {
    for (int i = 0; s && s[i]; i++) if (s[i] == '.') return 1;
    return 0;
}

static void copy_forwarder_part(char* dst, int dstCap, const char* src, int start, int end) {
    int p = 0;
    for (int i = start; i < end && p < dstCap - 1; i++) dst[p++] = src[i];
    dst[p] = 0;
}

static void append_dll_if_needed(char* name, int cap) {
    if (has_dot(name)) return;
    int p = c_strlen(name);
    const char* ext = ".dll";
    for (int i = 0; ext[i] && p < cap - 1; i++) name[p++] = ext[i];
    name[p] = 0;
}

static void* resolve_export_depth(void* module, const char* name, int depth) {
    if (!module || depth > 8) return NULLPTR;
    u8* base = (u8*)module;
    u32 e_lfanew = *(u32*)(base + 0x3C);
    u8* nt = base + e_lfanew;
    u8* opt = nt + 24;
    u32 exportRVA = *(u32*)(opt + 112);
    u32 exportSize = *(u32*)(opt + 116);
    if (!exportRVA) return NULLPTR;
    u8* ed = base + exportRVA;
    u32 numberOfNames = *(u32*)(ed + 24);
    u32 addressOfFunctions = *(u32*)(ed + 28);
    u32 addressOfNames = *(u32*)(ed + 32);
    u32 addressOfNameOrdinals = *(u32*)(ed + 36);
    u32* funcs = (u32*)(base + addressOfFunctions);
    u32* names = (u32*)(base + addressOfNames);
    u16* ords = (u16*)(base + addressOfNameOrdinals);
    for (u32 i = 0; i < numberOfNames; i++) {
        char* n = (char*)(base + names[i]);
        if (c_streq(n, name)) {
            u16 ord = ords[i];
            u32 rva = funcs[ord];
            if (exportSize && rva >= exportRVA && rva < exportRVA + exportSize) {
                // Forwarder string, e.g. "KERNELBASE.GetProcAddress".
                const char* fwd = (const char*)(base + rva);
                int dot = -1;
                for (int j = 0; fwd[j]; j++) { if (fwd[j] == '.') { dot = j; break; } }
                if (dot <= 0) return NULLPTR;
                char modName[80];
                char fnName[96];
                copy_forwarder_part(modName, 80, fwd, 0, dot);
                append_dll_if_needed(modName, 80);
                int end = dot + 1;
                while (fwd[end]) end++;
                copy_forwarder_part(fnName, 96, fwd, dot + 1, end);
                if (fnName[0] == '#') return NULLPTR;
                void* target = get_module_base(modName);
                return resolve_export_depth(target, fnName, depth + 1);
            }
            return (void*)(base + rva);
        }
    }
    return NULLPTR;
}

static void* resolve_export(void* module, const char* name) {
    return resolve_export_depth(module, name, 0);
}

// =====================
// Win32 function pointers
// =====================
typedef void* (*PFN_LoadLibraryA)(LPCSTR);
typedef void* (*PFN_GetProcAddress)(void*, LPCSTR);
typedef void  (*PFN_ExitProcess)(UINT);
typedef u64   (*PFN_GetTickCount64)(void);
typedef void  (*PFN_GetLocalTime)(SYSTEMTIME*);
typedef DWORD (*PFN_GetPrivateProfileStringA)(LPCSTR,LPCSTR,LPCSTR,char*,DWORD,LPCSTR);
typedef UINT  (*PFN_GetPrivateProfileIntA)(LPCSTR,LPCSTR,int,LPCSTR);

typedef unsigned short (*PFN_RegisterClassExA)(const WNDCLASSEXA*);
typedef HWND  (*PFN_CreateWindowExA)(DWORD,LPCSTR,LPCSTR,DWORD,int,int,int,int,HWND,HMENU,HINSTANCE,LPVOID);
typedef BOOL  (*PFN_ShowWindow)(HWND,int);
typedef BOOL  (*PFN_UpdateWindow)(HWND);
typedef BOOL  (*PFN_GetMessageA)(MSG*,HWND,UINT,UINT);
typedef BOOL  (*PFN_TranslateMessage)(const MSG*);
typedef LRESULT (*PFN_DispatchMessageA)(const MSG*);
typedef LRESULT (*PFN_DefWindowProcA)(HWND,UINT,WPARAM,LPARAM);
typedef void  (*PFN_PostQuitMessage)(int);
typedef BOOL  (*PFN_InvalidateRect)(HWND,const RECT*,BOOL);
typedef HDC   (*PFN_BeginPaint)(HWND,PAINTSTRUCT*);
typedef BOOL  (*PFN_EndPaint)(HWND,const PAINTSTRUCT*);
typedef UINT  (*PFN_SetTimer)(HWND,UINT,UINT,void*);
typedef BOOL  (*PFN_KillTimer)(HWND,UINT);
typedef BOOL  (*PFN_FillRect)(HDC,const RECT*,HBRUSH);
typedef BOOL  (*PFN_AdjustWindowRect)(RECT*,DWORD,BOOL);
typedef HCURSOR (*PFN_LoadCursorA)(HINSTANCE,LPCSTR);
typedef HANDLE (*PFN_LoadImageA)(HINSTANCE,LPCSTR,UINT,int,int,UINT);
typedef BOOL  (*PFN_MessageBeep)(UINT);
typedef LRESULT (*PFN_SendMessageA)(HWND,UINT,WPARAM,LPARAM);
typedef BOOL  (*PFN_SetWindowRgn)(HWND,HRGN,BOOL);
typedef BOOL  (*PFN_SetLayeredWindowAttributes)(HWND,COLORREF,u8,DWORD);
typedef int   (*PFN_DwmSetWindowAttribute)(HWND,DWORD,const void*,DWORD);

typedef BOOL  (*PFN_PlaySoundA)(LPCSTR,void*,DWORD);

typedef HBRUSH (*PFN_CreateSolidBrush)(COLORREF);
typedef HPEN   (*PFN_CreatePen)(int,int,COLORREF);
typedef HGDIOBJ(*PFN_SelectObject)(HDC,HGDIOBJ);
typedef BOOL   (*PFN_DeleteObject)(HGDIOBJ);
typedef HDC    (*PFN_CreateCompatibleDC)(HDC);
typedef HBITMAP(*PFN_CreateCompatibleBitmap)(HDC,int,int);
typedef BOOL   (*PFN_DeleteDC)(HDC);
typedef BOOL   (*PFN_BitBlt)(HDC,int,int,int,int,HDC,int,int,DWORD);
typedef BOOL   (*PFN_Polygon)(HDC,const POINT*,int);
typedef BOOL   (*PFN_MoveToEx)(HDC,int,int,POINT*);
typedef BOOL   (*PFN_LineTo)(HDC,int,int);
typedef BOOL   (*PFN_RoundRect)(HDC,int,int,int,int,int,int);
typedef HRGN   (*PFN_CreateRoundRectRgn)(int,int,int,int,int,int);
typedef int    (*PFN_AddFontResourceExA)(LPCSTR,DWORD,void*);
typedef int    (*PFN_SetBkMode)(HDC,int);
typedef COLORREF (*PFN_SetTextColor)(HDC,COLORREF);
typedef BOOL   (*PFN_TextOutA)(HDC,int,int,LPCSTR,int);
typedef BOOL   (*PFN_GetTextExtentPoint32A)(HDC,LPCSTR,int,SIZE*);
typedef HFONT  (*PFN_CreateFontA)(int,int,int,int,int,DWORD,DWORD,DWORD,DWORD,DWORD,DWORD,DWORD,DWORD,LPCSTR);

static PFN_LoadLibraryA pLoadLibraryA;
static PFN_GetProcAddress pGetProcAddress;
static PFN_ExitProcess pExitProcess;
static PFN_GetTickCount64 pGetTickCount64;
static PFN_GetLocalTime pGetLocalTime;
static PFN_GetPrivateProfileStringA pGetPrivateProfileStringA;
static PFN_GetPrivateProfileIntA pGetPrivateProfileIntA;

static PFN_RegisterClassExA pRegisterClassExA;
static PFN_CreateWindowExA pCreateWindowExA;
static PFN_ShowWindow pShowWindow;
static PFN_UpdateWindow pUpdateWindow;
static PFN_GetMessageA pGetMessageA;
static PFN_TranslateMessage pTranslateMessage;
static PFN_DispatchMessageA pDispatchMessageA;
static PFN_DefWindowProcA pDefWindowProcA;
static PFN_PostQuitMessage pPostQuitMessage;
static PFN_InvalidateRect pInvalidateRect;
static PFN_BeginPaint pBeginPaint;
static PFN_EndPaint pEndPaint;
static PFN_SetTimer pSetTimer;
static PFN_KillTimer pKillTimer;
static PFN_FillRect pFillRect;
static PFN_AdjustWindowRect pAdjustWindowRect;
static PFN_LoadCursorA pLoadCursorA;
static PFN_LoadImageA pLoadImageA;
static PFN_MessageBeep pMessageBeep;
static PFN_SendMessageA pSendMessageA;
static PFN_SetWindowRgn pSetWindowRgn;
static PFN_SetLayeredWindowAttributes pSetLayeredWindowAttributes;
static PFN_DwmSetWindowAttribute pDwmSetWindowAttribute;
static PFN_PlaySoundA pPlaySoundA;

static PFN_CreateSolidBrush pCreateSolidBrush;
static PFN_CreatePen pCreatePen;
static PFN_SelectObject pSelectObject;
static PFN_DeleteObject pDeleteObject;
static PFN_CreateCompatibleDC pCreateCompatibleDC;
static PFN_CreateCompatibleBitmap pCreateCompatibleBitmap;
static PFN_DeleteDC pDeleteDC;
static PFN_BitBlt pBitBlt;
static PFN_Polygon pPolygon;
static PFN_MoveToEx pMoveToEx;
static PFN_LineTo pLineTo;
static PFN_RoundRect pRoundRect;
static PFN_CreateRoundRectRgn pCreateRoundRectRgn;
static PFN_AddFontResourceExA pAddFontResourceExA;
static PFN_SetBkMode pSetBkMode;
static PFN_SetTextColor pSetTextColor;
static PFN_TextOutA pTextOutA;
static PFN_GetTextExtentPoint32A pGetTextExtentPoint32A;
static PFN_CreateFontA pCreateFontA;

static void* gp(void* mod, const char* name) { return pGetProcAddress(mod, name); }

// =====================
// App constants / state
// =====================
static const int SCREEN_W = 400;
static const int SCREEN_H = 240;
static const int SCREEN_CENTER_X = 200;
static const int SCALE = 1;
static const int BORDER_PX = 10; // narrower black frame; inner display remains 400x240
static const int OUTER_RADIUS_PX = 14; // fallback radius; DWM handles smoother corners when available
static const int CLIENT_W = SCREEN_W * SCALE + BORDER_PX * 2;
static const int CLIENT_H = SCREEN_H * SCALE + BORDER_PX * 2;

static const int CIRCLE = 360;
static const int MS_PER_SECOND = 1000;
static const int SECONDS_PER_MINUTE = 60;
static const int TRANSITION_MS = 300;
static const int PHASE_ALERT_MS = 2000;
static const int SETTINGS_MESSAGE_MS = 1200;
static const int A_LONG_PRESS_MS = 1000;
static const int BLINK_PERIOD_MS = 1300;
static const int BLINK_DUTY_PERCENT = 65;

static const int DEFAULT_LOOP_COUNT = 3;
static const int DEFAULT_WORK_MINUTES = 25;
static const int DEFAULT_BREAK_MINUTES = 5;
static const int LOOP_MIN = 1;
static const int LOOP_MAX = 10;
static const int MINUTES_MIN = 1;
static const int MINUTES_MAX = 60;

enum Mode { MODE_CLOCK=0, MODE_WORKTIME=1, MODE_BREAKTIME=2, MODE_SETTINGS=3, MODE_THEMES=4 };
enum Theme { THEME_DARK=0, THEME_LIGHT=1 };
enum Direction { DIR_NONE=0, DIR_UP=1, DIR_DOWN=2, DIR_LEFT=3, DIR_RIGHT=4 };

struct TimerObj {
    i64 durationMs;
    i64 timeLeft;
    u64 deadline;
    int paused;
};

struct TransitionState {
    int active;
    Mode from;
    Mode to;
    int dx;
    int dy;
    u64 start;
};

struct InputState {
    int btnA;
    int btnAReleased;
    int btnAHeld;
    int btnB;
    int up;
    int down;
    int left;
    int right;
    int crankChange;
};

static HWND g_hwnd;
static HFONT g_defaultFont;
static HFONT g_timeFont;
static HFONT g_timerFont;
static HFONT g_hintFont;
static char g_defaultFontFace[80] = "Consolas";
static char g_timeFontFace[80] = "Consolas";
static char g_timerFontFace[80] = "Consolas";
static char g_defaultFontFile[160] = "";
static char g_timeFontFile[160] = "";
static char g_timerFontFile[160] = "";
static int g_defaultFontPx = 11; // small Playdate-style system text on a 400x240 screen
static int g_timeFontPx = 29;    // clock font visual size from the original port
static int g_timerFontPx = 43;   // timer font visual size from the original port
static int g_defaultFontWeight = FW_BOLD;
static int g_timeFontWeight = FW_BOLD;
static int g_timerFontWeight = FW_BOLD;
static DWORD g_defaultFontItalic = 0;
static DWORD g_timeFontItalic = 0;
static DWORD g_timerFontItalic = 0;
static DWORD g_defaultFontQuality = NONANTIALIASED_QUALITY;
static DWORD g_timeFontQuality = NONANTIALIASED_QUALITY;
static DWORD g_timerFontQuality = NONANTIALIASED_QUALITY;
static char g_promptSoundPath[160] = "sounds\\prompt_tone.wav";
static char g_appIconPath[160] = "icon\\app.ico";
static int g_activeOpacityAlpha = APP_OPACITY_ACTIVE;
static int g_inactiveOpacityAlpha = APP_OPACITY_INACTIVE;
static int g_alwaysOnTop = 1;
static int g_drawOffsetX;
static int g_drawOffsetY;
static COLORREF g_textColor;

static Mode mode = MODE_CLOCK;
static Theme theme = THEME_LIGHT; // Windows build defaults to e-ink style
static TimerObj workTimer;
static TimerObj breakTimer;
static TimerObj* activeTimer = NULLPTR;
static TransitionState transition;

static int timerResetCrankAccum = 0;
static int settingsCrankAccum = 0;
static int loopCount = DEFAULT_LOOP_COUNT;
static int workMinutes = DEFAULT_WORK_MINUTES;
static int breakMinutes = DEFAULT_BREAK_MINUTES;
static int settingsIndex = 1;
static u64 settingsAHoldStart = 0;
static int settingsLongPressDone = 0;
static const char* settingsMessage = NULLPTR;
static u64 settingsMessageUntil = 0;
static int loopRunning = 0;
static int currentLoop = 1;
static Mode loopPhase = MODE_WORKTIME;
static const char* phaseAlertMessage = NULLPTR;
static u64 phaseAlertUntil = 0;
static int phaseAlertNext = 0; // 0 none, 1 work->break, 2 break->work, 3 finish

static int keyDown[256];
static int keyPressed[256];
static int keyReleased[256];

static const char* modeName(Mode m) {
    if (m == MODE_CLOCK) return "CLOCK";
    if (m == MODE_WORKTIME) return "WORKTIME";
    if (m == MODE_BREAKTIME) return "BREAKTIME";
    if (m == MODE_SETTINGS) return "SETTINGS";
    return "THEMES";
}
static const char* themeName() { return theme == THEME_LIGHT ? "LIGHT" : "DARK"; }

static int clampi(int v, int mn, int mx) { return v < mn ? mn : (v > mx ? mx : v); }
static i64 max_i64(i64 a, i64 b) { return a > b ? a : b; }
static int c_atoi_clamped(const char* s, int fallback, int mn, int mx) {
    if (!s || !s[0]) return fallback;
    int v = 0;
    int seen = 0;
    for (int i = 0; s[i]; i++) {
        char c = s[i];
        if (c >= '0' && c <= '9') {
            seen = 1;
            v = v * 10 + (c - '0');
            if (v > mx) return mx;
        } else if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            if (seen) break;
        } else {
            break;
        }
    }
    if (!seen) return fallback;
    return clampi(v, mn, mx);
}

// =====================
// Small formatting helpers
// =====================
static int append_char(char* out, int pos, char c) { out[pos++] = c; out[pos] = 0; return pos; }
static int append_str(char* out, int pos, const char* s) { while (*s) out[pos++] = *s++; out[pos] = 0; return pos; }
static int append_uint_pad(char* out, int pos, u32 v, int width) {
    char tmp[16];
    int n = 0;
    do { tmp[n++] = (char)('0' + (v % 10)); v /= 10; } while (v && n < 16);
    while (n < width) tmp[n++] = '0';
    for (int i = n - 1; i >= 0; i--) out[pos++] = tmp[i];
    out[pos] = 0;
    return pos;
}
static void fmt_time(char* out, SYSTEMTIME* st) {
    int p = 0;
    p = append_uint_pad(out, p, st->wHour, 2); p = append_char(out, p, ':');
    p = append_uint_pad(out, p, st->wMinute, 2); p = append_char(out, p, ':');
    p = append_uint_pad(out, p, st->wSecond, 2);
}
static void fmt_date(char* out, SYSTEMTIME* st) {
    const char* wd[7] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    int p = 0;
    p = append_str(out, p, wd[st->wDayOfWeek % 7]);
    p = append_str(out, p, "  ");
    p = append_uint_pad(out, p, st->wYear, 4); p = append_char(out, p, '-');
    p = append_uint_pad(out, p, st->wMonth, 2); p = append_char(out, p, '-');
    p = append_uint_pad(out, p, st->wDay, 2);
}
static void fmt_timer(char* out, i64 remaining) {
    if (remaining < 0) remaining = 0;
    u32 sec = (u32)(remaining / MS_PER_SECOND);
    u32 min = sec / SECONDS_PER_MINUTE;
    sec = sec % SECONDS_PER_MINUTE;
    int p = 0;
    p = append_uint_pad(out, p, min, 2); p = append_char(out, p, ':');
    p = append_uint_pad(out, p, sec, 2);
}
static void fmt_int(char* out, int v) { int p = 0; append_uint_pad(out, p, (u32)v, 1); }
static void fmt_setting_minutes(char* out, int v) { int p = 0; p = append_uint_pad(out, p, (u32)v, 1); append_char(out, p, 'm'); }
static void fmt_loop_text(char* out, const char* phase, int cur, int total) {
    int p = 0;
    p = append_str(out, p, phase); p = append_str(out, p, "    LOOP  ");
    p = append_uint_pad(out, p, (u32)cur, 1); p = append_str(out, p, " / ");
    p = append_uint_pad(out, p, (u32)total, 1);
}
static void fmt_loop_only(char* out, int cur, int total) {
    int p = 0;
    p = append_str(out, p, "LOOP  ");
    p = append_uint_pad(out, p, (u32)cur, 1);
    p = append_str(out, p, " / ");
    p = append_uint_pad(out, p, (u32)total, 1);
}

// =====================
// Timers and state helpers
// =====================
static u64 now_ms() { return pGetTickCount64 ? pGetTickCount64() : 0; }

static void timer_make(TimerObj* t, int minutes) {
    t->durationMs = (i64)minutes * SECONDS_PER_MINUTE * MS_PER_SECOND;
    t->timeLeft = t->durationMs;
    t->deadline = 0;
    t->paused = 1;
}
static void timer_update(TimerObj* t) {
    if (!t || t->paused) return;
    u64 n = now_ms();
    t->timeLeft = (t->deadline > n) ? (i64)(t->deadline - n) : 0;
}
static void timer_reset(TimerObj* t) { if (t) { t->timeLeft = t->durationMs; t->deadline = 0; t->paused = 1; } }
static void timer_pause(TimerObj* t) { if (t && !t->paused) { timer_update(t); t->paused = 1; } }
static void timer_start(TimerObj* t) { if (t) { if (t->timeLeft <= 0) t->timeLeft = t->durationMs; t->deadline = now_ms() + (u64)t->timeLeft; t->paused = 0; } }

static TimerObj* timerForMode(Mode m) {
    if (m == MODE_WORKTIME) return &workTimer;
    if (m == MODE_BREAKTIME) return &breakTimer;
    return NULLPTR;
}
static void rebuildTimers() { timer_make(&workTimer, workMinutes); timer_make(&breakTimer, breakMinutes); activeTimer = NULLPTR; }
static void clearPhaseAlert() { phaseAlertMessage = NULLPTR; phaseAlertUntil = 0; phaseAlertNext = 0; }
static void cancelSettingsHold() { settingsAHoldStart = 0; settingsLongPressDone = 0; }
static void showSettingsMessage(const char* text) { settingsMessage = text; settingsMessageUntil = now_ms() + SETTINGS_MESSAGE_MS; }

static Direction getReturnDirection(Mode fromMode) {
    if (fromMode == MODE_WORKTIME) return DIR_DOWN;
    if (fromMode == MODE_BREAKTIME) return DIR_UP;
    if (fromMode == MODE_SETTINGS) return DIR_RIGHT;
    if (fromMode == MODE_THEMES) return DIR_LEFT;
    return DIR_NONE;
}
static void directionToOffset(Direction direction, int* dx, int* dy) {
    *dx = 0; *dy = 0;
    if (direction == DIR_UP) *dy = SCREEN_H;
    else if (direction == DIR_DOWN) *dy = -SCREEN_H;
    else if (direction == DIR_LEFT) *dx = SCREEN_W;
    else if (direction == DIR_RIGHT) *dx = -SCREEN_W;
}
static void startTransition(Mode toMode, Direction direction) {
    if (mode == toMode) return;
    int dx, dy; directionToOffset(direction, &dx, &dy);
    if (dx == 0 && dy == 0) { mode = toMode; transition.active = 0; return; }
    Mode oldMode = mode;
    mode = toMode;
    transition.active = 1;
    transition.from = oldMode;
    transition.to = toMode;
    transition.dx = dx;
    transition.dy = dy;
    transition.start = now_ms();
}
static void updateTransition(u64 n) {
    if (transition.active && (n - transition.start >= (u64)TRANSITION_MS)) transition.active = 0;
}
static void stopAllTimers() {
    loopRunning = 0;
    currentLoop = 1;
    loopPhase = MODE_WORKTIME;
    transition.active = 0;
    clearPhaseAlert();
    timer_reset(&workTimer);
    timer_reset(&breakTimer);
    activeTimer = NULLPTR;
}
static void resetSettingsToDefault() {
    loopCount = DEFAULT_LOOP_COUNT;
    workMinutes = DEFAULT_WORK_MINUTES;
    breakMinutes = DEFAULT_BREAK_MINUTES;
    settingsCrankAccum = 0;
    timerResetCrankAccum = 0;
    stopAllTimers();
    rebuildTimers();
    showSettingsMessage("Defaults reset");
}
static void adjustSetting(int delta) {
    int oldLoop = loopCount, oldWork = workMinutes, oldBreak = breakMinutes;
    if (settingsIndex == 1) loopCount = clampi(loopCount + delta, LOOP_MIN, LOOP_MAX);
    else if (settingsIndex == 2) workMinutes = clampi(workMinutes + delta, MINUTES_MIN, MINUTES_MAX);
    else if (settingsIndex == 3) breakMinutes = clampi(breakMinutes + delta, MINUTES_MIN, MINUTES_MAX);
    if (oldLoop != loopCount || oldWork != workMinutes || oldBreak != breakMinutes) {
        stopAllTimers(); rebuildTimers();
    }
}
static void handleSettingsCrank(int change) {
    settingsCrankAccum += change;
    while (settingsCrankAccum >= CIRCLE) { adjustSetting(1); settingsCrankAccum -= CIRCLE; }
    while (settingsCrankAccum <= -CIRCLE) { adjustSetting(-1); settingsCrankAccum += CIRCLE; }
}
static void startOnly(TimerObj* timer) {
    if (!timer) return;
    timer_update(timer);
    if (timer == activeTimer && !timer->paused && timer->timeLeft > 0) return;
    loopRunning = 0;
    currentLoop = 1;
    loopPhase = mode;
    if (activeTimer && activeTimer != timer) { timer_pause(activeTimer); timer_reset(activeTimer); }
    activeTimer = timer;
    if (timer->paused || timer->timeLeft <= 0) { timer_reset(timer); timer_start(timer); }
}
static void startTimerForCurrentMode() { TimerObj* t = timerForMode(mode); if (t) startOnly(t); }
static void startLoopPhase(Mode phase) {
    TimerObj* timer = timerForMode(phase); if (!timer) return;
    loopPhase = phase;
    mode = phase;
    if (activeTimer && activeTimer != timer) { timer_pause(activeTimer); timer_reset(activeTimer); }
    activeTimer = timer;
    timer_reset(activeTimer);
    timer_start(activeTimer);
}
static void startLoop() { rebuildTimers(); loopRunning = 1; currentLoop = 1; startLoopPhase(MODE_WORKTIME); }
static void finishLoop() { stopAllTimers(); mode = MODE_CLOCK; }
static void stopFinishedTimer() { if (activeTimer) { timer_pause(activeTimer); timer_reset(activeTimer); } activeTimer = NULLPTR; }
static void playPromptSound() {
    if (pPlaySoundA && g_promptSoundPath[0]) {
        if (pPlaySoundA(g_promptSoundPath, NULLPTR, SND_FILENAME | SND_ASYNC | SND_NODEFAULT)) return;
    }
    if (pMessageBeep) pMessageBeep(0xFFFFFFFFu);
}

static void showPhaseAlert(const char* message, int next) {
    phaseAlertMessage = message;
    phaseAlertUntil = now_ms() + PHASE_ALERT_MS;
    phaseAlertNext = next;
    playPromptSound();
}
static void showLoopPhaseCompleteAlert() {
    Mode completedPhase = loopPhase;
    stopFinishedTimer();
    if (completedPhase == MODE_WORKTIME) { showPhaseAlert("WORK DONE", 1); return; }
    if (completedPhase == MODE_BREAKTIME) {
        if (currentLoop >= loopCount) showPhaseAlert("FINISHED", 3);
        else showPhaseAlert("BREAK DONE", 2);
    }
}
static void showSingleTimerCompleteAlert() {
    Mode completedMode = mode;
    stopFinishedTimer();
    if (completedMode == MODE_WORKTIME) showPhaseAlert("WORK DONE", 0);
    else if (completedMode == MODE_BREAKTIME) showPhaseAlert("BREAK DONE", 0);
    else showPhaseAlert("DONE", 0);
}
static void finishPhaseAlertIfNeeded(u64 n) {
    if (!phaseAlertMessage || n < phaseAlertUntil) return;
    int next = phaseAlertNext;
    clearPhaseAlert();
    if (next == 1) { if (loopRunning) startLoopPhase(MODE_BREAKTIME); }
    else if (next == 2) { if (loopRunning) { currentLoop++; startLoopPhase(MODE_WORKTIME); } }
    else if (next == 3) finishLoop();
}

// =====================
// Input
// =====================
static void readInput(InputState* input) {
    input->btnA = keyPressed['E'];
    input->btnAReleased = keyReleased['E'];
    input->btnAHeld = keyDown['E'];
    input->btnB = keyPressed['B'];
    input->up = keyPressed['W'];
    input->down = keyPressed['S'];
    input->left = keyPressed['A'];
    input->right = keyPressed['D'];
    input->crankChange = 0;
    if (keyPressed['V']) input->crankChange += CIRCLE;
    if (keyPressed['C']) input->crankChange -= CIRCLE;
}
static void clearInputEdges() { for (int i = 0; i < 256; i++) { keyPressed[i] = 0; keyReleased[i] = 0; } }

static int handleClockInput(InputState* input) {
    if (mode != MODE_CLOCK) return 0;
    Mode target = MODE_CLOCK; Direction dir = DIR_NONE;
    if (input->up) { target = loopRunning ? loopPhase : MODE_WORKTIME; dir = DIR_UP; }
    else if (input->down) { target = loopRunning ? loopPhase : MODE_BREAKTIME; dir = DIR_DOWN; }
    else if (input->left) { target = MODE_SETTINGS; dir = DIR_LEFT; }
    else if (input->right) { target = MODE_THEMES; dir = DIR_RIGHT; }
    if (dir != DIR_NONE) { startTransition(target, dir); return 1; }
    return 0;
}
static void handleSettingsSelection(InputState* input) {
    if (mode != MODE_SETTINGS) return;
    if (input->up) { settingsIndex = clampi(settingsIndex - 1, 1, 3); settingsCrankAccum = 0; }
    else if (input->down) { settingsIndex = clampi(settingsIndex + 1, 1, 3); settingsCrankAccum = 0; }
}
static int handleReturnInput(InputState* input) {
    if (mode == MODE_CLOCK) return 0;
    if (input->btnB) { startTransition(MODE_CLOCK, getReturnDirection(mode)); cancelSettingsHold(); return 1; }
    if (mode == MODE_WORKTIME && input->down) startTransition(MODE_CLOCK, DIR_DOWN);
    else if (mode == MODE_BREAKTIME && input->up) startTransition(MODE_CLOCK, DIR_UP);
    else if (mode == MODE_SETTINGS && input->right) startTransition(MODE_CLOCK, DIR_RIGHT);
    else if (mode == MODE_THEMES && input->left) startTransition(MODE_CLOCK, DIR_LEFT);
    else return 0;
    cancelSettingsHold(); return 1;
}
static void handleSettingsInput(InputState* input, u64 n) {
    handleSettingsCrank(input->crankChange);
    if (input->btnA) { settingsAHoldStart = n; settingsLongPressDone = 0; }
    if (input->btnAHeld && settingsAHoldStart && !settingsLongPressDone && n - settingsAHoldStart >= A_LONG_PRESS_MS) {
        resetSettingsToDefault(); settingsLongPressDone = 1;
    }
    if (input->btnAReleased && settingsAHoldStart) {
        if (!settingsLongPressDone) startLoop();
        cancelSettingsHold();
    }
}
static void handleThemeInput(InputState* input) { if (mode == MODE_THEMES && input->btnA) theme = (theme == THEME_LIGHT) ? THEME_DARK : THEME_LIGHT; }
static void resetActiveTimerByCrank(InputState* input) {
    if (mode != MODE_WORKTIME && mode != MODE_BREAKTIME) { timerResetCrankAccum = 0; return; }
    if (input->crankChange == 0) return;
    timerResetCrankAccum += (input->crankChange > 0) ? input->crankChange : -input->crankChange;
    while (timerResetCrankAccum >= CIRCLE) {
        timerResetCrankAccum -= CIRCLE;
        if (activeTimer) {
            timer_reset(activeTimer); activeTimer = NULLPTR; loopRunning = 0; clearPhaseAlert();
        }
    }
}
static void handleTimerInput(InputState* input) {
    if (input->btnA) startTimerForCurrentMode();
    resetActiveTimerByCrank(input);
}
static void handleInput() {
    InputState input; readInput(&input);
    u64 n = now_ms();
    if (phaseAlertMessage || transition.active) { cancelSettingsHold(); clearInputEdges(); return; }
    if (handleClockInput(&input)) { clearInputEdges(); return; }
    handleSettingsSelection(&input);
    if (handleReturnInput(&input)) { clearInputEdges(); return; }
    if (mode == MODE_SETTINGS) { handleSettingsInput(&input, n); clearInputEdges(); return; }
    if (mode == MODE_THEMES) { handleThemeInput(&input); clearInputEdges(); return; }
    handleTimerInput(&input);
    clearInputEdges();
}
static void updateTimerLogic() {
    timer_update(&workTimer); timer_update(&breakTimer);
    u64 n = now_ms();
    updateTransition(n);
    finishPhaseAlertIfNeeded(n);
    if (phaseAlertMessage) return;
    if (activeTimer) { timer_update(activeTimer); if (activeTimer->timeLeft <= 0) { if (loopRunning) showLoopPhaseCompleteAlert(); else showSingleTimerCompleteAlert(); } }
}

// =====================
// Drawing helpers
// =====================
static int px(int x) { return BORDER_PX + (g_drawOffsetX + x) * SCALE; }
static int py(int y) { return BORDER_PX + (g_drawOffsetY + y) * SCALE; }
static int ps(int v) { return v * SCALE; }
static int blinkVisible() { u64 t = now_ms() % BLINK_PERIOD_MS; return t < (u64)(BLINK_PERIOD_MS * BLINK_DUTY_PERCENT / 100); }
static COLORREF bgColor() { return theme == THEME_LIGHT ? COLOR_LIGHT_BG : COLOR_DARK_BG; }
static COLORREF inkColor() { return theme == THEME_LIGHT ? COLOR_LIGHT_INK : COLOR_DARK_INK; }
static COLORREF selectedInkColor() { return theme == THEME_LIGHT ? COLOR_LIGHT_BG : COLOR_DARK_BG; }

static HBRUSH makeBrush(COLORREF color) { return pCreateSolidBrush(color); }
static HPEN makePen(COLORREF color) { return pCreatePen(PS_SOLID, 1, color); }
static void fill_rect_client(HDC dc, int left, int top, int right, int bottom, COLORREF color) {
    RECT r; r.left = left; r.top = top; r.right = right; r.bottom = bottom;
    HBRUSH b = makeBrush(color); pFillRect(dc, &r, b); pDeleteObject(b);
}
static void fillRectLogical(HDC dc, int x, int y, int w, int h, COLORREF color) { fill_rect_client(dc, px(x), py(y), px(x+w), py(y+h), color); }

static void set_text(HDC dc, COLORREF color) { g_textColor = color; pSetTextColor(dc, color); pSetBkMode(dc, TRANSPARENT_BK); }
static void select_font(HDC dc, HFONT font) { pSelectObject(dc, font); }
static void drawText(HDC dc, const char* text, int x, int y) { pTextOutA(dc, px(x), py(y), text, c_strlen(text)); }
static void drawTextAligned(HDC dc, const char* text, int x, int y, int align) {
    SIZE sz; sz.cx = 0; sz.cy = 0;
    int len = c_strlen(text);
    pGetTextExtentPoint32A(dc, text, len, &sz);
    int tx = px(x);
    if (align == 1) tx -= sz.cx / 2;
    else if (align == 2) tx -= sz.cx;
    pTextOutA(dc, tx, py(y), text, len);
}

static void drawTextAlignedFit(HDC dc, const char* text, int x, int y, int align, int maxLogicalWidth) {
    SIZE sz; sz.cx = 0; sz.cy = 0;
    int len = c_strlen(text);
    pGetTextExtentPoint32A(dc, text, len, &sz);
    if (sz.cx > ps(maxLogicalWidth) && g_hintFont) {
        pSelectObject(dc, g_hintFont);
        pGetTextExtentPoint32A(dc, text, len, &sz);
    }
    int tx = px(x);
    if (align == 1) tx -= sz.cx / 2;
    else if (align == 2) tx -= sz.cx;
    int minX = px(4);
    int maxX = px(SCREEN_W - 4) - sz.cx;
    if (tx < minX) tx = minX;
    if (tx > maxX) tx = maxX;
    pTextOutA(dc, tx, py(y), text, len);
}

static void fillPolygonLogical(HDC dc, const int* pts, int count, COLORREF fill, COLORREF border) {
    POINT p[8];
    for (int i = 0; i < count; i++) { p[i].x = px(pts[i*2]); p[i].y = py(pts[i*2+1]); }
    HBRUSH b = makeBrush(fill); HPEN pen = makePen(border);
    HGDIOBJ oldB = pSelectObject(dc, b); HGDIOBJ oldP = pSelectObject(dc, pen);
    pPolygon(dc, p, count);
    pSelectObject(dc, oldB); pSelectObject(dc, oldP);
    pDeleteObject(b); pDeleteObject(pen);
}
static void drawClosedPolygon(HDC dc, const int* pts, int count, COLORREF color) {
    HPEN pen = makePen(color); HGDIOBJ oldP = pSelectObject(dc, pen);
    pMoveToEx(dc, px(pts[0]), py(pts[1]), NULLPTR);
    for (int i = 1; i < count; i++) pLineTo(dc, px(pts[i*2]), py(pts[i*2+1]));
    pLineTo(dc, px(pts[0]), py(pts[1]));
    pSelectObject(dc, oldP); pDeleteObject(pen);
}

static void drawDPadButton(HDC dc, const int* pts, int ptsCount, int active, const int* arrow, int arrowCount) {
    COLORREF fill, border, arrowColor;
    if (theme == THEME_LIGHT) {
        if (active) { fill = COLOR_LIGHT_INK; border = COLOR_LIGHT_INK; arrowColor = COLOR_LIGHT_BG; }
        else { fill = COLOR_LIGHT_BG; border = COLOR_LIGHT_INK; arrowColor = COLOR_LIGHT_INK; }
    } else {
        if (active) { fill = COLOR_DARK_INK; border = COLOR_DARK_INK; arrowColor = COLOR_DARK_BG; }
        else { fill = COLOR_DARK_BG; border = COLOR_DARK_INK; arrowColor = COLOR_DARK_INK; }
    }
    fillPolygonLogical(dc, pts, ptsCount, fill, border);
    drawClosedPolygon(dc, pts, ptsCount, border);
    fillPolygonLogical(dc, arrow, arrowCount, arrowColor, arrowColor);
}

static const int PAD_UP_POINTS[]    = {38,35, 30,27, 30,14, 46,14, 46,27};
static const int PAD_DOWN_POINTS[]  = {38,41, 46,49, 46,62, 30,62, 30,49};
static const int PAD_LEFT_POINTS[]  = {35,38, 27,46, 14,46, 14,30, 27,30};
static const int PAD_RIGHT_POINTS[] = {41,38, 49,30, 62,30, 62,46, 49,46};
static const int PAD_UP_ARROW[]     = {38,18, 35,24, 41,24};
static const int PAD_DOWN_ARROW[]   = {35,52, 41,52, 38,58};
static const int PAD_LEFT_ARROW[]   = {18,38, 24,35, 24,41};
static const int PAD_RIGHT_ARROW[]  = {58,38, 52,35, 52,41};

static void drawDPad(HDC dc, Mode activeMode) {
    drawDPadButton(dc, PAD_UP_POINTS, 5, activeMode == MODE_WORKTIME, PAD_UP_ARROW, 3);
    drawDPadButton(dc, PAD_DOWN_POINTS, 5, activeMode == MODE_BREAKTIME, PAD_DOWN_ARROW, 3);
    drawDPadButton(dc, PAD_LEFT_POINTS, 5, activeMode == MODE_SETTINGS, PAD_LEFT_ARROW, 3);
    drawDPadButton(dc, PAD_RIGHT_POINTS, 5, activeMode == MODE_THEMES, PAD_RIGHT_ARROW, 3);
}

static void drawClock(HDC dc) {
    SYSTEMTIME st; pGetLocalTime(&st);
    char timeText[32]; char dateText[64];
    fmt_time(timeText, &st); fmt_date(dateText, &st);
    select_font(dc, g_timeFont);
    set_text(dc, inkColor());
    drawTextAligned(dc, timeText, SCREEN_CENTER_X, 84, 1);
    select_font(dc, g_defaultFont);
    drawTextAligned(dc, dateText, SCREEN_CENTER_X, 180, 1);
}
static void drawTimer(HDC dc, TimerObj* timer) {
    char buf[64];
    if (!timer) return;
    timer_update(timer);
    fmt_timer(buf, max_i64(0, timer->timeLeft));
    select_font(dc, g_timerFont);
    set_text(dc, inkColor());
    drawTextAligned(dc, buf, SCREEN_CENTER_X, 84, 1);
    select_font(dc, g_defaultFont);
    if (loopRunning) {
        fmt_loop_text(buf, loopPhase == MODE_WORKTIME ? "WORK" : "BREAK", currentLoop, loopCount);
        drawTextAligned(dc, buf, SCREEN_CENTER_X, 190, 1);
    } else if (timer->paused && blinkVisible()) {
        drawTextAlignedFit(dc, "start: press E    reset: press C/V", SCREEN_CENTER_X, 190, 1, 360);
    }
}
static void fillRoundRectLogical(HDC dc, int x, int y, int w, int h, int r, COLORREF color) {
    HBRUSH b = makeBrush(color); HPEN pen = makePen(color);
    HGDIOBJ oldB = pSelectObject(dc, b); HGDIOBJ oldP = pSelectObject(dc, pen);
    pRoundRect(dc, px(x), py(y), px(x+w), py(y+h), ps(r), ps(r));
    pSelectObject(dc, oldB); pSelectObject(dc, oldP);
    pDeleteObject(b); pDeleteObject(pen);
}
static void drawSettingsRow(HDC dc, int index, const char* label, const char* value, int rowX, int rowY, int rowW, int rowH) {
    COLORREF normalColor = inkColor();
    if (settingsIndex == index) {
        fillRoundRectLogical(dc, rowX, rowY - 4, rowW, rowH, 4, normalColor);
        set_text(dc, selectedInkColor());
    } else {
        set_text(dc, normalColor);
    }
    drawText(dc, label, rowX + 10, rowY);
    drawTextAligned(dc, value, rowX + rowW - 10, rowY, 2);
}
static void drawTimerSettings(HDC dc) {
    select_font(dc, g_defaultFont);
    int rowX = 112, rowW = 176, rowH = 22, firstRowY = 76, rowGap = 30;
    char v1[16], v2[16], v3[16];
    fmt_int(v1, loopCount); fmt_setting_minutes(v2, workMinutes); fmt_setting_minutes(v3, breakMinutes);
    drawSettingsRow(dc, 1, "LOOP", v1, rowX, firstRowY, rowW, rowH);
    drawSettingsRow(dc, 2, "WORK", v2, rowX, firstRowY + rowGap, rowW, rowH);
    drawSettingsRow(dc, 3, "BREAK", v3, rowX, firstRowY + rowGap * 2, rowW, rowH);
    set_text(dc, inkColor());
    drawTextAlignedFit(dc, "press C/V to -/+", SCREEN_CENTER_X, 176, 1, 360);
    if (settingsMessage && now_ms() < settingsMessageUntil) drawTextAligned(dc, settingsMessage, SCREEN_CENTER_X, 198, 1);
    else drawTextAlignedFit(dc, "start: press E    reset: hold E", SCREEN_CENTER_X, 198, 1, 360);
}
static void drawThemeSettings(HDC dc) {
    select_font(dc, g_defaultFont);
    char buf[64]; int p = 0;
    p = append_str(buf, p, "Theme :  "); append_str(buf, p, themeName());
    set_text(dc, inkColor());
    drawTextAligned(dc, buf, SCREEN_CENTER_X, 110, 1);
    if (blinkVisible()) drawTextAlignedFit(dc, "press  E  to  switch", SCREEN_CENTER_X, 140, 1, 360);
}
static void drawModeScreen(HDC dc, Mode screenMode) {
    fillRectLogical(dc, 0, 0, SCREEN_W, SCREEN_H, bgColor());
    set_text(dc, inkColor());
    drawDPad(dc, screenMode);
    if (screenMode == MODE_CLOCK) drawClock(dc);
    else if (screenMode == MODE_WORKTIME) drawTimer(dc, &workTimer);
    else if (screenMode == MODE_BREAKTIME) drawTimer(dc, &breakTimer);
    else if (screenMode == MODE_SETTINGS) drawTimerSettings(dc);
    else if (screenMode == MODE_THEMES) drawThemeSettings(dc);
    select_font(dc, g_defaultFont);
    set_text(dc, inkColor());
    drawTextAligned(dc, modeName(screenMode), SCREEN_CENTER_X, 30, 1);
}
static void drawModeScreenAt(HDC dc, Mode screenMode, int ox, int oy) {
    int oldX = g_drawOffsetX, oldY = g_drawOffsetY;
    g_drawOffsetX = ox; g_drawOffsetY = oy;
    drawModeScreen(dc, screenMode);
    g_drawOffsetX = oldX; g_drawOffsetY = oldY;
}
static void drawPhaseAlert(HDC dc) {
    fillRectLogical(dc, 0, 0, SCREEN_W, SCREEN_H, bgColor());
    set_text(dc, inkColor());
    select_font(dc, g_timerFont);
    drawTextAligned(dc, phaseAlertMessage ? phaseAlertMessage : "DONE", SCREEN_CENTER_X, 60, 1);
    select_font(dc, g_defaultFont);
    if (phaseAlertMessage && c_streq(phaseAlertMessage, "FINISHED")) {
        char buf[64]; fmt_loop_only(buf, loopCount, loopCount); drawTextAligned(dc, buf, SCREEN_CENTER_X, 158, 1);
    } else if (loopRunning) {
        char buf[64]; fmt_loop_text(buf, loopPhase == MODE_WORKTIME ? "WORK" : "BREAK", currentLoop, loopCount); drawTextAligned(dc, buf, SCREEN_CENTER_X, 158, 1);
    }
}
static void drawOuterBorder(HDC dc) {
    // Repaint the border last so sliding screen content never leaks into the frame.
    fill_rect_client(dc, 0, 0, CLIENT_W, BORDER_PX, COLOR_BLACK);
    fill_rect_client(dc, 0, CLIENT_H - BORDER_PX, CLIENT_W, CLIENT_H, COLOR_BLACK);
    fill_rect_client(dc, 0, 0, BORDER_PX, CLIENT_H, COLOR_BLACK);
    fill_rect_client(dc, CLIENT_W - BORDER_PX, 0, CLIENT_W, CLIENT_H, COLOR_BLACK);
}
static void render(HDC dc) {
    fill_rect_client(dc, 0, 0, CLIENT_W, CLIENT_H, COLOR_BLACK);
    g_drawOffsetX = 0; g_drawOffsetY = 0;
    if (phaseAlertMessage) drawPhaseAlert(dc);
    else if (transition.active) {
        u64 elapsed64 = now_ms() - transition.start;
        int elapsed = (elapsed64 > (u64)TRANSITION_MS) ? TRANSITION_MS : (int)elapsed64;
        int oldOx = (transition.dx * elapsed) / TRANSITION_MS;
        int oldOy = (transition.dy * elapsed) / TRANSITION_MS;
        int newOx = (-transition.dx * (TRANSITION_MS - elapsed)) / TRANSITION_MS;
        int newOy = (-transition.dy * (TRANSITION_MS - elapsed)) / TRANSITION_MS;
        drawModeScreenAt(dc, transition.from, oldOx, oldOy);
        drawModeScreenAt(dc, transition.to, newOx, newOy);
    } else drawModeScreenAt(dc, mode, 0, 0);
    drawOuterBorder(dc);
}

static void applyRoundedWindowRegion(HWND hwnd) {
    // Prefer Windows DWM rounded corners for antialiased edges.
    // Fall back to CreateRoundRectRgn on older systems.
    if (pDwmSetWindowAttribute && hwnd) {
        int pref = DWMWCP_ROUND;
        if (pDwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &pref, sizeof(pref)) == 0) {
            return;
        }
    }

    if (!pCreateRoundRectRgn || !pSetWindowRgn || !hwnd) return;
    HRGN region = pCreateRoundRectRgn(0, 0, CLIENT_W + 1, CLIENT_H + 1, OUTER_RADIUS_PX * 2, OUTER_RADIUS_PX * 2);
    if (region) pSetWindowRgn(hwnd, region, TRUE);
}

static void setWindowOpacity(HWND hwnd, u8 alpha) {
    if (pSetLayeredWindowAttributes && hwnd) {
        pSetLayeredWindowAttributes(hwnd, 0, alpha, LWA_ALPHA);
    }
}

static HICON loadAppIcon(int size) {
    if (!pLoadImageA || !g_appIconPath[0]) return NULLPTR;
    return (HICON)pLoadImageA(NULLPTR, g_appIconPath, IMAGE_ICON, size, size, LR_LOADFROMFILE);
}

static void renderBuffered(HDC dc) {
    if (!pCreateCompatibleDC || !pCreateCompatibleBitmap || !pBitBlt || !pDeleteDC) {
        render(dc);
        return;
    }

    HDC memDC = pCreateCompatibleDC(dc);
    if (!memDC) {
        render(dc);
        return;
    }

    HBITMAP bmp = pCreateCompatibleBitmap(dc, CLIENT_W, CLIENT_H);
    if (!bmp) {
        pDeleteDC(memDC);
        render(dc);
        return;
    }

    HGDIOBJ oldBmp = pSelectObject(memDC, (HGDIOBJ)bmp);
    render(memDC);
    pBitBlt(dc, 0, 0, CLIENT_W, CLIENT_H, memDC, 0, 0, SRCCOPY);
    if (oldBmp) pSelectObject(memDC, oldBmp);
    pDeleteObject((HGDIOBJ)bmp);
    pDeleteDC(memDC);
}

// =====================
// Window procedure / init
// =====================
static LRESULT wndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_CREATE) {
        setWindowOpacity(hwnd, (u8)g_activeOpacityAlpha);
        pSetTimer(hwnd, 1, 16, NULLPTR);
        return 0;
    }
    if (msg == WM_ACTIVATE) {
        int isActive = ((int)(wParam & 0xFFFF)) != WA_INACTIVE;
        setWindowOpacity(hwnd, (u8)(isActive ? g_activeOpacityAlpha : g_inactiveOpacityAlpha));
        return 0;
    }
    if (msg == WM_ERASEBKGND) return 1;
    if (msg == WM_TIMER) {
        handleInput();
        updateTimerLogic();
        pInvalidateRect(hwnd, NULLPTR, FALSE);
        return 0;
    }
    if (msg == WM_KEYDOWN) {
        int vk = (int)(wParam & 0xFF);
        if (vk == VK_ESCAPE) { pPostQuitMessage(0); return 0; }
        if (vk >= 0 && vk < 256) {
            int wasDown = keyDown[vk];
            keyDown[vk] = 1;
            if (!wasDown) keyPressed[vk] = 1;
        }
        return 0;
    }
    if (msg == WM_KEYUP) {
        int vk = (int)(wParam & 0xFF);
        if (vk >= 0 && vk < 256) { keyDown[vk] = 0; keyReleased[vk] = 1; }
        return 0;
    }
    if (msg == WM_LBUTTONDOWN) {
        if (pSendMessageA) pSendMessageA(hwnd, WM_NCLBUTTONDOWN, (WPARAM)HTCAPTION, 0);
        return 0;
    }
    if (msg == WM_PAINT) {
        PAINTSTRUCT ps; HDC dc = pBeginPaint(hwnd, &ps);
        renderBuffered(dc);
        pEndPaint(hwnd, &ps);
        return 0;
    }
    if (msg == WM_DESTROY) {
        pKillTimer(hwnd, 1);
        pPostQuitMessage(0);
        return 0;
    }
    return pDefWindowProcA(hwnd, msg, wParam, lParam);
}

static void resolve_win32() {
    void* kernel32 = get_module_base("kernel32.dll");
    pLoadLibraryA = (PFN_LoadLibraryA)resolve_export(kernel32, "LoadLibraryA");
    pGetProcAddress = (PFN_GetProcAddress)resolve_export(kernel32, "GetProcAddress");
    pExitProcess = (PFN_ExitProcess)resolve_export(kernel32, "ExitProcess");
    pGetTickCount64 = (PFN_GetTickCount64)resolve_export(kernel32, "GetTickCount64");
    pGetLocalTime = (PFN_GetLocalTime)resolve_export(kernel32, "GetLocalTime");
    pGetPrivateProfileStringA = (PFN_GetPrivateProfileStringA)resolve_export(kernel32, "GetPrivateProfileStringA");
    pGetPrivateProfileIntA = (PFN_GetPrivateProfileIntA)resolve_export(kernel32, "GetPrivateProfileIntA");
    void* user32 = pLoadLibraryA("user32.dll");
    void* gdi32 = pLoadLibraryA("gdi32.dll");
    void* winmm = pLoadLibraryA("winmm.dll");
    void* dwmapi = pLoadLibraryA("dwmapi.dll");
    pRegisterClassExA = (PFN_RegisterClassExA)gp(user32, "RegisterClassExA");
    pCreateWindowExA = (PFN_CreateWindowExA)gp(user32, "CreateWindowExA");
    pShowWindow = (PFN_ShowWindow)gp(user32, "ShowWindow");
    pUpdateWindow = (PFN_UpdateWindow)gp(user32, "UpdateWindow");
    pGetMessageA = (PFN_GetMessageA)gp(user32, "GetMessageA");
    pTranslateMessage = (PFN_TranslateMessage)gp(user32, "TranslateMessage");
    pDispatchMessageA = (PFN_DispatchMessageA)gp(user32, "DispatchMessageA");
    pDefWindowProcA = (PFN_DefWindowProcA)gp(user32, "DefWindowProcA");
    pPostQuitMessage = (PFN_PostQuitMessage)gp(user32, "PostQuitMessage");
    pInvalidateRect = (PFN_InvalidateRect)gp(user32, "InvalidateRect");
    pBeginPaint = (PFN_BeginPaint)gp(user32, "BeginPaint");
    pEndPaint = (PFN_EndPaint)gp(user32, "EndPaint");
    pSetTimer = (PFN_SetTimer)gp(user32, "SetTimer");
    pKillTimer = (PFN_KillTimer)gp(user32, "KillTimer");
    pFillRect = (PFN_FillRect)gp(user32, "FillRect");
    pAdjustWindowRect = (PFN_AdjustWindowRect)gp(user32, "AdjustWindowRect");
    pLoadCursorA = (PFN_LoadCursorA)gp(user32, "LoadCursorA");
    pLoadImageA = (PFN_LoadImageA)gp(user32, "LoadImageA");
    pMessageBeep = (PFN_MessageBeep)gp(user32, "MessageBeep");
    pSendMessageA = (PFN_SendMessageA)gp(user32, "SendMessageA");
    pSetWindowRgn = (PFN_SetWindowRgn)gp(user32, "SetWindowRgn");
    pSetLayeredWindowAttributes = (PFN_SetLayeredWindowAttributes)gp(user32, "SetLayeredWindowAttributes");
    pDwmSetWindowAttribute = dwmapi ? (PFN_DwmSetWindowAttribute)gp(dwmapi, "DwmSetWindowAttribute") : NULLPTR;
    pCreateSolidBrush = (PFN_CreateSolidBrush)gp(gdi32, "CreateSolidBrush");
    pCreatePen = (PFN_CreatePen)gp(gdi32, "CreatePen");
    pSelectObject = (PFN_SelectObject)gp(gdi32, "SelectObject");
    pDeleteObject = (PFN_DeleteObject)gp(gdi32, "DeleteObject");
    pCreateCompatibleDC = (PFN_CreateCompatibleDC)gp(gdi32, "CreateCompatibleDC");
    pCreateCompatibleBitmap = (PFN_CreateCompatibleBitmap)gp(gdi32, "CreateCompatibleBitmap");
    pDeleteDC = (PFN_DeleteDC)gp(gdi32, "DeleteDC");
    pBitBlt = (PFN_BitBlt)gp(gdi32, "BitBlt");
    pPolygon = (PFN_Polygon)gp(gdi32, "Polygon");
    pMoveToEx = (PFN_MoveToEx)gp(gdi32, "MoveToEx");
    pLineTo = (PFN_LineTo)gp(gdi32, "LineTo");
    pRoundRect = (PFN_RoundRect)gp(gdi32, "RoundRect");
    pCreateRoundRectRgn = (PFN_CreateRoundRectRgn)gp(gdi32, "CreateRoundRectRgn");
    pSetBkMode = (PFN_SetBkMode)gp(gdi32, "SetBkMode");
    pSetTextColor = (PFN_SetTextColor)gp(gdi32, "SetTextColor");
    pTextOutA = (PFN_TextOutA)gp(gdi32, "TextOutA");
    pGetTextExtentPoint32A = (PFN_GetTextExtentPoint32A)gp(gdi32, "GetTextExtentPoint32A");
    pCreateFontA = (PFN_CreateFontA)gp(gdi32, "CreateFontA");
    pAddFontResourceExA = (PFN_AddFontResourceExA)gp(gdi32, "AddFontResourceExA");
    pPlaySoundA = winmm ? (PFN_PlaySoundA)gp(winmm, "PlaySoundA") : NULLPTR;
}

static void loadResourceConfig() {
    if (pGetPrivateProfileStringA) {
        pGetPrivateProfileStringA("fonts", "default", "Consolas", g_defaultFontFace, 80, ".\\resources.ini");
        pGetPrivateProfileStringA("fonts", "time", "Consolas", g_timeFontFace, 80, ".\\resources.ini");
        pGetPrivateProfileStringA("fonts", "timer", "Consolas", g_timerFontFace, 80, ".\\resources.ini");
        pGetPrivateProfileStringA("font_files", "default", "", g_defaultFontFile, 160, ".\\resources.ini");
        pGetPrivateProfileStringA("font_files", "time", "", g_timeFontFile, 160, ".\\resources.ini");
        pGetPrivateProfileStringA("font_files", "timer", "", g_timerFontFile, 160, ".\\resources.ini");
        pGetPrivateProfileStringA("sound", "prompt", "sounds\\prompt_tone.wav", g_promptSoundPath, 160, ".\\resources.ini");
        pGetPrivateProfileStringA("icon", "app", "icon\\app.ico", g_appIconPath, 160, ".\\resources.ini");
        pGetPrivateProfileStringA("icon", "path", g_appIconPath, g_appIconPath, 160, ".\\resources.ini");
    }
    if (pGetPrivateProfileIntA) {
        g_defaultFontPx = clampi((int)pGetPrivateProfileIntA("font_sizes", "default", 11, ".\\resources.ini"), 6, 40);
        g_timeFontPx = clampi((int)pGetPrivateProfileIntA("font_sizes", "time", 29, ".\\resources.ini"), 12, 90);
        g_timerFontPx = clampi((int)pGetPrivateProfileIntA("font_sizes", "timer", 43, ".\\resources.ini"), 20, 120);

        g_defaultFontWeight = clampi((int)pGetPrivateProfileIntA("font_weights", "default", FW_BOLD, ".\\resources.ini"), 100, 900);
        g_timeFontWeight = clampi((int)pGetPrivateProfileIntA("font_weights", "time", FW_BOLD, ".\\resources.ini"), 100, 900);
        g_timerFontWeight = clampi((int)pGetPrivateProfileIntA("font_weights", "timer", FW_BOLD, ".\\resources.ini"), 100, 900);

        g_defaultFontItalic = (DWORD)clampi((int)pGetPrivateProfileIntA("font_italic", "default", 0, ".\\resources.ini"), 0, 1);
        g_timeFontItalic = (DWORD)clampi((int)pGetPrivateProfileIntA("font_italic", "time", 0, ".\\resources.ini"), 0, 1);
        g_timerFontItalic = (DWORD)clampi((int)pGetPrivateProfileIntA("font_italic", "timer", 0, ".\\resources.ini"), 0, 1);

        g_defaultFontQuality = (DWORD)clampi((int)pGetPrivateProfileIntA("font_quality", "default", NONANTIALIASED_QUALITY, ".\\resources.ini"), 0, 5);
        g_timeFontQuality = (DWORD)clampi((int)pGetPrivateProfileIntA("font_quality", "time", NONANTIALIASED_QUALITY, ".\\resources.ini"), 0, 5);
        g_timerFontQuality = (DWORD)clampi((int)pGetPrivateProfileIntA("font_quality", "timer", NONANTIALIASED_QUALITY, ".\\resources.ini"), 0, 5);

        g_activeOpacityAlpha = clampi((int)pGetPrivateProfileIntA("window", "active_alpha", APP_OPACITY_ACTIVE, ".\\resources.ini"), 30, 255);
        g_inactiveOpacityAlpha = clampi((int)pGetPrivateProfileIntA("window", "inactive_alpha", APP_OPACITY_INACTIVE, ".\\resources.ini"), 30, 255);
        int inactivePercent = (int)pGetPrivateProfileIntA("window", "inactive_opacity_percent", -1, ".\\resources.ini");
        if (inactivePercent >= 0 && inactivePercent <= 100) {
            g_inactiveOpacityAlpha = clampi((inactivePercent * 255) / 100, 30, 255);
        }
        g_alwaysOnTop = clampi((int)pGetPrivateProfileIntA("window", "always_on_top", 1, ".\\resources.ini"), 0, 1);
    }
}

static void loadPrivateFonts() {
    if (!pAddFontResourceExA) return;

    // Optional per-role font files from resources.ini.
    // Example:
    // [font_files]
    // default=font\Nontendo\Nontendo-Bold.ttf
    // time=font\Pixel-font\PixelFont.ttf
    // timer=font\Pixel-font\PixelFont-60.ttf
    if (g_defaultFontFile[0]) pAddFontResourceExA(g_defaultFontFile, FR_PRIVATE, NULLPTR);
    if (g_timeFontFile[0]) pAddFontResourceExA(g_timeFontFile, FR_PRIVATE, NULLPTR);
    if (g_timerFontFile[0]) pAddFontResourceExA(g_timerFontFile, FR_PRIVATE, NULLPTR);

    pAddFontResourceExA("font\\Curberick\\Curberick-Bold.ttf", FR_PRIVATE, NULLPTR);

    pAddFontResourceExA("fonts\\PixelFont.ttf", FR_PRIVATE, NULLPTR);
    pAddFontResourceExA("fonts\\PixelFont.otf", FR_PRIVATE, NULLPTR);
    pAddFontResourceExA("fonts\\PixelFont-60.ttf", FR_PRIVATE, NULLPTR);
    pAddFontResourceExA("fonts\\PixelFont-60.otf", FR_PRIVATE, NULLPTR);
    pAddFontResourceExA("font\\Pixel-font\\PixelFont.ttf", FR_PRIVATE, NULLPTR);
    pAddFontResourceExA("font\\Pixel-font\\PixelFont.otf", FR_PRIVATE, NULLPTR);
    pAddFontResourceExA("font\\Pixel-font\\PixelFont-60.ttf", FR_PRIVATE, NULLPTR);
    pAddFontResourceExA("font\\Pixel-font\\PixelFont-60.otf", FR_PRIVATE, NULLPTR);
}

static void createFonts() {
    loadPrivateFonts();
    // Original Lua uses the small system bold font for labels/prompts, and large fonts only for the clock/timer numbers.
    g_defaultFont = pCreateFontA(-g_defaultFontPx, 0, 0, 0, g_defaultFontWeight, g_defaultFontItalic,0,0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, g_defaultFontQuality, FIXED_PITCH | FF_MODERN, g_defaultFontFace);
    int hintPx = g_defaultFontPx > 12 ? 12 : g_defaultFontPx;
    g_hintFont = pCreateFontA(-hintPx, 0, 0, 0, g_defaultFontWeight, g_defaultFontItalic,0,0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, g_defaultFontQuality, FIXED_PITCH | FF_MODERN, g_defaultFontFace);
    g_timeFont = pCreateFontA(-g_timeFontPx, 0, 0, 0, g_timeFontWeight, g_timeFontItalic,0,0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, g_timeFontQuality, FIXED_PITCH | FF_MODERN, g_timeFontFace);
    g_timerFont = pCreateFontA(-g_timerFontPx, 0, 0, 0, g_timerFontWeight, g_timerFontItalic,0,0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, g_timerFontQuality, FIXED_PITCH | FF_MODERN, g_timerFontFace);
}

extern "C" void entry(void) {
    resolve_win32();
    loadResourceConfig();
    rebuildTimers();
    createFonts();

    HICON appIcon = loadAppIcon(32);
    HICON appIconSmall = loadAppIcon(16);

    WNDCLASSEXA wc;
    wc.cbSize = sizeof(WNDCLASSEXA);
    wc.style = 0;
    wc.lpfnWndProc = wndProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = NULLPTR;
    wc.hIcon = appIcon;
    wc.hCursor = pLoadCursorA ? pLoadCursorA(NULLPTR, IDC_ARROW) : NULLPTR;
    wc.hbrBackground = NULLPTR;
    wc.lpszMenuName = NULLPTR;
    wc.lpszClassName = "PlaydateTimerWinClass";
    wc.hIconSm = appIconSmall ? appIconSmall : appIcon;
    pRegisterClassExA(&wc);

    int winW = CLIENT_W;
    int winH = CLIENT_H;
    DWORD exStyle = WS_EX_LAYERED | WS_EX_APPWINDOW;
    if (g_alwaysOnTop) exStyle |= WS_EX_TOPMOST;
    g_hwnd = pCreateWindowExA(exStyle, "PlaydateTimerWinClass", "Playdate Timer - Windows", WS_POPUP,
                             CW_USEDEFAULT, CW_USEDEFAULT, winW, winH, NULLPTR, NULLPTR, NULLPTR, NULLPTR);
    if (pSendMessageA) {
        if (appIcon) pSendMessageA(g_hwnd, WM_SETICON, ICON_BIG, (LPARAM)appIcon);
        if (appIconSmall || appIcon) pSendMessageA(g_hwnd, WM_SETICON, ICON_SMALL, (LPARAM)(appIconSmall ? appIconSmall : appIcon));
    }
    applyRoundedWindowRegion(g_hwnd);
    pShowWindow(g_hwnd, SW_SHOW);
    pUpdateWindow(g_hwnd);

    MSG msg;
    while (pGetMessageA(&msg, NULLPTR, 0, 0) > 0) {
        pTranslateMessage(&msg);
        pDispatchMessageA(&msg);
    }
    if (pExitProcess) pExitProcess((UINT)msg.wParam);
    for (;;) {}
}
