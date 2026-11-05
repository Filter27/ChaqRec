#include "Settings.h"
#include <shlobj.h>
#include <algorithm>
#include <iterator>

namespace {
std::wstring ApplicationPath() {
    wchar_t exe[32768]{};
    GetModuleFileNameW(nullptr, exe, static_cast<DWORD>(std::size(exe)));
    return exe;
}

std::wstring IniReadString(const std::wstring& file, const wchar_t* key, const wchar_t* fallback) {
    wchar_t buffer[64]{};
    GetPrivateProfileStringW(L"ChaqRec", key, fallback, buffer,
                             static_cast<DWORD>(std::size(buffer)), file.c_str());
    return buffer;
}

void IniWrite(const std::wstring& file, const wchar_t* key, const std::wstring& value) {
    WritePrivateProfileStringW(L"ChaqRec", key, value.c_str(), file.c_str());
}
}

std::wstring Settings::ConfigFile() {
    PWSTR appdata = nullptr;
    std::wstring path;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, KF_FLAG_DEFAULT, nullptr, &appdata))) {
        path = appdata;
        CoTaskMemFree(appdata);
    } else {
        wchar_t tmp[MAX_PATH]{};
        GetEnvironmentVariableW(L"APPDATA", tmp, MAX_PATH);
        path = tmp;
    }
    path += L"\\ChaqRec";
    CreateDirectoryW(path.c_str(), nullptr);
    return path + L"\\settings.ini";
}

void Settings::Load() {
    const std::wstring file = ConfigFile();
    auto get = [&](const wchar_t* key, int fallback) {
        return static_cast<int>(GetPrivateProfileIntW(L"ChaqRec", key, fallback, file.c_str()));
    };
    x = get(L"x", -1); y = get(L"y", -1);
    width = std::clamp(get(L"width", 570), 220, 1400);
    height = std::clamp(get(L"height", 165), 75, 440);
    visible = get(L"visible", 1) != 0;
    locked = get(L"locked", 0) != 0;
    topmost = get(L"topmost", 1) != 0;
    paused = get(L"paused", 0) != 0;
    // The upgraded visualizer defaults to flowing spectrum even with old v0.1 preferences.
    visualStyle = std::clamp(get(L"visualStyle", 2), 0, 2);
    source = std::clamp(get(L"source", 2), 0, 2);
    color = std::clamp(get(L"color", 4), 0, 5);
    const int oldSensitivity = get(L"sensitivity", 100);
    sensitivityLevel = std::clamp(get(L"sensitivityLevel", std::clamp((oldSensitivity - 40) / 14, 1, 10)), 1, 10);
    smoothing = std::clamp(get(L"smoothing", 70), 0, 100);
    noiseGate = std::clamp(get(L"noiseGate", 8), 0, 30);
    opacity = std::clamp(get(L"opacity", 90), 30, 100);
    recordFormat = std::clamp(get(L"recordFormat", 0), 0, 1);
    recordMode = std::clamp(get(L"recordMode", 0), 0, 2);
    language = IniReadString(file, L"language", L"");
    if (language != L"es" && language != L"en") {
        const LANGID lid = GetUserDefaultUILanguage();
        language = PRIMARYLANGID(lid) == LANG_SPANISH ? L"es" : L"en";
    }
}

void Settings::Save() const {
    const std::wstring file = ConfigFile();
    auto number = [&](const wchar_t* key, int n) { IniWrite(file, key, std::to_wstring(n)); };
    number(L"x", x); number(L"y", y);
    number(L"width", width); number(L"height", height);
    number(L"visible", visible); number(L"locked", locked);
    number(L"topmost", topmost); number(L"paused", paused);
    number(L"visualStyle", visualStyle); number(L"source", source);
    number(L"color", color); number(L"sensitivityLevel", sensitivityLevel);
    number(L"smoothing", smoothing); number(L"noiseGate", noiseGate);
    number(L"opacity", opacity); number(L"recordFormat", recordFormat);
    number(L"recordMode", recordMode);
    IniWrite(file, L"language", language);
}

bool StartsWithWindows() {
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                      0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS) return false;
    wchar_t buffer[32768]{};
    DWORD type = 0, bytes = sizeof(buffer);
    const LONG result = RegQueryValueExW(key, L"ChaqRec", nullptr, &type,
                                         reinterpret_cast<LPBYTE>(buffer), &bytes);
    RegCloseKey(key);
    return result == ERROR_SUCCESS && type == REG_SZ &&
           std::wstring(buffer) == L"\"" + ApplicationPath() + L"\"";
}

bool SetStartWithWindows(bool enabled) {
    HKEY key = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                        0, nullptr, 0, KEY_SET_VALUE, nullptr, &key, nullptr) != ERROR_SUCCESS) return false;
    LONG result;
    if (enabled) {
        const std::wstring command = L"\"" + ApplicationPath() + L"\"";
        result = RegSetValueExW(key, L"ChaqRec", 0, REG_SZ,
            reinterpret_cast<const BYTE*>(command.c_str()),
            static_cast<DWORD>((command.size() + 1) * sizeof(wchar_t)));
    } else result = RegDeleteValueW(key, L"ChaqRec");
    RegCloseKey(key);
    return result == ERROR_SUCCESS || (!enabled && result == ERROR_FILE_NOT_FOUND);
}
