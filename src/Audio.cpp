#include "Audio.h"
#include "M4aRecorder.h"

#include <audioclient.h>
#include <mmdeviceapi.h>
#include <mmreg.h>
#include <ksmedia.h>
#include <shlobj.h>
#include <algorithm>
#include <cmath>
#include <complex>
#include <cstring>
#include <memory>
#include <utility>

namespace {
template<class T> class ComPtr {
public:
    ~ComPtr() { reset(); }
    T* operator->() const { return value_; }
    T* get() const { return value_; }
    T** put() { reset(); return &value_; }
    void reset() { if (value_) { value_->Release(); value_ = nullptr; } }
private:
    T* value_ = nullptr;
};

constexpr float kPi = 3.14159265358979323846f;
constexpr int kFFT = 2048;

void FFT(std::array<std::complex<float>, kFFT>& a) {
    for (int i = 1, j = 0; i < kFFT; ++i) {
        int bit = kFFT >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap(a[i], a[j]);
    }
    for (int len = 2; len <= kFFT; len <<= 1) {
        const float phase = -2.0f * kPi / static_cast<float>(len);
        const std::complex<float> step(std::cos(phase), std::sin(phase));
        for (int i = 0; i < kFFT; i += len) {
            std::complex<float> rotation(1.0f, 0.0f);
            for (int j = 0; j < len / 2; ++j) {
                const auto u = a[i + j];
                const auto v = a[i + j + len / 2] * rotation;
                a[i + j] = u + v;
                a[i + j + len / 2] = u - v;
                rotation *= step;
            }
        }
    }
}

AudioSnapshot Analyze(const std::array<float, kFFT>& ring, size_t next, unsigned sampleRate) {
    AudioSnapshot result;
    std::array<std::complex<float>, kFFT> spectrum{};
    for (int i = 0; i < kFFT; ++i) {
        const float sample = ring[(next + static_cast<size_t>(i)) % kFFT];
        const float window = 0.5f - 0.5f * std::cos(2.0f * kPi * i / (kFFT - 1));
        spectrum[i] = {sample * window, 0.0f};
    }
    for (size_t i = 0; i < result.waveform.size(); ++i) {
        result.waveform[i] = ring[(next + i * 8) % kFFT];
    }
    FFT(spectrum);

    const float maxFrequency = std::min(12000.0f, sampleRate * 0.46f);
    constexpr float minFrequency = 55.0f;
    for (int bar = 0; bar < 64; ++bar) {
        const float low = minFrequency * std::pow(maxFrequency / minFrequency, bar / 64.0f);
        const float high = minFrequency * std::pow(maxFrequency / minFrequency, (bar + 1) / 64.0f);
        const int start = std::clamp(static_cast<int>(low * kFFT / sampleRate), 1, kFFT / 2 - 1);
        const int finish = std::clamp(static_cast<int>(high * kFFT / sampleRate), start + 1, kFFT / 2);
        float peak = 0.0f;
        for (int i = start; i < finish; ++i) {
            peak = std::max(peak, std::abs(spectrum[i]) * 4.0f / kFFT);
        }
        // -62 dBFS is the visual silence floor. Log scaling makes quiet music visible.
        const float db = 20.0f * std::log10(std::max(peak, 0.000001f));
        result.bands[bar] = std::pow(std::clamp((db + 62.0f) / 60.0f, 0.0f, 1.0f), 1.45f);
    }
    return result;
}

#pragma pack(push, 1)
struct WaveHeader {
    char riff[4] = {'R','I','F','F'};
    uint32_t riffSize = 36;
    char wave[4] = {'W','A','V','E'};
    char fmt[4] = {'f','m','t',' '};
    uint32_t fmtSize = 16;
    uint16_t formatTag = 1;
    uint16_t channels = 1;
    uint32_t sampleRate = 48000;
    uint32_t byteRate = 96000;
    uint16_t blockAlign = 2;
    uint16_t bitsPerSample = 16;
    char data[4] = {'d','a','t','a'};
    uint32_t dataSize = 0;
};
#pragma pack(pop)
static_assert(sizeof(WaveHeader) == 44);

bool WriteExact(HANDLE file, const void* data, DWORD bytes) {
    DWORD written = 0;
    return WriteFile(file, data, bytes, &written, nullptr) && written == bytes;
}

std::wstring TimeStamp() {
    SYSTEMTIME now;
    GetLocalTime(&now);
    wchar_t buf[80];
    swprintf_s(buf, L"%04u-%02u-%02u_%02u-%02u-%02u",
        now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond);
    return buf;
}
}

struct CaptureStream::Internal {
    ComPtr<IMMDeviceEnumerator> enumerator;
    ComPtr<IMMDevice> endpoint;
    ComPtr<IAudioClient> audio;
    ComPtr<IAudioCaptureClient> capture;
    WAVEFORMATEX* fmt = nullptr;
    bool floatingPoint = false;
    int bits = 0;
    int channels = 0;
    bool running = false;
    ~Internal() {
        if (audio.get() && running) audio->Stop();
        if (fmt) CoTaskMemFree(fmt);
    }
};

CaptureStream::~CaptureStream() { Close(); }
void CaptureStream::Close() { delete p_; p_ = nullptr; samples_.clear(); }

bool CaptureStream::Open(AudioSource source) {
    Close();
    std::unique_ptr<Internal> in(new Internal());
    HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                  IID_PPV_ARGS(in->enumerator.put()));
    if (FAILED(hr)) return false;
    hr = in->enumerator->GetDefaultAudioEndpoint(
        source == AudioSource::System ? eRender : eCapture,
        source == AudioSource::System ? eMultimedia : eConsole, in->endpoint.put());
    if (FAILED(hr)) return false;
    hr = in->endpoint->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
                                reinterpret_cast<void**>(in->audio.put()));
    if (FAILED(hr)) return false;
    hr = in->audio->GetMixFormat(&in->fmt);
    if (FAILED(hr) || !in->fmt || in->fmt->nChannels == 0 || in->fmt->nSamplesPerSec == 0) return false;

    const auto* fmt = in->fmt;
    in->bits = fmt->wBitsPerSample;
    in->channels = fmt->nChannels;
    if (fmt->wFormatTag == WAVE_FORMAT_IEEE_FLOAT) {
        in->floatingPoint = true;
    } else if (fmt->wFormatTag == WAVE_FORMAT_PCM) {
        in->floatingPoint = false;
    } else if (fmt->wFormatTag == WAVE_FORMAT_EXTENSIBLE &&
               fmt->cbSize >= (sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX))) {
        const auto* ext = reinterpret_cast<const WAVEFORMATEXTENSIBLE*>(fmt);
        if (IsEqualGUID(ext->SubFormat, KSDATAFORMAT_SUBTYPE_IEEE_FLOAT)) in->floatingPoint = true;
        else if (IsEqualGUID(ext->SubFormat, KSDATAFORMAT_SUBTYPE_PCM)) in->floatingPoint = false;
        else return false;
    } else return false;
    if (!(in->floatingPoint && in->bits == 32) &&
        !(!in->floatingPoint && (in->bits == 16 || in->bits == 24 || in->bits == 32))) return false;

    const DWORD flags = source == AudioSource::System ? AUDCLNT_STREAMFLAGS_LOOPBACK : 0;
    hr = in->audio->Initialize(AUDCLNT_SHAREMODE_SHARED, flags, 0, 0, in->fmt, nullptr);
    if (FAILED(hr)) return false;
    hr = in->audio->GetService(__uuidof(IAudioCaptureClient),
                                reinterpret_cast<void**>(in->capture.put()));
    if (FAILED(hr)) return false;
    hr = in->audio->Start();
    if (FAILED(hr)) return false;
    in->running = true;
    sampleRate_ = fmt->nSamplesPerSec;
    p_ = in.release();
    return true;
}

bool CaptureStream::Poll(const std::function<void(const float*, size_t)>& callback) {
    if (!p_) return false;
    UINT32 packets = 0;
    HRESULT hr = p_->capture->GetNextPacketSize(&packets);
    if (FAILED(hr)) return false;
    while (packets > 0) {
        BYTE* raw = nullptr;
        UINT32 frames = 0;
        DWORD flags = 0;
        hr = p_->capture->GetBuffer(&raw, &frames, &flags, nullptr, nullptr);
        if (FAILED(hr)) return false;
        samples_.resize(frames);
        const int bytesPerSample = p_->bits / 8;
        const bool silent = (flags & AUDCLNT_BUFFERFLAGS_SILENT) != 0 || !raw;
        for (UINT32 i = 0; i < frames; ++i) {
            if (silent) { samples_[i] = 0; continue; }
            float sum = 0;
            for (int c = 0; c < p_->channels; ++c) {
                const BYTE* s = raw + static_cast<size_t>(i) * p_->fmt->nBlockAlign + c * bytesPerSample;
                float value = 0;
                if (p_->floatingPoint) {
                    std::memcpy(&value, s, sizeof(float));
                    if (!std::isfinite(value)) value = 0;
                } else if (p_->bits == 16) {
                    int16_t v;
                    std::memcpy(&v, s, sizeof(v));
                    value = v / 32768.0f;
                } else if (p_->bits == 24) {
                    int32_t v = static_cast<int32_t>(s[0]) |
                                (static_cast<int32_t>(s[1]) << 8) |
                                (static_cast<int32_t>(s[2]) << 16);
                    if (v & 0x800000) v |= static_cast<int32_t>(0xff000000u);
                    value = v / 8388608.0f;
                } else {
                    int32_t v;
                    std::memcpy(&v, s, sizeof(v));
                    value = v / 2147483648.0f;
                }
                sum += value;
            }
            samples_[i] = std::clamp(sum / p_->channels, -1.0f, 1.0f);
        }
        callback(samples_.data(), samples_.size());
        hr = p_->capture->ReleaseBuffer(frames);
        if (FAILED(hr)) return false;
        hr = p_->capture->GetNextPacketSize(&packets);
        if (FAILED(hr)) return false;
    }
    return true;
}

void AudioMonitor::Start(AudioSource source) {
    Stop();
    {
        std::lock_guard<std::mutex> lock(mutex_);
        snapshot_ = {};
        lastUpdate_ = 0;
    }
    stop_ = false;
    worker_ = std::thread(&AudioMonitor::Run, this, source);
}

void AudioMonitor::Stop() {
    stop_ = true;
    if (worker_.joinable()) worker_.join();
}

AudioSnapshot AudioMonitor::Snapshot() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (lastUpdate_ == 0 || GetTickCount64() - lastUpdate_ > 400) return {};
    return snapshot_;
}

void AudioMonitor::Run(AudioSource source) {
    if (FAILED(CoInitializeEx(nullptr, COINIT_MULTITHREADED))) return;
    while (!stop_) {
        CaptureStream system;
        CaptureStream microphone;
        const bool useSystem = source != AudioSource::Microphone;
        const bool useMic = source != AudioSource::System;
        const bool systemReady = useSystem && system.Open(AudioSource::System);
        const bool micReady = useMic && microphone.Open(AudioSource::Microphone);
        if (!systemReady && !micReady) {
            for (int i = 0; i < 70 && !stop_; ++i) Sleep(20);
            continue;
        }
        struct FrameBuffer {
            std::array<float, kFFT> ring{};
            size_t next = 0;
            ULONGLONG lastSamples = 0;
            void Add(const float* samples, size_t count) {
                if (count) lastSamples = GetTickCount64();
                for (size_t i = 0; i < count; ++i) {
                    ring[next] = samples[i];
                    next = (next + 1) % kFFT;
                }
            }
        } systemBuffer, micBuffer;
        ULONGLONG lastCompute = 0;
        while (!stop_) {
            Sleep(15);
            bool ok = true;
            if (systemReady) ok = system.Poll([&](const float* data, size_t n) {
                systemBuffer.Add(data, n);
            });
            if (micReady && ok) ok = microphone.Poll([&](const float* data, size_t n) {
                micBuffer.Add(data, n);
            });
            if (!ok) break; // Audio device changed: reopen captures.
            const ULONGLONG now = GetTickCount64();
            if (now - lastCompute >= 33) {
                const bool freshSystem = systemReady && systemBuffer.lastSamples &&
                    now - systemBuffer.lastSamples <= 180;
                const bool freshMic = micReady && micBuffer.lastSamples &&
                    now - micBuffer.lastSamples <= 180;
                AudioSnapshot snapshot{};
                if (freshSystem) snapshot = Analyze(systemBuffer.ring, systemBuffer.next, system.SampleRate());
                if (freshMic) {
                    AudioSnapshot second = Analyze(micBuffer.ring, micBuffer.next, microphone.SampleRate());
                    if (freshSystem) {
                        // Keep both sources independently audible to the spectrum. Averaging
                        // the raw samples would make a quiet microphone disappear in music.
                        for (size_t i = 0; i < snapshot.bands.size(); ++i)
                            snapshot.bands[i] = std::max(snapshot.bands[i], second.bands[i]);
                        for (size_t i = 0; i < snapshot.waveform.size(); ++i)
                            snapshot.waveform[i] = std::clamp(0.5f * (snapshot.waveform[i] +
                                second.waveform[i]), -1.0f, 1.0f);
                    } else snapshot = second;
                }
                {
                    std::lock_guard<std::mutex> lock(mutex_);
                    snapshot_ = snapshot;
                    lastUpdate_ = now;
                }
                lastCompute = now;
            }
        }
    }
    CoUninitialize();
}

std::wstring MicrophoneRecorder::RecordingsFolder() {
    PWSTR documents = nullptr;
    std::wstring folder;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Documents, KF_FLAG_DEFAULT, nullptr, &documents))) {
        folder = documents;
        CoTaskMemFree(documents);
    } else {
        wchar_t tmp[MAX_PATH]{};
        GetEnvironmentVariableW(L"USERPROFILE", tmp, MAX_PATH);
        folder = std::wstring(tmp) + L"\\Documents";
    }
    folder += L"\\ChaqRec";
    CreateDirectoryW(folder.c_str(), nullptr);
    folder += L"\\Recordings";
    CreateDirectoryW(folder.c_str(), nullptr);
    return folder;
}

std::wstring MicrophoneRecorder::LastFile() const {
    std::lock_guard<std::mutex> lock(fileMutex_);
    return lastFile_;
}

void MicrophoneRecorder::Start(HWND notify, RecordFormat format) {
    if (requested_) return;
    if (worker_.joinable()) worker_.join();
    requested_ = true;
    worker_ = std::thread(&MicrophoneRecorder::Run, this, notify, format);
}

void MicrophoneRecorder::Stop() {
    requested_ = false;
    if (worker_.joinable()) worker_.join();
}

void MicrophoneRecorder::Run(HWND notify, RecordFormat format) {
    bool success = false;
    bool started = false;
    const bool comReady = SUCCEEDED(CoInitializeEx(nullptr, COINIT_MULTITHREADED));
    if (comReady) {
        {
            CaptureStream stream;
            if (requested_ && stream.Open(AudioSource::Microphone)) {
                std::wstring fileName;
                const std::wstring folder = RecordingsFolder();
                const bool m4a = format == RecordFormat::M4A;
                for (int i = 0; i < 100; ++i) {
                    std::wstring candidate = folder + L"\\ChaqRec_" + TimeStamp();
                    if (i) candidate += L"_" + std::to_wstring(i);
                    candidate += m4a ? L".m4a" : L".wav";
                    if (GetFileAttributesW(candidate.c_str()) == INVALID_FILE_ATTRIBUTES) {
                        fileName = candidate;
                        break;
                    }
                }
                if (!fileName.empty() && requested_) {
                    if (m4a) {
                        M4aRecorder writer;
                        if (writer.Open(fileName, stream.SampleRate())) {
                            {
                                std::lock_guard<std::mutex> lock(fileMutex_);
                                lastFile_ = fileName;
                            }
                            started = true;
                            PostMessageW(notify, WM_CHAQ_RECORD_STATUS, CHAQ_RECORD_STARTED, 0);
                            success = true;
                            while (requested_) {
                                Sleep(15);
                                bool writeOkay = true;
                                const bool captureOkay = stream.Poll([&](const float* samples, size_t count) {
                                    if (writeOkay) writeOkay = writer.Write(samples, count);
                                });
                                if (!captureOkay || !writeOkay) {
                                    success = false;
                                    break;
                                }
                            }
                            if (!writer.Close()) success = false;
                        } else {
                            // Never keep an invalid half-open .m4a on initialization errors.
                            DeleteFileW(fileName.c_str());
                        }
                    } else {
                        HANDLE file = CreateFileW(fileName.c_str(), GENERIC_WRITE,
                            FILE_SHARE_READ, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
                        if (file != INVALID_HANDLE_VALUE) {
                            WaveHeader header;
                            header.sampleRate = stream.SampleRate();
                            header.byteRate = header.sampleRate * 2;
                            uint32_t bytes = 0;
                            if (WriteExact(file, &header, sizeof(header))) {
                                {
                                    std::lock_guard<std::mutex> lock(fileMutex_);
                                    lastFile_ = fileName;
                                }
                                started = true;
                                PostMessageW(notify, WM_CHAQ_RECORD_STATUS, CHAQ_RECORD_STARTED, 0);
                                success = true;
                                std::vector<int16_t> pcm;
                                while (requested_) {
                                    Sleep(15);
                                    bool writeOkay = true;
                                    const bool captureOkay = stream.Poll([&](const float* samples, size_t count) {
                                        if (!writeOkay) return;
                                        pcm.resize(count);
                                        for (size_t j = 0; j < count; ++j) {
                                            const float value = std::clamp(samples[j], -1.0f, 1.0f);
                                            pcm[j] = static_cast<int16_t>(std::lround(value * (value < 0 ? 32768.0f : 32767.0f)));
                                        }
                                        const uint64_t packetBytes = count * sizeof(int16_t);
                                        if (packetBytes > UINT32_MAX - 36u - bytes ||
                                            !WriteExact(file, pcm.data(), static_cast<DWORD>(packetBytes))) {
                                            writeOkay = false;
                                        } else bytes += static_cast<uint32_t>(packetBytes);
                                    });
                                    if (!captureOkay || !writeOkay) { success = false; break; }
                                }
                            }
                            header.dataSize = bytes;
                            header.riffSize = 36 + bytes;
                            LARGE_INTEGER begin{};
                            if (!SetFilePointerEx(file, begin, nullptr, FILE_BEGIN) ||
                                !WriteExact(file, &header, sizeof(header))) success = false;
                            FlushFileBuffers(file);
                            CloseHandle(file);
                            // Even on an interruption, WAV may contain recoverable samples.
                        }
                    }
                }
            }
        }
        CoUninitialize();
    }
    if (!success && (started || requested_))
        PostMessageW(notify, WM_CHAQ_RECORD_STATUS, CHAQ_RECORD_FAILED, 0);
    requested_ = false;
}
