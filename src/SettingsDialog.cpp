#include "SettingsDialog.h"
#include <commctrl.h>
#include <array>
#include <string>

namespace {
constexpr wchar_t kClass[] = L"ChaqRecSettingsWindow";
HWND gDialog = nullptr;
Settings* gSettings = nullptr;
std::function<void()> gOnChanged;
std::array<HWND, 4> gTracks{};
std::array<HWND, 4> gValues{};
const wchar_t* labelsEs[] = {L"Sensibilidad", L"Suavizado", L"Filtro de ruido", L"Opacidad"};
const wchar_t* labelsEn[] = {L"Sensitivity", L"Smoothing", L"Noise gate", L"Opacity"};
int mins[] = {1, 0, 0, 30};
int maxs[] = {10, 100, 30, 100};

int GetValue(int i) {
    if (!gSettings) return 0;
    switch (i) {
        case 0: return gSettings->sensitivityLevel;
        case 1: return gSettings->smoothing;
        case 2: return gSettings->noiseGate;
        default: return gSettings->opacity;
    }
}
void PutValue(int i, int n) {
    if (!gSettings) return;
    switch (i) {
        case 0: gSettings->sensitivityLevel = n; break;
        case 1: gSettings->smoothing = n; break;
        case 2: gSettings->noiseGate = n; break;
        default: gSettings->opacity = n; break;
    }
}
void UpdateValueLabel(int i) {
    if (!gValues[i]) return;
    std::wstring value = std::to_wstring(GetValue(i));
    if (i > 0) value += L"%";
    else value += L" / 10";
    SetWindowTextW(gValues[i], value.c_str());
}
LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_CREATE: {
            const bool spanish = gSettings && gSettings->language == L"es";
            for (int i = 0; i < 4; ++i) {
                const int y = 22 + i * 68;
                CreateWindowExW(0, L"STATIC", spanish ? labelsEs[i] : labelsEn[i],
                    WS_CHILD | WS_VISIBLE, 22, y, 245, 19, hwnd, nullptr, nullptr, nullptr);
                gValues[i] = CreateWindowExW(0, L"STATIC", L"",
                    WS_CHILD | WS_VISIBLE | SS_RIGHT, 300, y, 108, 19,
                    hwnd, nullptr, nullptr, nullptr);
                gTracks[i] = CreateWindowExW(0, TRACKBAR_CLASSW, L"",
                    WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS | TBS_HORZ,
                    17, y + 19, 396, 31, hwnd, nullptr, nullptr, nullptr);
                SendMessageW(gTracks[i], TBM_SETRANGE, TRUE, MAKELPARAM(mins[i], maxs[i]));
                SendMessageW(gTracks[i], TBM_SETTICFREQ, (i == 0 ? 1 : 10), 0);
                SendMessageW(gTracks[i], TBM_SETPOS, TRUE, GetValue(i));
                UpdateValueLabel(i);
            }
            CreateWindowExW(0, L"STATIC",
                spanish ? L"Solo cambia el visualizador; no modifica el volumen grabado."
                        : L"Only affects visualization, not the recorded volume.",
                WS_CHILD | WS_VISIBLE, 22, 300, 405, 20, hwnd, nullptr, nullptr, nullptr);
            return 0;
        }
        case WM_HSCROLL:
            for (int i = 0; i < 4; ++i) {
                if (reinterpret_cast<HWND>(lp) == gTracks[i]) {
                    PutValue(i, static_cast<int>(SendMessageW(gTracks[i], TBM_GETPOS, 0, 0)));
                    UpdateValueLabel(i);
                    gSettings->Save();
                    if (gOnChanged) gOnChanged();
                    break;
                }
            }
            return 0;
        case WM_CLOSE: DestroyWindow(hwnd); return 0;
        case WM_NCDESTROY:
            gDialog = nullptr;
            gTracks = {}; gValues = {};
            return DefWindowProcW(hwnd, msg, wp, lp);
        default: return DefWindowProcW(hwnd, msg, wp, lp);
    }
}
}

void ShowSettingsDialog(Settings& settings, const std::function<void()>& changed) {
    gSettings = &settings;
    gOnChanged = changed;
    if (gDialog && IsWindow(gDialog)) {
        SetForegroundWindow(gDialog);
        return;
    }
    INITCOMMONCONTROLSEX cc{sizeof(INITCOMMONCONTROLSEX), ICC_BAR_CLASSES};
    InitCommonControlsEx(&cc);
    WNDCLASSEXW cls{};
    cls.cbSize = sizeof(cls);
    cls.hInstance = GetModuleHandleW(nullptr);
    cls.lpfnWndProc = WindowProc;
    cls.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    cls.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    cls.lpszClassName = kClass;
    RegisterClassExW(&cls);
    gDialog = CreateWindowExW(WS_EX_TOOLWINDOW, kClass,
        settings.language == L"es" ? L"ChaqRec · Ajustes visuales" : L"ChaqRec · Visual settings",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
        CW_USEDEFAULT, CW_USEDEFAULT, 450, 375,
        nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    if (gDialog) {
        ShowWindow(gDialog, SW_SHOWNORMAL);
        UpdateWindow(gDialog);
        SetForegroundWindow(gDialog);
    }
}
