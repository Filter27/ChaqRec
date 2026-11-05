#include "Renderer.h"
#include <gdiplus.h>
#include <algorithm>
#include <array>
#include <cmath>

using namespace Gdiplus;

namespace {
Color Tint(int palette, BYTE a = 255) {
    switch (palette) {
        case 0: return Color(a, 61, 197, 252);   // cyan
        case 1: return Color(a, 191, 137, 253);  // lavender
        case 2: return Color(a, 245, 247, 250);  // white
        case 3: return Color(a, 93, 225, 158);   // mint
        case 5: return Color(a, 85, 158, 255);   // electric blue
        default: return Color(a, 26, 62, 156); // #1A3E9C dark blue
    }
}

void RoundBar(Graphics& graphics, float x, float y, float w, float h,
              const Color& top, const Color& bottom) {
    const float radius = std::min(w / 2.0f, h / 2.0f);
    GraphicsPath path;
    path.AddArc(x, y, radius * 2, radius * 2, 180, 90);
    path.AddArc(x + w - radius * 2, y, radius * 2, radius * 2, 270, 90);
    path.AddLine(x + w, y + radius, x + w, y + h);
    path.AddLine(x + w, y + h, x, y + h);
    path.CloseFigure();
    LinearGradientBrush fill(PointF(x, y), PointF(x, y + std::max(2.0f, h)),
                             top, bottom);
    graphics.FillPath(&fill, &path);
}

void DrawBars(Graphics& g, int width, int height, const AudioSnapshot& snap,
              int color) {
    const float baseline = height - 13.0f;
    const float maxH = height - 28.0f;
    constexpr int count = 48;
    const float total = std::max(1.0f, width - 24.0f);
    const float step = total / count;
    const float w = std::max(2.0f, step * 0.60f);
    for (int i = 0; i < count; ++i) {
        const int band = std::min(63, i * 64 / count);
        const float value = std::clamp(snap.bands[band], 0.0f, 1.0f);
        if (value < .014f) continue;
        const float h = std::max(3.0f, value * maxH);
        const float x = 12.0f + i * step + (step - w) / 2;
        const float y = baseline - h;
        const Color accent = Tint(color);
        RoundBar(g, x - 1, y - 2, w + 2, h + 4,
                 Color(24, accent.GetR(), accent.GetG(), accent.GetB()),
                 Color(4, accent.GetR(), accent.GetG(), accent.GetB()));
        RoundBar(g, x, y, w, h,
                 color == 4 ? Color(236, 85, 158, 255) : Tint(color, 242),
                 Tint(color, 90));
    }
}

void GlowStroke(Graphics& g, const GraphicsPath& path, int color,
                float width = 2.0f) {
    const Color c = color == 4 ? Color(255, 85, 158, 255) : Tint(color);
    Pen outer(Color(20, c.GetR(), c.GetG(), c.GetB()), width + 11.0f);
    Pen middle(Color(52, c.GetR(), c.GetG(), c.GetB()), width + 5.0f);
    Pen line(Color(238, c.GetR(), c.GetG(), c.GetB()), width);
    for (Pen* pen : {&outer, &middle, &line}) {
        pen->SetLineJoin(LineJoinRound);
        pen->SetStartCap(LineCapRound);
        pen->SetEndCap(LineCapRound);
        g.DrawPath(pen, &path);
    }
}

void DrawSpectrum(Graphics& g, int width, int height,
                  const AudioSnapshot& snap, int color) {
    constexpr int n = 64;
    std::array<PointF, n> pts{};
    const float baseline = height - 19.0f;
    for (int i = 0; i < n; ++i) {
        const float level = std::clamp(snap.bands[i], 0.0f, 1.0f);
        pts[i] = PointF(13.0f + (width - 26.0f) * i / (n - 1.0f),
                        baseline - level * (height - 37.0f));
    }
    GraphicsPath area;
    area.AddCurve(pts.data(), n, .38f);
    area.AddLine(pts.back(), PointF(pts.back().X, baseline + 1));
    area.AddLine(PointF(pts.back().X, baseline + 1), PointF(pts[0].X, baseline + 1));
    area.CloseFigure();
    const Color accent = Tint(color);
    LinearGradientBrush gradient(PointF(0, 0), PointF(0, static_cast<REAL>(height)),
        Color(76, accent.GetR(), accent.GetG(), accent.GetB()),
        Color(0, accent.GetR(), accent.GetG(), accent.GetB()));
    g.FillPath(&gradient, &area);
    GraphicsPath line;
    line.AddCurve(pts.data(), n, .38f);
    GlowStroke(g, line, color, 2.4f);
}

void DrawWave(Graphics& g, int width, int height,
              const AudioSnapshot& snap, int color) {
    constexpr int n = 128;
    std::array<PointF, n> pts{};
    for (int i = 0; i < n; ++i) {
        const size_t s = static_cast<size_t>(i * 2);
        // A small spatial filter eliminates sample-to-sample jaggedness.
        const float a = snap.waveform[s > 0 ? s - 1 : s];
        const float b = snap.waveform[s];
        const float c = snap.waveform[std::min<size_t>(255, s + 1)];
        const float v = .25f * a + .5f * b + .25f * c;
        pts[i] = PointF(12.0f + (width - 24.0f) * i / (n - 1.0f),
                        height * .52f - std::clamp(v, -1.0f, 1.0f) * height * .36f);
    }
    GraphicsPath path;
    path.AddCurve(pts.data(), n, .40f);
    GlowStroke(g, path, color, 2.0f);
}
}

Renderer::~Renderer() { Stop(); }

bool Renderer::Start() {
    if (gdiplusToken_) return true;
    GdiplusStartupInput input;
    ULONG_PTR token = 0;
    if (GdiplusStartup(&token, &input, nullptr) != Ok) return false;
    gdiplusToken_ = static_cast<unsigned long long>(token);
    return true;
}

void Renderer::Stop() {
    if (dib_) { SelectObject(dc_, oldBitmap_); DeleteObject(dib_); dib_ = nullptr; }
    if (dc_) { DeleteDC(dc_); dc_ = nullptr; }
    pixels_ = nullptr; width_ = height_ = 0;
    if (gdiplusToken_) {
        GdiplusShutdown(static_cast<ULONG_PTR>(gdiplusToken_));
        gdiplusToken_ = 0;
    }
}

bool Renderer::Resize(int width, int height) {
    if (dc_ && width == width_ && height == height_) return true;
    if (dib_) { SelectObject(dc_, oldBitmap_); DeleteObject(dib_); dib_ = nullptr; }
    if (!dc_) dc_ = CreateCompatibleDC(nullptr);
    if (!dc_) return false;
    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = -height; // top-down 32-bit ARGB
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    dib_ = CreateDIBSection(dc_, &info, DIB_RGB_COLORS, &pixels_, nullptr, 0);
    if (!dib_ || !pixels_) return false;
    oldBitmap_ = SelectObject(dc_, dib_);
    width_ = width; height_ = height;
    return true;
}

void Renderer::Draw(HWND window, const AudioSnapshot& snap,
                    const Settings& settings, bool recording) {
    if (!window || !gdiplusToken_) return;
    RECT rect{};
    if (!GetWindowRect(window, &rect)) return;
    const int width = rect.right - rect.left;
    const int height = rect.bottom - rect.top;
    if (width <= 0 || height <= 0 || !Resize(width, height)) return;

    Bitmap bitmap(width, height, width * 4, PixelFormat32bppPARGB,
                  static_cast<BYTE*>(pixels_));
    {
        Graphics g(&bitmap);
        g.SetSmoothingMode(SmoothingModeAntiAlias);
        g.SetCompositingQuality(CompositingQualityHighSpeed);
        g.SetPixelOffsetMode(PixelOffsetModeHighQuality);
        // Near-transparent pixels keep the overlay draggable, even at reduced opacity.
        g.Clear(Color(4, 0, 0, 0));
        if (settings.visualStyle == 0) DrawBars(g, width, height, snap, settings.color);
        else if (settings.visualStyle == 1) DrawWave(g, width, height, snap, settings.color);
        else DrawSpectrum(g, width, height, snap, settings.color);

        if (recording) {
            SolidBrush halo(Color(43, 255, 60, 79));
            SolidBrush dot(Color(238, 255, 67, 87));
            g.FillEllipse(&halo, 7, 7, 16, 16);
            g.FillEllipse(&dot, 11, 11, 8, 8); // no REC text
        }
    }
    POINT src{0, 0};
    POINT dst{rect.left, rect.top};
    SIZE size{width, height};
    BLENDFUNCTION blend{};
    blend.BlendOp = AC_SRC_OVER;
    blend.SourceConstantAlpha = static_cast<BYTE>(std::clamp(settings.opacity, 30, 100) * 255 / 100);
    blend.AlphaFormat = AC_SRC_ALPHA;
    UpdateLayeredWindow(window, nullptr, &dst, &size, dc_, &src, 0, &blend, ULW_ALPHA);
}
