#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <objbase.h>
#include <windowsx.h>
#include <shellapi.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <string>

#include "Audio.h"
#include "Settings.h"
#include "SettingsDialog.h"
#include "Renderer.h"
#include "Transcriber.h"
#include "resources.h"

namespace {
constexpr wchar_t kOverlayClass[] = L"ChaqRecVisualizerWindow";
constexpr wchar_t kControllerClass[] = L"ChaqRecTrayController";
constexpr UINT WM_CHAQ_TRAY = WM_APP + 12;
constexpr UINT kTrayId = 1;
constexpr UINT_PTR kFrameTimer = 1;
constexpr UINT_PTR kKeepVisibleTimer = 2;

enum MenuCommand : UINT {
    CMD_RECORD = 100, CMD_FOLDER, CMD_FORMAT_M4A, CMD_FORMAT_WAV,
    CMD_MODE_RECORD, CMD_MODE_TRANSCRIBE, CMD_MODE_BOTH, CMD_OPEN_TXT,
    CMD_SETTINGS, CMD_SHOW, CMD_PAUSE, CMD_LOCK, CMD_TOP,
    CMD_SYSTEM, CMD_MIC, CMD_SOURCE_BOTH, CMD_BARS, CMD_WAVE, CMD_SPECTRUM,
    CMD_SMALL, CMD_MEDIUM, CMD_LARGE,
    CMD_COLOR_CYAN, CMD_COLOR_VIOLET, CMD_COLOR_WHITE, CMD_COLOR_GREEN,
    CMD_COLOR_NAVY, CMD_COLOR_ELECTRIC,
    CMD_OPACITY_50, CMD_OPACITY_70, CMD_OPACITY_90, CMD_OPACITY_100,
    CMD_STARTUP, CMD_LANG_ES, CMD_LANG_EN,
    CMD_ABOUT_INFO, CMD_GITHUB, CMD_DONATE, CMD_QUIT
};

enum class Tr {
    StartRecord, StopRecord, Recordings, Format, M4A, WAV, Adjust,
    Visualizer, Show, Hide, Pause, Continue, Lock, Unlock, Topmost,
    Source, System, Mic, Style, Bars, Wave, Spectrum,
    Size, Small, Medium, Large, Color, Cyan, Violet, White, Green, Navy, Electric,
    Opacity, Startup, Language, About, Exit, ErrorRecord,
    ErrorStartup, VersionInfo, Hint,
    BothAudio, RecordAction, RecordOnly, TranscribeOnly, RecordAndTranscribe,
    AboutInfo, GitHubLink, DonateLink, DownloadConsent,
    TranscriptionBusy, TranscriptionStarting, TranscriptionDownloading,
    TranscriptionProcessing, TranscriptionCompleted, OpenText
};

struct Translation { const wchar_t* es; const wchar_t* en; };
constexpr Translation kWords[] = {
    {L"Iniciar grabación del micrófono", L"Start microphone recording"},
    {L"Detener grabación", L"Stop recording"},
    {L"Abrir carpeta de grabaciones", L"Open recordings folder"},
    {L"Formato de grabación", L"Recording format"},
    {L"M4A comprimido (recomendado)", L"Compressed M4A (recommended)"},
    {L"WAV sin compresión", L"Uncompressed WAV"},
    {L"Ajustar sensibilidad y transparencia...", L"Adjust sensitivity and transparency..."},
    {L"Visualizador", L"Visualizer"},
    {L"Mostrar", L"Show"},
    {L"Ocultar", L"Hide"},
    {L"Pausar animación", L"Pause animation"},
    {L"Reanudar animación", L"Resume animation"},
    {L"Bloquear posición / atravesar clics", L"Lock position / click-through"},
    {L"Desbloquear posición", L"Unlock position"},
    {L"Siempre visible (encima)", L"Always on top"},
    {L"Audio del visualizador", L"Visualizer audio source"},
    {L"Sonido de Windows", L"Windows system audio"},
    {L"Micrófono", L"Microphone"},
    {L"Estilo", L"Style"},
    {L"Barras redondeadas", L"Rounded bars"},
    {L"Onda suave", L"Smooth wave"},
    {L"Espectro fluido", L"Flowing spectrum"},
    {L"Tamaño", L"Size"},
    {L"Pequeño", L"Small"},
    {L"Mediano", L"Medium"},
    {L"Grande", L"Large"},
    {L"Color", L"Color"},
    {L"Cian", L"Cyan"},
    {L"Violeta", L"Violet"},
    {L"Blanco", L"White"},
    {L"Verde", L"Green"},
    {L"Azul oscuro", L"Dark blue"},
    {L"Azul eléctrico", L"Electric blue"},
    {L"Opacidad", L"Opacity"},
    {L"Iniciar con Windows", L"Start with Windows"},
    {L"Idioma", L"Language"},
    {L"Acerca de ChaqRec", L"About ChaqRec"},
    {L"Salir", L"Exit"},
    {L"No se pudo completar la grabación. Revisa el micrófono y el espacio libre. "
     L"Si falló al cerrar un M4A, puede que el archivo no se reproduzca.",
     L"Could not complete the recording. Check microphone access and free disk space. "
     L"If M4A finalization failed, the file might not play."},
    {L"No se pudo actualizar el inicio automático.", L"Could not change autostart."},
    {L"ChaqRec 0.3.0\nVisualizador elegante, grabador y transcripción local opcional.\n"
     L"M4A: AAC mono 48 kHz / 96 kbps; WAV opcional.\n"
     L"Los archivos se guardan solo en tu equipo.\n\n"
     L"Arrastra para mover; bordes para tamaño.\n"
     L"Graba el micrófono; el visualizador puede mostrar ambas fuentes.\n"
     L"La transcripción usa Whisper local después de grabar.",
     L"ChaqRec 0.3.0\nElegant visualizer, recorder and optional offline transcription.\n"
     L"M4A: AAC mono 48 kHz / 96 kbps; optional WAV.\n"
     L"Files stay on your computer.\n\n"
     L"Drag to move; drag edges to resize.\n"
     L"Records microphone; visualizer can monitor both sources.\n"
     L"Transcription runs locally after recording."},
    {L"Arrastra para mover · bordes para tamaño · clic derecho para opciones",
     L"Drag to move · drag edges to resize · right-click for settings"},
    {L"Ambos (Windows + micrófono)", L"Both (Windows + microphone)"},
    {L"Al detener la grabación", L"When recording stops"},
    {L"Solo grabar audio", L"Record audio only"},
    {L"Solo transcribir (TXT)", L"Transcribe only (TXT)"},
    {L"Grabar y transcribir (audio + TXT)", L"Record and transcribe (audio + TXT)"},
    {L"Información de ChaqRec", L"About ChaqRec"},
    {L"Ver proyecto en GitHub", L"View project on GitHub"},
    {L"Apoyar el proyecto (PayPal)", L"Support the project (PayPal)"},
    {L"La transcripción utiliza Whisper.cpp y un modelo multilingüe gratuito.\n\n"
     L"La primera vez descargará aproximadamente 148 MB desde Hugging Face.\n"
     L"Se guardará en tu equipo para reutilizarlo sin conexión.\n"
     L"Whisper se ejecutará como proceso separado después de detener la grabación;\n"
     L"puede utilizar varios cientos de MB de RAM mientras transcribe.\n\n"
     L"¿Deseas habilitar esta opción?",
     L"Transcription uses Whisper.cpp and a free multilingual model.\n\n"
     L"The first use downloads about 148 MB from Hugging Face.\n"
     L"The model stays on your PC for offline reuse.\n"
     L"Whisper starts in a separate process after recording stops and\n"
     L"may use several hundred MB of RAM while transcribing.\n\n"
     L"Enable this option?"},
    {L"Termina la transcripción anterior antes de iniciar otra grabación.",
     L"Finish the previous transcription before starting another recording."},
    {L"Preparando la transcripción local...", L"Preparing local transcription..."},
    {L"Descargando y verificando modelo gratuito (148 MB)...",
     L"Downloading and verifying free model (148 MB)..."},
    {L"Transcribiendo fuera de ChaqRec (Whisper local)...",
     L"Transcribing outside ChaqRec (local Whisper)..."},
    {L"Transcripción lista. Usa 'Abrir última transcripción' en el menú.",
     L"Transcript ready. Use 'Open last transcript' in the menu."},
    {L"Abrir última transcripción (TXT)", L"Open last transcript (TXT)"}
};
static_assert(std::size(kWords) == static_cast<size_t>(Tr::OpenText) + 1);

Settings gSettings;
AudioMonitor gMonitor;
MicrophoneRecorder gRecorder;
Transcriber gTranscriber;
int gSessionRecordMode = 0;
AudioSnapshot gShown{};
Renderer gRenderer;
HWND gController = nullptr;
HWND gOverlay = nullptr;
HINSTANCE gInstance = nullptr;
HICON gIcon = nullptr;
HICON gRecordingIcon = nullptr;
bool gRecordingActive = false;
bool gMonitorActive = false;
bool gTimerActive = false;

const wchar_t* Text(Tr index) {
    const auto& word = kWords[static_cast<size_t>(index)];
    return gSettings.language == L"en" ? word.en : word.es;
}

void SaveGeometry() {
    if (!gOverlay) return;
    RECT r{};
    if (!GetWindowRect(gOverlay, &r)) return;
    gSettings.x = r.left; gSettings.y = r.top;
    gSettings.width = r.right - r.left;
    gSettings.height = r.bottom - r.top;
    gSettings.Save();
}

void SyncAudioAndTimer() {
    const bool active = gSettings.visible && !gSettings.paused;
    if (active != gMonitorActive) {
        if (active) gMonitor.Start(static_cast<AudioSource>(gSettings.source));
        else gMonitor.Stop();
        gMonitorActive = active;
    }
    if (active != gTimerActive) {
        if (active) SetTimer(gController, kFrameTimer, 33, nullptr); // 30 fps max
        else KillTimer(gController, kFrameTimer);
        gTimerActive = active;
    }
    if (gOverlay) InvalidateRect(gOverlay, nullptr, FALSE);
}

void UpdateClickThrough() {
    if (!gOverlay) return;
    LONG_PTR flags = GetWindowLongPtrW(gOverlay, GWL_EXSTYLE);
    if (gSettings.locked) flags |= WS_EX_TRANSPARENT;
    else flags &= ~static_cast<LONG_PTR>(WS_EX_TRANSPARENT);
    SetWindowLongPtrW(gOverlay, GWL_EXSTYLE, flags);
    SetWindowPos(gOverlay, nullptr, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
}

void UpdateTopmost() {
    if (!gOverlay) return;
    SetWindowPos(gOverlay, gSettings.topmost ? HWND_TOPMOST : HWND_NOTOPMOST,
                 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
}

void ApplyVisibility() {
    if (gOverlay) ShowWindow(gOverlay, gSettings.visible ? SW_SHOWNOACTIVATE : SW_HIDE);
    SyncAudioAndTimer();
    gSettings.Save();
}

void UpdateTrayIcon(bool add = false) {
    if (!gController) return;
    NOTIFYICONDATAW nid{};
    nid.cbSize = sizeof(nid);
    nid.hWnd = gController;
    nid.uID = kTrayId;
    nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    nid.uCallbackMessage = WM_CHAQ_TRAY;
    nid.hIcon = gRecordingActive ? gRecordingIcon : gIcon;
    wcscpy_s(nid.szTip, L"ChaqRec");
    if (add) {
        if (Shell_NotifyIconW(NIM_ADD, &nid)) {
            nid.uVersion = NOTIFYICON_VERSION_4;
            Shell_NotifyIconW(NIM_SETVERSION, &nid);
        }
    } else Shell_NotifyIconW(NIM_MODIFY, &nid);
}

void StopRecording(bool startTranscription = true) {
    const bool hadRecording = gRecordingActive;
    gRecorder.Stop();
    gRecordingActive = false;
    UpdateTrayIcon();
    if (gOverlay) InvalidateRect(gOverlay, nullptr, FALSE);
    if (startTranscription && hadRecording && gSessionRecordMode != 0) {
        const std::wstring audio = gRecorder.LastFile();
        if (!audio.empty() && GetFileAttributesW(audio.c_str()) != INVALID_FILE_ATTRIBUTES) {
            gTranscriber.Start(gController, audio, gSessionRecordMode == 1);
        }
    }
}

void ShowBalloon(const wchar_t* message) {
    if (!gController) return;
    NOTIFYICONDATAW icon{};
    icon.cbSize = sizeof(icon);
    icon.hWnd = gController;
    icon.uID = kTrayId;
    icon.uFlags = NIF_INFO;
    wcscpy_s(icon.szInfoTitle, L"ChaqRec");
    wcsncpy_s(icon.szInfo, message, _TRUNCATE);
    Shell_NotifyIconW(NIM_MODIFY, &icon);
}

// Win+D can temporarily minimize/hide layered desktop overlays. Reassert the
// persisted user visibility, but never override explicit 'Hide' or a disabled
// 'Always on top' preference. No audio work is done in this watchdog.
void KeepOverlayOnDesktop() {
    if (!gOverlay || !gSettings.visible || !gSettings.topmost) return;
    if (IsIconic(gOverlay)) ShowWindow(gOverlay, SW_RESTORE);
    if (!IsWindowVisible(gOverlay)) ShowWindow(gOverlay, SW_SHOWNOACTIVATE);
    SetWindowPos(gOverlay, HWND_TOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
}

void ResizeOverlay(int width, int height) {
    if (!gOverlay) return;
    RECT r{}; GetWindowRect(gOverlay, &r);
    // Preserve the visualizer's center when switching between presets.
    const int x = r.left + (r.right - r.left - width) / 2;
    const int y = r.top + (r.bottom - r.top - height) / 2;
    SetWindowPos(gOverlay, nullptr, x, y, width, height,
                 SWP_NOZORDER | SWP_NOACTIVATE);
    SaveGeometry();
}

void MenuItem(HMENU menu, UINT id, Tr label, bool checked = false) {
    AppendMenuW(menu, MF_STRING | (checked ? MF_CHECKED : 0), id, Text(label));
}

void SubMenu(HMENU parent, HMENU sub, Tr label) {
    AppendMenuW(parent, MF_POPUP, reinterpret_cast<UINT_PTR>(sub), Text(label));
}

void ShowMenu(HWND owner) {
    HMENU menu = CreatePopupMenu();
    HMENU visual = CreatePopupMenu();
    HMENU audio = CreatePopupMenu();
    HMENU style = CreatePopupMenu();
    HMENU size = CreatePopupMenu();
    HMENU color = CreatePopupMenu();
    HMENU opacity = CreatePopupMenu();
    HMENU format = CreatePopupMenu();
    HMENU language = CreatePopupMenu();
    HMENU mode = CreatePopupMenu();
    HMENU about = CreatePopupMenu();

    MenuItem(menu, CMD_RECORD, gRecorder.Requested() ? Tr::StopRecord : Tr::StartRecord);
    MenuItem(menu, CMD_FOLDER, Tr::Recordings);
    MenuItem(format, CMD_FORMAT_M4A, Tr::M4A, gSettings.recordFormat == 0);
    MenuItem(format, CMD_FORMAT_WAV, Tr::WAV, gSettings.recordFormat == 1);
    SubMenu(menu, format, Tr::Format);
    MenuItem(mode, CMD_MODE_RECORD, Tr::RecordOnly, gSettings.recordMode == 0);
    MenuItem(mode, CMD_MODE_TRANSCRIBE, Tr::TranscribeOnly, gSettings.recordMode == 1);
    MenuItem(mode, CMD_MODE_BOTH, Tr::RecordAndTranscribe, gSettings.recordMode == 2);
    SubMenu(menu, mode, Tr::RecordAction);
    const std::wstring txt = gTranscriber.LastTextFile();
    if (!txt.empty()) MenuItem(menu, CMD_OPEN_TXT, Tr::OpenText);
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);

    MenuItem(visual, CMD_SHOW, gSettings.visible ? Tr::Hide : Tr::Show);
    MenuItem(visual, CMD_PAUSE, gSettings.paused ? Tr::Continue : Tr::Pause);
    MenuItem(visual, CMD_LOCK, gSettings.locked ? Tr::Unlock : Tr::Lock);
    MenuItem(visual, CMD_TOP, Tr::Topmost, gSettings.topmost);
    AppendMenuW(visual, MF_SEPARATOR, 0, nullptr);
    MenuItem(audio, CMD_SYSTEM, Tr::System, gSettings.source == 0);
    MenuItem(audio, CMD_MIC, Tr::Mic, gSettings.source == 1);
    MenuItem(audio, CMD_SOURCE_BOTH, Tr::BothAudio, gSettings.source == 2);
    SubMenu(visual, audio, Tr::Source);
    MenuItem(style, CMD_BARS, Tr::Bars, gSettings.visualStyle == 0);
    MenuItem(style, CMD_WAVE, Tr::Wave, gSettings.visualStyle == 1);
    MenuItem(style, CMD_SPECTRUM, Tr::Spectrum, gSettings.visualStyle == 2);
    SubMenu(visual, style, Tr::Style);
    MenuItem(size, CMD_SMALL, Tr::Small);
    MenuItem(size, CMD_MEDIUM, Tr::Medium);
    MenuItem(size, CMD_LARGE, Tr::Large);
    SubMenu(visual, size, Tr::Size);
    MenuItem(color, CMD_COLOR_CYAN, Tr::Cyan, gSettings.color == 0);
    MenuItem(color, CMD_COLOR_VIOLET, Tr::Violet, gSettings.color == 1);
    MenuItem(color, CMD_COLOR_WHITE, Tr::White, gSettings.color == 2);
    MenuItem(color, CMD_COLOR_GREEN, Tr::Green, gSettings.color == 3);
    MenuItem(color, CMD_COLOR_NAVY, Tr::Navy, gSettings.color == 4);
    MenuItem(color, CMD_COLOR_ELECTRIC, Tr::Electric, gSettings.color == 5);
    SubMenu(visual, color, Tr::Color);
    AppendMenuW(opacity, MF_STRING | (gSettings.opacity == 50 ? MF_CHECKED : 0), CMD_OPACITY_50, L"50%");
    AppendMenuW(opacity, MF_STRING | (gSettings.opacity == 70 ? MF_CHECKED : 0), CMD_OPACITY_70, L"70%");
    AppendMenuW(opacity, MF_STRING | (gSettings.opacity == 90 ? MF_CHECKED : 0), CMD_OPACITY_90, L"90%");
    AppendMenuW(opacity, MF_STRING | (gSettings.opacity == 100 ? MF_CHECKED : 0), CMD_OPACITY_100, L"100%");
    SubMenu(visual, opacity, Tr::Opacity);
    MenuItem(visual, CMD_SETTINGS, Tr::Adjust);
    SubMenu(menu, visual, Tr::Visualizer);
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    MenuItem(menu, CMD_STARTUP, Tr::Startup, StartsWithWindows());
    AppendMenuW(language, MF_STRING | (gSettings.language == L"es" ? MF_CHECKED : 0), CMD_LANG_ES, L"Español");
    AppendMenuW(language, MF_STRING | (gSettings.language == L"en" ? MF_CHECKED : 0), CMD_LANG_EN, L"English");
    SubMenu(menu, language, Tr::Language);
    MenuItem(about, CMD_ABOUT_INFO, Tr::AboutInfo);
    AppendMenuW(about, MF_SEPARATOR, 0, nullptr);
    MenuItem(about, CMD_GITHUB, Tr::GitHubLink);
    MenuItem(about, CMD_DONATE, Tr::DonateLink);
    SubMenu(menu, about, Tr::About);
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    MenuItem(menu, CMD_QUIT, Tr::Exit);

    POINT point{}; GetCursorPos(&point);
    SetForegroundWindow(owner);
    const UINT action = TrackPopupMenu(menu, TPM_RIGHTBUTTON | TPM_RETURNCMD | TPM_NONOTIFY,
        point.x, point.y, 0, owner, nullptr);
    DestroyMenu(menu);
    PostMessageW(owner, WM_NULL, 0, 0);
    if (action) PostMessageW(gController, WM_COMMAND, action, 0);
}

void ExecuteMenu(UINT id) {
    switch (id) {
        case CMD_RECORD:
            if (gRecorder.Requested()) StopRecording();
            else if (gTranscriber.Busy()) {
                MessageBoxW(gController, Text(Tr::TranscriptionBusy), L"ChaqRec", MB_OK | MB_ICONINFORMATION);
            } else {
                gSessionRecordMode = gSettings.recordMode;
                gRecorder.Start(gController, gSettings.recordFormat == 0 ? RecordFormat::M4A : RecordFormat::WAV);
            }
            break;
        case CMD_FOLDER: {
            const auto dir = MicrophoneRecorder::RecordingsFolder();
            ShellExecuteW(nullptr, L"open", dir.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
            break;
        }
        case CMD_FORMAT_M4A: case CMD_FORMAT_WAV:
            if (!gRecorder.Requested()) gSettings.recordFormat = id == CMD_FORMAT_M4A ? 0 : 1;
            break;
        case CMD_MODE_RECORD: case CMD_MODE_TRANSCRIBE: case CMD_MODE_BOTH:
            if (!gRecorder.Requested()) {
                const int requestedMode = id == CMD_MODE_RECORD ? 0 :
                    (id == CMD_MODE_TRANSCRIBE ? 1 : 2);
                if (requestedMode == 0 || MessageBoxW(gController, Text(Tr::DownloadConsent),
                    L"ChaqRec · Whisper", MB_YESNO | MB_ICONINFORMATION) == IDYES)
                    gSettings.recordMode = requestedMode;
            }
            break;
        case CMD_OPEN_TXT: {
            const std::wstring path = gTranscriber.LastTextFile();
            if (!path.empty()) ShellExecuteW(nullptr, L"open", path.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
            break;
        }
        case CMD_SETTINGS:
            ShowSettingsDialog(gSettings, []() {
                if (gOverlay) InvalidateRect(gOverlay, nullptr, FALSE);
            });
            break;
        case CMD_SHOW: gSettings.visible = !gSettings.visible; ApplyVisibility(); break;
        case CMD_PAUSE: gSettings.paused = !gSettings.paused; SyncAudioAndTimer(); break;
        case CMD_LOCK: gSettings.locked = !gSettings.locked; UpdateClickThrough(); break;
        case CMD_TOP: gSettings.topmost = !gSettings.topmost; UpdateTopmost(); break;
        case CMD_SYSTEM: case CMD_MIC: case CMD_SOURCE_BOTH:
            gSettings.source = id == CMD_MIC ? 1 : (id == CMD_SOURCE_BOTH ? 2 : 0);
            if (gMonitorActive) gMonitor.Start(static_cast<AudioSource>(gSettings.source));
            break;
        case CMD_BARS: gSettings.visualStyle = 0; break;
        case CMD_WAVE: gSettings.visualStyle = 1; break;
        case CMD_SPECTRUM: gSettings.visualStyle = 2; break;
        case CMD_SMALL: ResizeOverlay(330, 95); break;
        case CMD_MEDIUM: ResizeOverlay(570, 165); break;
        case CMD_LARGE: ResizeOverlay(900, 235); break;
        case CMD_COLOR_CYAN: case CMD_COLOR_VIOLET: case CMD_COLOR_WHITE:
        case CMD_COLOR_GREEN: case CMD_COLOR_NAVY: case CMD_COLOR_ELECTRIC:
            gSettings.color = static_cast<int>(id - CMD_COLOR_CYAN); break;
        case CMD_OPACITY_50: gSettings.opacity = 50; break;
        case CMD_OPACITY_70: gSettings.opacity = 70; break;
        case CMD_OPACITY_90: gSettings.opacity = 90; break;
        case CMD_OPACITY_100: gSettings.opacity = 100; break;
        case CMD_STARTUP:
            if (!SetStartWithWindows(!StartsWithWindows()))
                MessageBoxW(nullptr, Text(Tr::ErrorStartup), L"ChaqRec", MB_OK | MB_ICONWARNING);
            break;
        case CMD_LANG_ES: gSettings.language = L"es"; break;
        case CMD_LANG_EN: gSettings.language = L"en"; break;
        case CMD_ABOUT_INFO:
            MessageBoxW(gController, Text(Tr::VersionInfo), L"ChaqRec", MB_OK | MB_ICONINFORMATION);
            break;
        case CMD_GITHUB:
            ShellExecuteW(nullptr, L"open", L"https://github.com/Filter27/ChaqRec", nullptr, nullptr, SW_SHOWNORMAL);
            break;
        case CMD_DONATE:
            ShellExecuteW(nullptr, L"open", L"https://www.paypal.com/paypalme/Filter27", nullptr, nullptr, SW_SHOWNORMAL);
            break;
        case CMD_QUIT: DestroyWindow(gController); return;
        default: return;
    }
    if (gOverlay) InvalidateRect(gOverlay, nullptr, FALSE);
    gSettings.Save();
}

void PaintOverlay(HWND hwnd) {
    PAINTSTRUCT ps{};
    BeginPaint(hwnd, &ps);
    EndPaint(hwnd, &ps);
    gRenderer.Draw(hwnd, gShown, gSettings, gRecordingActive);
}

LRESULT CALLBACK OverlayProc(HWND hwnd, UINT message, WPARAM w, LPARAM l) {
    switch (message) {
        case WM_SYSCOMMAND:
            if ((w & 0xfff0) == SC_MINIMIZE && gSettings.visible && gSettings.topmost) return 0;
            return DefWindowProcW(hwnd, message, w, l);
        case WM_NCHITTEST: {
            if (gSettings.locked) return HTTRANSPARENT;
            RECT r{}; GetWindowRect(hwnd, &r);
            const int x = GET_X_LPARAM(l), y = GET_Y_LPARAM(l);
            const bool left = x < r.left + 8, right = x >= r.right - 8;
            const bool top = y < r.top + 8, bottom = y >= r.bottom - 8;
            if (left && top) return HTTOPLEFT;
            if (right && top) return HTTOPRIGHT;
            if (left && bottom) return HTBOTTOMLEFT;
            if (right && bottom) return HTBOTTOMRIGHT;
            if (left) return HTLEFT;
            if (right) return HTRIGHT;
            if (top) return HTTOP;
            if (bottom) return HTBOTTOM;
            return HTCAPTION;
        }
        case WM_GETMINMAXINFO: {
            auto* limit = reinterpret_cast<MINMAXINFO*>(l);
            limit->ptMinTrackSize = {220, 75};
            limit->ptMaxTrackSize = {1400, 440};
            return 0;
        }
        case WM_PAINT: PaintOverlay(hwnd); return 0;
        case WM_SIZE: InvalidateRect(hwnd, nullptr, FALSE); return 0;
        case WM_ERASEBKGND: return 1;
        case WM_EXITSIZEMOVE: SaveGeometry(); return 0;
        case WM_CONTEXTMENU: ShowMenu(gController); return 0;
        case WM_NCRBUTTONUP: ShowMenu(gController); return 0;
        case WM_CLOSE:
            gSettings.visible = false;
            ApplyVisibility();
            return 0;
        default: return DefWindowProcW(hwnd, message, w, l);
    }
}

LRESULT CALLBACK ControllerProc(HWND hwnd, UINT message, WPARAM w, LPARAM l) {
    switch (message) {
        case WM_CHAQ_TRAY:
            // NOTIFYICON_VERSION_4 sends WM_CONTEXTMENU, NIN_SELECT and NIN_KEYSELECT.
            if (LOWORD(l) == WM_CONTEXTMENU || LOWORD(l) == WM_RBUTTONUP) {
                ShowMenu(hwnd); return 0;
            }
            if (LOWORD(l) == NIN_SELECT || LOWORD(l) == NIN_KEYSELECT || LOWORD(l) == WM_LBUTTONUP) {
                gSettings.visible = !gSettings.visible;
                ApplyVisibility(); return 0;
            }
            return 0;
        case WM_COMMAND: ExecuteMenu(LOWORD(w)); return 0;
        case WM_TIMER:
            if (w == kKeepVisibleTimer) { KeepOverlayOnDesktop(); return 0; }
            if (w == kFrameTimer && gOverlay && gSettings.visible && !gSettings.paused) {
                const AudioSnapshot current = gMonitor.Snapshot();
                const float gain = 0.25f + 0.14f * gSettings.sensitivityLevel;
                const float gate = gSettings.noiseGate / 100.0f;
                const float t = gSettings.smoothing / 100.0f;
                const float attack = 0.65f - 0.44f * t;
                const float release = 0.32f - 0.26f * t;
                for (size_t i = 0; i < 64; ++i) {
                    const size_t left = i ? i - 1 : i;
                    const size_t right = i < 63 ? i + 1 : i;
                    const float spatial = .25f * current.bands[left] +
                        .50f * current.bands[i] + .25f * current.bands[right];
                    float target = std::clamp(spatial * gain, 0.0f, 1.0f);
                    if (target < gate) target = 0;
                    const float speed = target > gShown.bands[i] ? attack : release;
                    gShown.bands[i] += (target - gShown.bands[i]) * speed;
                }
                const float waveAlpha = .58f - .42f * t;
                for (size_t i = 0; i < gShown.waveform.size(); ++i) {
                    const float target = current.waveform[i] * gain;
                    gShown.waveform[i] += (target - gShown.waveform[i]) * waveAlpha;
                }
                InvalidateRect(gOverlay, nullptr, FALSE);
            }
            return 0;
        case WM_CHAQ_RECORD_STATUS:
            if (w == CHAQ_RECORD_STARTED) {
                gRecordingActive = gRecorder.Requested();
                UpdateTrayIcon();
                if (gOverlay) InvalidateRect(gOverlay, nullptr, FALSE);
            } else if (w == CHAQ_RECORD_FAILED) {
                StopRecording(false);
                MessageBoxW(nullptr, Text(Tr::ErrorRecord), L"ChaqRec", MB_OK | MB_ICONWARNING);
            }
            return 0;
        case WM_CHAQ_TRANSCRIBE_STATUS:
            switch (static_cast<TranscriptionStatus>(w)) {
                case TranscriptionStatus::Preparing:
                    ShowBalloon(Text(Tr::TranscriptionStarting)); break;
                case TranscriptionStatus::Downloading:
                    ShowBalloon(Text(Tr::TranscriptionDownloading)); break;
                case TranscriptionStatus::Processing:
                    ShowBalloon(Text(Tr::TranscriptionProcessing)); break;
                case TranscriptionStatus::Completed:
                    ShowBalloon(Text(Tr::TranscriptionCompleted)); break;
                case TranscriptionStatus::Failed: {
                    const auto reason = gTranscriber.LastError();
                    const std::wstring info = (gSettings.language == L"es"
                        ? L"No se pudo transcribir. El audio original se conservó.\n\n"
                        : L"Transcription failed. The original audio was kept.\n\n") + reason;
                    MessageBoxW(hwnd, info.c_str(), L"ChaqRec · Whisper", MB_OK | MB_ICONWARNING);
                    break;
                }
                default: break;
            }
            return 0;
        case WM_DESTROY: {
            KillTimer(hwnd, kKeepVisibleTimer);
            gTranscriber.Stop();
            KillTimer(hwnd, kFrameTimer);
            gMonitor.Stop();
            gRecorder.Stop();
            NOTIFYICONDATAW nid{};
            nid.cbSize = sizeof(nid); nid.hWnd = hwnd; nid.uID = kTrayId;
            Shell_NotifyIconW(NIM_DELETE, &nid);
            if (gOverlay) SaveGeometry();
            gSettings.Save();
            PostQuitMessage(0);
            return 0;
        }
        default: return DefWindowProcW(hwnd, message, w, l);
    }
}
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
    HANDLE mutex = CreateMutexW(nullptr, FALSE, L"Local\\ChaqRec_OnlyOneInstance");
    if (mutex && GetLastError() == ERROR_ALREADY_EXISTS) {
        HWND existing = FindWindowW(L"ChaqRecVisualizerWindow", nullptr);
        // Do not undo an explicit Hide setting in the existing instance.
        (void)existing;
        CloseHandle(mutex);
        return 0;
    }
    SetProcessDPIAware();
    const HRESULT com = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(com)) { if (mutex) CloseHandle(mutex); return 1; }
    gInstance = instance;
    gSettings.Load();
    if (!gRenderer.Start()) {
        MessageBoxW(nullptr, L"No se pudo iniciar el renderizador de ChaqRec.", L"ChaqRec", MB_ICONERROR);
        CoUninitialize();
        if (mutex) CloseHandle(mutex);
        return 1;
    }
    gIcon = reinterpret_cast<HICON>(LoadImageW(instance, MAKEINTRESOURCEW(IDI_APP), IMAGE_ICON, 16, 16, 0));
    gRecordingIcon = reinterpret_cast<HICON>(LoadImageW(instance, MAKEINTRESOURCEW(IDI_RECORDING), IMAGE_ICON, 16, 16, 0));
    if (!gIcon) gIcon = LoadIconW(nullptr, IDI_APPLICATION);
    if (!gRecordingIcon) gRecordingIcon = LoadIconW(nullptr, IDI_ERROR);

    WNDCLASSEXW controllerClass{};
    controllerClass.cbSize = sizeof(controllerClass);
    controllerClass.hInstance = instance;
    controllerClass.lpfnWndProc = ControllerProc;
    controllerClass.lpszClassName = kControllerClass;
    RegisterClassExW(&controllerClass);
    gController = CreateWindowExW(WS_EX_TOOLWINDOW, kControllerClass, L"ChaqRec", WS_OVERLAPPED,
                                  0, 0, 0, 0, nullptr, nullptr, instance, nullptr);

    WNDCLASSEXW overlayClass{};
    overlayClass.cbSize = sizeof(overlayClass);
    overlayClass.hInstance = instance;
    overlayClass.lpfnWndProc = OverlayProc;
    overlayClass.lpszClassName = kOverlayClass;
    overlayClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    RegisterClassExW(&overlayClass);
    RECT area{};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &area, 0);
    int x = gSettings.x;
    int y = gSettings.y;
    RECT savedPosition{x, y, x + gSettings.width, y + gSettings.height};
    if (x == -1 || y == -1 ||
        MonitorFromRect(&savedPosition, MONITOR_DEFAULTTONULL) == nullptr) {
        x = area.left + (area.right - area.left - gSettings.width) / 2;
        y = area.bottom - gSettings.height - 65;
    }
    DWORD exStyle = WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE;
    if (gSettings.topmost) exStyle |= WS_EX_TOPMOST;
    gOverlay = CreateWindowExW(exStyle, kOverlayClass, L"ChaqRec - Visualizer",
                               WS_POPUP, x, y, gSettings.width, gSettings.height,
                               nullptr, nullptr, instance, nullptr);
    if (!gOverlay || !gController) {
        MessageBoxW(nullptr, L"No se pudo iniciar ChaqRec.", L"ChaqRec", MB_ICONERROR);
        if (gController) DestroyWindow(gController);
        gRenderer.Stop();
        CoUninitialize();
        if (mutex) CloseHandle(mutex);
        return 1;
    }
    UpdateClickThrough();
    UpdateTrayIcon(true);
    SetTimer(gController, kKeepVisibleTimer, 900, nullptr);
    if (gSettings.visible) ShowWindow(gOverlay, SW_SHOWNOACTIVATE);
    SyncAudioAndTimer();

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    if (IsWindow(gOverlay)) DestroyWindow(gOverlay);
    if (gIcon) DestroyIcon(gIcon);
    if (gRecordingIcon) DestroyIcon(gRecordingIcon);
    gRenderer.Stop();
    CoUninitialize();
    if (mutex) CloseHandle(mutex);
    return static_cast<int>(msg.wParam);
}
