#include "M4aRecorder.h"
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mferror.h>
#include <algorithm>
#include <cmath>
#include <cstring>

namespace {
constexpr UINT32 kOutputRate = 48000;
constexpr UINT32 kOutputBytesPerSecond = 12000; // 96 kbps AAC

void SetPcmTypes(IMFMediaType* type, bool output) {
    type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
    type->SetGUID(MF_MT_SUBTYPE, output ? MFAudioFormat_AAC : MFAudioFormat_PCM);
    type->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, kOutputRate);
    type->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, 1);
    type->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 16);
    type->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, output ? kOutputBytesPerSecond : kOutputRate * 2);
    if (!output) type->SetUINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, 2);
}
}

M4aRecorder::~M4aRecorder() { Close(); }

bool M4aRecorder::Open(const std::wstring& filename, unsigned inputRate) {
    Close();
    if (!inputRate || FAILED(MFStartup(MF_VERSION))) return false;
    mfStarted_ = true;
    inputRate_ = inputRate;
    incoming_ = emitted_ = 0;
    nextPosition_ = 0;
    previous_ = 0;

    IMFMediaType* out = nullptr;
    IMFMediaType* in = nullptr;
    HRESULT hr = MFCreateSinkWriterFromURL(filename.c_str(), nullptr, nullptr, &writer_);
    if (SUCCEEDED(hr)) hr = MFCreateMediaType(&out);
    if (SUCCEEDED(hr)) {
        SetPcmTypes(out, true);
        hr = writer_->AddStream(out, &streamIndex_);
    }
    if (SUCCEEDED(hr)) hr = MFCreateMediaType(&in);
    if (SUCCEEDED(hr)) {
        SetPcmTypes(in, false);
        hr = writer_->SetInputMediaType(streamIndex_, in, nullptr);
    }
    if (SUCCEEDED(hr)) hr = writer_->BeginWriting();
    if (out) out->Release();
    if (in) in->Release();
    if (FAILED(hr)) {
        Close();
        return false;
    }
    begun_ = true;
    return true;
}

bool M4aRecorder::WritePcm(const std::vector<int16_t>& pcm) {
    if (!writer_ || !begun_) return false;
    if (pcm.empty()) return true;
    const size_t bytes = pcm.size() * sizeof(int16_t);
    if (bytes > static_cast<size_t>(UINT32_MAX)) return false;
    IMFMediaBuffer* buf = nullptr;
    IMFSample* sample = nullptr;
    HRESULT hr = MFCreateMemoryBuffer(static_cast<DWORD>(bytes), &buf);
    if (SUCCEEDED(hr)) {
        BYTE* destination = nullptr;
        hr = buf->Lock(&destination, nullptr, nullptr);
        if (SUCCEEDED(hr)) {
            std::memcpy(destination, pcm.data(), bytes);
            buf->Unlock();
            hr = buf->SetCurrentLength(static_cast<DWORD>(bytes));
        }
    }
    if (SUCCEEDED(hr)) hr = MFCreateSample(&sample);
    if (SUCCEEDED(hr)) hr = sample->AddBuffer(buf);
    if (SUCCEEDED(hr)) {
        const auto t0 = static_cast<LONGLONG>(emitted_ * 10000000ULL / kOutputRate);
        const auto t1 = static_cast<LONGLONG>((emitted_ + pcm.size()) * 10000000ULL / kOutputRate);
        hr = sample->SetSampleTime(t0);
        if (SUCCEEDED(hr)) hr = sample->SetSampleDuration(t1 - t0);
    }
    if (SUCCEEDED(hr)) hr = writer_->WriteSample(streamIndex_, sample);
    if (sample) sample->Release();
    if (buf) buf->Release();
    if (FAILED(hr)) return false;
    emitted_ += pcm.size();
    return true;
}

bool M4aRecorder::Write(const float* mono, size_t count) {
    if (!writer_ || !begun_) return false;
    if (!count) return true;
    buffer_.clear();
    buffer_.reserve(static_cast<size_t>(count * (static_cast<double>(kOutputRate) / inputRate_) + 8));
    const double step = static_cast<double>(inputRate_) / kOutputRate;
    for (size_t i = 0; i < count; ++i) {
        float now = mono[i];
        if (!std::isfinite(now)) now = 0;
        now = std::clamp(now, -1.0f, 1.0f);
        const auto sampleIndex = incoming_++;
        if (sampleIndex == 0) {
            previous_ = now;
        }
        const double left = sampleIndex ? static_cast<double>(sampleIndex - 1) : 0.0;
        while (nextPosition_ <= static_cast<double>(sampleIndex) + 1e-7) {
            const double fraction = sampleIndex ? std::clamp(nextPosition_ - left, 0.0, 1.0) : 0.0;
            const float value = previous_ + (now - previous_) * static_cast<float>(fraction);
            buffer_.push_back(static_cast<int16_t>(std::lround(value * (value < 0 ? 32768.0f : 32767.0f))));
            nextPosition_ += step;
        }
        previous_ = now;
    }
    return WritePcm(buffer_);
}

bool M4aRecorder::Close() {
    bool ok = true;
    if (writer_) {
        if (begun_) ok = SUCCEEDED(writer_->Finalize());
        writer_->Release();
        writer_ = nullptr;
    }
    begun_ = false;
    if (mfStarted_) {
        if (FAILED(MFShutdown())) ok = false;
        mfStarted_ = false;
    }
    buffer_.clear();
    return ok;
}
