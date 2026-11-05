#pragma once
#define NOMINMAX
#include <windows.h>
#include "Audio.h"
#include "Settings.h"

// Small 32-bit ARGB back buffer. The window uses UpdateLayeredWindow (per-pixel alpha),
// not a magic transparent color. Windows/GDI+ are the only graphics dependencies.
class Renderer {
public:
    Renderer() = default;
    ~Renderer();
    bool Start();
    void Stop();
    void Draw(HWND window, const AudioSnapshot& snapshot,
              const Settings& settings, bool recording);
private:
    bool Resize(int width, int height);
    HDC dc_ = nullptr;
    HBITMAP dib_ = nullptr;
    HGDIOBJ oldBitmap_ = nullptr;
    void* pixels_ = nullptr;
    int width_ = 0;
    int height_ = 0;
    unsigned long long gdiplusToken_ = 0;
};
