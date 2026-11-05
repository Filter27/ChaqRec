#pragma once
#define NOMINMAX
#include <windows.h>
#include <atomic>
#include <mutex>
#include <string>
#include <thread>

// Download and transcription run only after recording stops, on a separate worker.
// No model, Whisper runtime or network requests are loaded during normal recording.
constexpr UINT WM_CHAQ_TRANSCRIBE_STATUS = WM_APP + 24;
enum class TranscriptionStatus : WPARAM {
    Preparing = 1, Downloading = 2, Processing = 3, Completed = 4, Failed = 5, Cancelled = 6
};

class Transcriber {
public:
    ~Transcriber() { Stop(); }
    bool Start(HWND notify, std::wstring audioFile, bool transcriptOnly);
    void Stop();
    bool Busy() const { return busy_.load(); }
    std::wstring LastTextFile() const;
    std::wstring LastError() const;
private:
    void Run(HWND notify, std::wstring audioFile, bool transcriptOnly);
    void UpdateResult(std::wstring textFile, std::wstring error);
    std::atomic<bool> cancel_{false};
    std::atomic<bool> busy_{false};
    std::thread worker_;
    mutable std::mutex mutex_;
    std::wstring lastText_;
    std::wstring lastError_;
};
