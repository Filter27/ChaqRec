#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include "Transcriber.h"

#include <windows.h>
#include <objbase.h>
#include <shlobj.h>
#include <winhttp.h>
#include <bcrypt.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mferror.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace {
constexpr wchar_t kModelUrl[] =
    L"https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-base.bin";
constexpr wchar_t kModelSha256[] =
    L"60ed5bc3dd14eea856493d334349b405782ddcaf0028d4b5df4088345fba2efe";
constexpr unsigned long long kModelBytes = 147951465ULL;
constexpr DWORD kRate = 16000;

struct UniqueHandle {
    HANDLE value = INVALID_HANDLE_VALUE;
    ~UniqueHandle() { if (value != INVALID_HANDLE_VALUE && value != nullptr) CloseHandle(value); }
};
template<typename T> struct ComRef {
    T* value = nullptr;
    ~ComRef() { if (value) value->Release(); }
    T** out() { return &value; }
    T* operator->() const { return value; }
};

bool WriteData(HANDLE f, const void* data, DWORD bytes) {
    DWORD actual = 0;
    return WriteFile(f, data, bytes, &actual, nullptr) && actual == bytes;
}
std::wstring DirectoryOf(const std::wstring& path) {
    const auto slash = path.find_last_of(L"\\/");
    return slash == std::wstring::npos ? L"." : path.substr(0, slash);
}
std::wstring StripExtension(const std::wstring& path) {
    const auto slash = path.find_last_of(L"\\/");
    const auto dot = path.find_last_of(L'.');
    return dot != std::wstring::npos && (slash == std::wstring::npos || dot > slash)
        ? path.substr(0, dot) : path;
}
std::wstring AppDirectory() {
    std::wstring name(32768, L'\0');
    const DWORD n = GetModuleFileNameW(nullptr, name.data(), static_cast<DWORD>(name.size()));
    name.resize(n);
    return DirectoryOf(name);
}
std::wstring ModelDirectory() {
    PWSTR path = nullptr;
    std::wstring root;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_DEFAULT, nullptr, &path))) {
        root = path;
        CoTaskMemFree(path);
    } else {
        wchar_t tmp[MAX_PATH]{};
        GetEnvironmentVariableW(L"LOCALAPPDATA", tmp, MAX_PATH);
        root = tmp;
    }
    if (root.empty()) return L"";
    root += L"\\ChaqRec";
    CreateDirectoryW(root.c_str(), nullptr);
    root += L"\\Models";
    CreateDirectoryW(root.c_str(), nullptr);
    return root;
}

// Verify a fixed model checksum: a partial download or HTML error page is never executed.
bool VerifyModel(const std::wstring& file) {
    UniqueHandle input;
    input.value = CreateFileW(file.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                              OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (input.value == INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER size{};
    if (!GetFileSizeEx(input.value, &size) || size.QuadPart != kModelBytes) return false;

    BCRYPT_ALG_HANDLE alg = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    bool ok = false;
    if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) == 0) {
        if (BCryptCreateHash(alg, &hash, nullptr, 0, nullptr, 0, 0) == 0) {
            std::array<UCHAR, 65536> buffer{};
            DWORD count = 0;
            bool readOK = true;
            for (;;) {
                if (!ReadFile(input.value, buffer.data(), static_cast<DWORD>(buffer.size()), &count, nullptr)) {
                    readOK = false;
                    break;
                }
                if (!count) break;
                if (BCryptHashData(hash, buffer.data(), count, 0) != 0) { readOK = false; break; }
            }
            std::array<UCHAR, 32> digest{};
            if (readOK && BCryptFinishHash(hash, digest.data(), static_cast<ULONG>(digest.size()), 0) == 0) {
                constexpr wchar_t hex[] = L"0123456789abcdef";
                std::wstring actual;
                actual.reserve(64);
                for (UCHAR b : digest) { actual.push_back(hex[b >> 4]); actual.push_back(hex[b & 15]); }
                ok = actual == kModelSha256;
            }
        }
    }
    if (hash) BCryptDestroyHash(hash);
    if (alg) BCryptCloseAlgorithmProvider(alg, 0);
    return ok;
}

// Download streaming over WinHTTP with redirects, only on explicit user consent.
bool DownloadModel(const std::wstring& destination, const std::atomic<bool>& cancel) {
    HINTERNET session = WinHttpOpen(L"ChaqRec/0.3.0", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                                    WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session) return false;
    WinHttpSetTimeouts(session, 20000, 20000, 30000, 30000);
    URL_COMPONENTS url{};
    url.dwStructSize = sizeof(url);
    url.dwHostNameLength = static_cast<DWORD>(-1);
    url.dwUrlPathLength = static_cast<DWORD>(-1);
    url.dwExtraInfoLength = static_cast<DWORD>(-1);
    bool success = false;
    if (WinHttpCrackUrl(kModelUrl, 0, 0, &url)) {
        const std::wstring host(url.lpszHostName, url.dwHostNameLength);
        std::wstring path(url.lpszUrlPath, url.dwUrlPathLength);
        if (url.lpszExtraInfo) path.append(url.lpszExtraInfo, url.dwExtraInfoLength);
        HINTERNET connection = WinHttpConnect(session, host.c_str(), url.nPort, 0);
        if (connection) {
            HINTERNET request = WinHttpOpenRequest(connection, L"GET", path.c_str(), nullptr,
                WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                url.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0);
            if (request) {
                if (WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                    WINHTTP_NO_REQUEST_DATA, 0, 0, 0) && WinHttpReceiveResponse(request, nullptr)) {
                    DWORD status = 0, bytes = sizeof(status);
                    if (WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                        WINHTTP_HEADER_NAME_BY_INDEX, &status, &bytes, WINHTTP_NO_HEADER_INDEX) && status == 200) {
                        UniqueHandle output;
                        output.value = CreateFileW(destination.c_str(), GENERIC_WRITE, 0, nullptr,
                                                   CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
                        if (output.value != INVALID_HANDLE_VALUE) {
                            std::array<BYTE, 65536> buffer{};
                            success = true;
                            unsigned long long total = 0;
                            while (!cancel.load()) {
                                DWORD got = 0;
                                if (!WinHttpReadData(request, buffer.data(),
                                                     static_cast<DWORD>(buffer.size()), &got)) {
                                    success = false; break;
                                }
                                if (got == 0) break;
                                total += got;
                                if (total > kModelBytes || !WriteData(output.value, buffer.data(), got)) {
                                    success = false; break;
                                }
                            }
                            if (cancel.load() || total != kModelBytes) success = false;
                        }
                    }
                }
                WinHttpCloseHandle(request);
            }
            WinHttpCloseHandle(connection);
        }
    }
    WinHttpCloseHandle(session);
    if (!success) DeleteFileW(destination.c_str());
    return success;
}

#pragma pack(push, 1)
struct WavHeader {
    char riff[4] = {'R','I','F','F'};
    uint32_t riffSize = 36;
    char wave[4] = {'W','A','V','E'};
    char fmt[4] = {'f','m','t',' '};
    uint32_t fmtSize = 16;
    uint16_t format = 1;
    uint16_t channels = 1;
    uint32_t rate = kRate;
    uint32_t bytesPerSecond = kRate * 2;
    uint16_t align = 2;
    uint16_t bits = 16;
    char data[4] = {'d','a','t','a'};
    uint32_t dataSize = 0;
};
#pragma pack(pop)
static_assert(sizeof(WavHeader) == 44);

class WavOutput {
public:
    ~WavOutput() { if (file_.value != INVALID_HANDLE_VALUE) Close(false); }
    bool Open(const std::wstring& path, DWORD inputRate) {
        path_ = path;
        inputRate_ = inputRate;
        file_.value = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                                   FILE_ATTRIBUTE_NORMAL, nullptr);
        WavHeader header;
        return file_.value != INVALID_HANDLE_VALUE && WriteData(file_.value, &header, sizeof(header));
    }
    bool Add(const BYTE* raw, size_t bytes, const std::atomic<bool>& cancel) {
        if (!inputRate_ || (bytes % 2)) return false;
        auto* pcm = reinterpret_cast<const int16_t*>(raw);
        const size_t count = bytes / 2;
        if (inputRate_ == kRate) {
            if (bytes > UINT32_MAX - 36u - total_) return false;
            if (!WriteData(file_.value, raw, static_cast<DWORD>(bytes))) return false;
            total_ += static_cast<uint32_t>(bytes);
            return true;
        }
        std::vector<int16_t> downsample;
        downsample.reserve(count * kRate / inputRate_ + 8);
        const double step = static_cast<double>(inputRate_) / kRate;
        for (size_t i = 0; i < count; ++i) {
            if ((i % 8192 == 0) && cancel.load()) return false;
            const int16_t now = pcm[i];
            const double left = incoming_ ? static_cast<double>(incoming_ - 1) : 0.0;
            while (nextPosition_ <= static_cast<double>(incoming_) + 1e-7) {
                const double fraction = incoming_ ? std::clamp(nextPosition_ - left, 0.0, 1.0) : 0.0;
                const double value = previous_ + (now - previous_) * fraction;
                downsample.push_back(static_cast<int16_t>(std::lround(std::clamp(value, -32768.0, 32767.0))));
                nextPosition_ += step;
            }
            previous_ = now;
            ++incoming_;
        }
        const uint64_t toWrite = downsample.size() * sizeof(int16_t);
        if (toWrite > UINT32_MAX - 36u - total_) return false;
        if (!WriteData(file_.value, downsample.data(), static_cast<DWORD>(toWrite))) return false;
        total_ += static_cast<uint32_t>(toWrite);
        return true;
    }
    bool Close(bool finish) {
        if (file_.value == INVALID_HANDLE_VALUE) return false;
        bool success = finish && total_ > 0;
        if (finish) {
            WavHeader header;
            header.dataSize = total_;
            header.riffSize = 36u + total_;
            LARGE_INTEGER begin{};
            if (!SetFilePointerEx(file_.value, begin, nullptr, FILE_BEGIN) ||
                !WriteData(file_.value, &header, sizeof(header))) success = false;
        }
        CloseHandle(file_.value);
        file_.value = INVALID_HANDLE_VALUE;
        if (!success) DeleteFileW(path_.c_str());
        return success;
    }
private:
    UniqueHandle file_;
    std::wstring path_;
    uint32_t total_ = 0;
    DWORD inputRate_ = 0;
    uint64_t incoming_ = 0;
    double nextPosition_ = 0;
    int16_t previous_ = 0;
};

// Windows Media Foundation decodes ChaqRec's AAC/M4A and WAV; ffmpeg is not required.
bool ConvertTo16kWav(const std::wstring& audio, const std::wstring& wav,
                     const std::atomic<bool>& cancel) {
    if (FAILED(MFStartup(MF_VERSION))) return false;
    bool success = false;
    {
        ComRef<IMFSourceReader> reader;
        HRESULT hr = MFCreateSourceReaderFromURL(audio.c_str(), nullptr, reader.out());
        if (SUCCEEDED(hr)) hr = reader->SetStreamSelection(MF_SOURCE_READER_ALL_STREAMS, FALSE);
        if (SUCCEEDED(hr)) hr = reader->SetStreamSelection(MF_SOURCE_READER_FIRST_AUDIO_STREAM, TRUE);
        DWORD outputRate = 0;
        if (SUCCEEDED(hr)) {
            const DWORD rates[] = {static_cast<DWORD>(kRate), 48000u, 44100u};
            for (const DWORD rate : rates) {
                ComRef<IMFMediaType> type;
                if (FAILED(MFCreateMediaType(type.out()))) break;
                type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
                type->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
                type->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, 1);
                type->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, rate);
                type->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 16);
                type->SetUINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, 2);
                type->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, rate * 2);
                hr = reader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM, nullptr, type.value);
                if (SUCCEEDED(hr)) { outputRate = rate; break; }
            }
        }
        if (outputRate && !cancel.load()) {
            WavOutput output;
            if (output.Open(wav, outputRate)) {
                success = true;
                for (;;) {
                    if (cancel.load()) { success = false; break; }
                    DWORD streamIndex = 0, flags = 0;
                    LONGLONG timestamp = 0;
                    ComRef<IMFSample> sample;
                    hr = reader->ReadSample(MF_SOURCE_READER_FIRST_AUDIO_STREAM, 0,
                                             &streamIndex, &flags, &timestamp, sample.out());
                    if (FAILED(hr) || (flags & MF_SOURCE_READERF_ERROR)) { success = false; break; }
                    if (sample.value) {
                        ComRef<IMFMediaBuffer> buffer;
                        hr = sample->ConvertToContiguousBuffer(buffer.out());
                        if (FAILED(hr)) { success = false; break; }
                        BYTE* raw = nullptr;
                        DWORD length = 0;
                        hr = buffer->Lock(&raw, nullptr, &length);
                        if (FAILED(hr)) { success = false; break; }
                        const bool written = output.Add(raw, length, cancel);
                        buffer->Unlock();
                        if (!written) { success = false; break; }
                    }
                    if (flags & MF_SOURCE_READERF_ENDOFSTREAM) break;
                }
                success = output.Close(success);
            }
        }
    }
    MFShutdown();
    return success;
}

bool ExecuteWhisper(const std::wstring& tool, const std::wstring& model,
                    const std::wstring& wav, const std::wstring& prefix,
                    const std::atomic<bool>& cancel) {
    const std::wstring command = L"\"" + tool + L"\" -m \"" + model + L"\" -f \"" +
        wav + L"\" -l es -otxt -of \"" + prefix + L"\" -t 4";
    std::vector<wchar_t> mutableCommand(command.begin(), command.end());
    mutableCommand.push_back(L'\0');
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION proc{};
    if (!CreateProcessW(tool.c_str(), mutableCommand.data(), nullptr, nullptr, FALSE,
                        CREATE_NO_WINDOW, nullptr, nullptr, &startup, &proc)) return false;
    CloseHandle(proc.hThread);
    bool cancelled = false;
    while (WaitForSingleObject(proc.hProcess, 200) == WAIT_TIMEOUT) {
        if (cancel.load()) {
            TerminateProcess(proc.hProcess, ERROR_CANCELLED);
            cancelled = true;
            break;
        }
    }
    WaitForSingleObject(proc.hProcess, INFINITE);
    DWORD exitCode = 1;
    GetExitCodeProcess(proc.hProcess, &exitCode);
    CloseHandle(proc.hProcess);
    return !cancelled && !cancel.load() && exitCode == 0;
}
}

bool Transcriber::Start(HWND notify, std::wstring audioFile, bool transcriptOnly) {
    if (busy_.exchange(true)) return false;
    if (worker_.joinable()) worker_.join();
    cancel_ = false;
    UpdateResult(L"", L"");
    worker_ = std::thread(&Transcriber::Run, this, notify, std::move(audioFile), transcriptOnly);
    return true;
}
void Transcriber::Stop() {
    cancel_ = true;
    if (worker_.joinable()) worker_.join();
    busy_ = false;
}
std::wstring Transcriber::LastTextFile() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return lastText_;
}
std::wstring Transcriber::LastError() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return lastError_;
}
void Transcriber::UpdateResult(std::wstring file, std::wstring error) {
    std::lock_guard<std::mutex> lock(mutex_);
    lastText_ = std::move(file);
    lastError_ = std::move(error);
}
void Transcriber::Run(HWND notify, std::wstring audioFile, bool transcriptOnly) {
    auto post = [&](TranscriptionStatus status) {
        PostMessageW(notify, WM_CHAQ_TRANSCRIBE_STATUS, static_cast<WPARAM>(status), 0);
    };
    const std::wstring prefix = StripExtension(audioFile);
    const std::wstring wav = prefix + L".transcribe.tmp.wav";
    const std::wstring txt = prefix + L".txt";
    const std::wstring tool = AppDirectory() + L"\\tools\\whisper-cli.exe";
    const std::wstring folder = ModelDirectory();
    const std::wstring model = folder + L"\\ggml-base.bin";
    std::wstring error;
    bool succeeded = false;
    const HRESULT initialized = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(initialized)) error = L"No se pudo inicializar COM.";
    else {
        post(TranscriptionStatus::Preparing);
        if (GetFileAttributesW(tool.c_str()) == INVALID_FILE_ATTRIBUTES) {
            error = L"No se encontro tools\\whisper-cli.exe. Descarga el paquete portable completo desde GitHub Actions.";
        } else if (folder.empty()) {
            error = L"No se encontro la carpeta de modelos en AppData.";
        } else {
            if (!VerifyModel(model) && !cancel_) {
                post(TranscriptionStatus::Downloading);
                const std::wstring temporary = model + L".download";
                if (!DownloadModel(temporary, cancel_) || !VerifyModel(temporary) ||
                    !MoveFileExW(temporary.c_str(), model.c_str(), MOVEFILE_REPLACE_EXISTING)) {
                    DeleteFileW(temporary.c_str());
                    error = L"No se pudo descargar o verificar el modelo gratuito (148 MB). Revisa tu conexion o el acceso a Hugging Face.";
                }
            }
            if (error.empty() && !cancel_) {
                post(TranscriptionStatus::Processing);
                if (!ConvertTo16kWav(audioFile, wav, cancel_)) {
                    error = L"No fue posible convertir la grabacion a WAV de 16 kHz.";
                } else if (!ExecuteWhisper(tool, model, wav, prefix, cancel_)) {
                    error = L"whisper-cli no pudo completar la transcripcion local.";
                } else {
                    UniqueHandle handle;
                    handle.value = CreateFileW(txt.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                                               OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
                    LARGE_INTEGER size{};
                    if (handle.value != INVALID_HANDLE_VALUE &&
                        GetFileSizeEx(handle.value, &size) && size.QuadPart > 0) {
                        succeeded = true;
                    } else error = L"Whisper no devolvio texto. Revisa si se registraron voces audibles.";
                }
            }
        }
        CoUninitialize();
    }
    DeleteFileW(wav.c_str());
    if (succeeded && transcriptOnly) DeleteFileW(audioFile.c_str());
    if (succeeded) UpdateResult(txt, L"");
    else UpdateResult(L"", error);
    busy_ = false;
    post(cancel_ ? TranscriptionStatus::Cancelled :
        (succeeded ? TranscriptionStatus::Completed : TranscriptionStatus::Failed));
}
