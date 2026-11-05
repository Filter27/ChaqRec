#pragma once
#define NOMINMAX
#include <windows.h>
#include <array>
#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

// No external audio libraries: WASAPI shared-mode capture and Windows COM.
enum class AudioSource { System = 0, Microphone = 1, Both = 2 };
enum class RecordFormat { M4A = 0, WAV = 1 };

constexpr UINT WM_CHAQ_RECORD_STATUS = WM_APP + 23;
constexpr WPARAM CHAQ_RECORD_STARTED = 1;
constexpr WPARAM CHAQ_RECORD_FAILED = 2;

struct AudioSnapshot {
    std::array<float, 64> bands{};
    std::array<float, 256> waveform{};
};

// CaptureStream is used only from a COM-initialized worker thread.
class CaptureStream {
public:
    CaptureStream() = default;
    ~CaptureStream();
    CaptureStream(const CaptureStream&) = delete;
    CaptureStream& operator=(const CaptureStream&) = delete;

    bool Open(AudioSource source);
    bool Poll(const std::function<void(const float*, size_t)>& callback);
    unsigned SampleRate() const { return sampleRate_; }
    void Close();

private:
    struct Internal;
    Internal* p_ = nullptr;
    unsigned sampleRate_ = 48000;
    std::vector<float> samples_;
};

class AudioMonitor {
public:
    ~AudioMonitor() { Stop(); }
    void Start(AudioSource source);
    void Stop();
    AudioSnapshot Snapshot();

private:
    void Run(AudioSource source);
    std::thread worker_;
    std::atomic<bool> stop_{false};
    std::mutex mutex_;
    AudioSnapshot snapshot_{};
    ULONGLONG lastUpdate_ = 0;
};

class MicrophoneRecorder {
public:
    ~MicrophoneRecorder() { Stop(); }
    void Start(HWND notify, RecordFormat format);
    void Stop();
    bool Requested() const { return requested_.load(); }
    std::wstring LastFile() const;
    static std::wstring RecordingsFolder();

private:
    void Run(HWND notify, RecordFormat format);
    std::atomic<bool> requested_{false};
    std::thread worker_;
    mutable std::mutex fileMutex_;
    std::wstring lastFile_;
};
