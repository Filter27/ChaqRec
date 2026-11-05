#pragma once
#define NOMINMAX
#include <windows.h>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

struct IMFSinkWriter;

// AAC-LC mono 96 kbit/s, 48 kHz using Windows Media Foundation (no third-party codec).
// Call solely from the recorder's COM-initialized worker thread.
class M4aRecorder {
public:
    ~M4aRecorder();
    bool Open(const std::wstring& filename, unsigned inputRate);
    bool Write(const float* mono, size_t count);
    bool Close(); // Finalize the M4A container before reporting success.
private:
    bool WritePcm(const std::vector<int16_t>& pcm);
    IMFSinkWriter* writer_ = nullptr;
    bool mfStarted_ = false;
    bool begun_ = false;
    DWORD streamIndex_ = 0;
    unsigned inputRate_ = 48000;
    uint64_t incoming_ = 0;
    uint64_t emitted_ = 0;
    double nextPosition_ = 0;
    float previous_ = 0;
    std::vector<int16_t> buffer_;
};
