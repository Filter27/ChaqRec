#pragma once
#define NOMINMAX
#include <windows.h>
#include <string>

struct Settings {
    int x = -1;
    int y = -1;
    int width = 570;
    int height = 165;
    bool visible = true;
    bool locked = false;
    bool topmost = true;
    bool paused = false;
    int visualStyle = 2; // 0 = round bars, 1 = smooth wave, 2 = flowing spectrum
    int source = 2;      // 0 = system output, 1 = microphone, 2 = both (visualizer only)
    int color = 4;       // 0 cyan, 1 violet, 2 white, 3 green, 4 dark blue, 5 electric blue
    int sensitivityLevel = 4; // 1...10 (visualization only)
    int smoothing = 70;       // 0...100
    int noiseGate = 8;        // 0...30 visual amplitude percentage
    int opacity = 90;         // 30...100 global window opacity
    int recordFormat = 0;     // 0 = M4A/AAC, 1 = PCM/WAV
    int recordMode = 0;       // 0 = audio only, 1 = transcript only, 2 = both
    std::wstring language = L"es";

    static std::wstring ConfigFile();
    void Load();
    void Save() const;
};

bool StartsWithWindows();
bool SetStartWithWindows(bool enabled);
