#include "video/VideoDecoder.h"
#include "core/Logger.h"
#include <propvarutil.h>
#include <mferror.h>
#include <filesystem>
#include <sstream>
#include <iomanip>

// ponytail: [MFVideoFormat_RGB32 direct HW conversion] -> [NV12 hardware decoding with compute shader YUV-to-RGB conversion]
// ponytail: [Media Foundation Source Reader synchronous ReadSample] -> [Asynchronous IMFSourceReaderCallback with triple buffered D3D11 queue]

namespace {
std::string HrToString(HRESULT hr) {
    std::ostringstream ss;
    ss << "0x" << std::hex << std::uppercase << std::setfill('0') << std::setw(8) << static_cast<uint32_t>(hr);
    return ss.str();
}
}

VideoDecoder::VideoDecoder() = default;

VideoDecoder::~VideoDecoder() {
    Cleanup();
}

void VideoDecoder::Cleanup() {
    m_sourceReader.Reset();
    m_dxgiManager.Reset();
    if (m_initialized) {
        MFShutdown();
        m_initialized = false;
    }
}

bool VideoDecoder::Initialize(ID3D11Device* pDevice) {
    if (!pDevice) {
        Logger::LogError("VideoDecoder::Initialize called with null ID3D11Device.");
        return false;
    }

    // Enable multithread protection on D3D11 device for concurrent MF DXGI Device Manager access
    ComPtr<ID3D10Multithread> multithread;
    if (SUCCEEDED(pDevice->QueryInterface(IID_PPV_ARGS(&multithread)))) {
        multithread->SetMultithreadProtected(TRUE);
    }

    HRESULT hr = MFStartup(MF_VERSION);
    if (FAILED(hr)) {
        Logger::LogError("MFStartup failed: " + HrToString(hr));
        return false;
    }
    m_initialized = true;

    hr = MFCreateDXGIDeviceManager(&m_resetToken, &m_dxgiManager);
    if (FAILED(hr)) {
        Logger::LogError("MFCreateDXGIDeviceManager failed: " + HrToString(hr));
        Cleanup();
        return false;
    }

    hr = m_dxgiManager->ResetDevice(pDevice, m_resetToken);
    if (FAILED(hr)) {
        Logger::LogError("m_dxgiManager->ResetDevice failed: " + HrToString(hr));
        Cleanup();
        return false;
    }

    Logger::LogInfo("VideoDecoder initialized with D3D11 DXGI Device Manager.");
    return true;
}

bool VideoDecoder::OpenFile(const std::wstring& filePath) {
    if (!m_initialized || !m_dxgiManager) {
        Logger::LogError("VideoDecoder::OpenFile called before successful Initialize.");
        return false;
    }

    m_sourceReader.Reset();

    ComPtr<IMFAttributes> pAttributes;
    HRESULT hr = MFCreateAttributes(&pAttributes, 3);
    if (FAILED(hr)) {
        Logger::LogError("MFCreateAttributes failed: " + HrToString(hr));
        return false;
    }

    pAttributes->SetUINT32(MF_SOURCE_READER_ENABLE_ADVANCED_VIDEO_PROCESSING, TRUE);
    pAttributes->SetUINT32(MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, TRUE);
    pAttributes->SetUnknown(MF_SOURCE_READER_D3D_MANAGER, m_dxgiManager.Get());

    hr = MFCreateSourceReaderFromURL(filePath.c_str(), pAttributes.Get(), &m_sourceReader);
    if (FAILED(hr)) {
        Logger::LogError("MFCreateSourceReaderFromURL failed: " + HrToString(hr));
        return false;
    }

    // Deselect all streams and select first video stream
    m_sourceReader->SetStreamSelection(MF_SOURCE_READER_ALL_STREAMS, FALSE);
    m_sourceReader->SetStreamSelection(MF_SOURCE_READER_FIRST_VIDEO_STREAM, TRUE);

    // Configure video output format to RGB32 (D3D11 zero-copy texture surface)
    ComPtr<IMFMediaType> pPartialType;
    hr = MFCreateMediaType(&pPartialType);
    if (FAILED(hr)) {
        Logger::LogError("MFCreateMediaType failed: " + HrToString(hr));
        return false;
    }

    pPartialType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    pPartialType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32);

    hr = m_sourceReader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, nullptr, pPartialType.Get());
    if (FAILED(hr)) {
        Logger::LogWarning("SetCurrentMediaType RGB32 failed (" + HrToString(hr) + "). Trying NV12 format fallback...");
        pPartialType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_NV12);
        hr = m_sourceReader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, nullptr, pPartialType.Get());
        if (FAILED(hr)) {
            Logger::LogError("SetCurrentMediaType failed for both RGB32 and NV12: " + HrToString(hr));
            return false;
        }
    }

    Logger::LogInfo("Video file opened successfully: " + std::filesystem::path(filePath).string());
    return true;
}

DecodedFrame VideoDecoder::GetNextFrame() {
    DecodedFrame frame;

    if (!m_sourceReader) {
        frame.isEndOfStream = true;
        return frame;
    }

    DWORD flags = 0;
    DWORD actualStreamIndex = 0;
    LONGLONG timestamp = 0;
    ComPtr<IMFSample> sample;

    for (int attempt = 0; attempt < 10; ++attempt) {
        HRESULT hr = m_sourceReader->ReadSample(
            MF_SOURCE_READER_FIRST_VIDEO_STREAM,
            0,
            &actualStreamIndex,
            &flags,
            &timestamp,
            &sample
        );

        if (FAILED(hr)) {
            Logger::LogError("ReadSample failed: " + HrToString(hr));
            frame.isEndOfStream = true;
            return frame;
        }

        if (flags & MF_SOURCE_READERF_ENDOFSTREAM) {
            frame.isEndOfStream = true;
            return frame;
        }

        if (sample) {
            break;
        }
    }

    if (sample) {
        sample->GetSampleTime(&frame.timestamp);
        sample->GetSampleDuration(&frame.duration);

        ComPtr<IMFMediaBuffer> buffer;
        HRESULT hr = sample->GetBufferByIndex(0, &buffer);
        if (SUCCEEDED(hr) && buffer) {
            ComPtr<IMFDXGIBuffer> dxgiBuffer;
            if (SUCCEEDED(buffer.As(&dxgiBuffer))) {
                dxgiBuffer->GetResource(IID_PPV_ARGS(&frame.texture));
                if (frame.texture) {
                    frame.isHardwareAccelerated = true;
                }
            }
        }
    } else {
        frame.isEndOfStream = true;
    }

    return frame;
}

bool VideoDecoder::Rewind() {
    if (!m_sourceReader) {
        return false;
    }

    PROPVARIANT varPosition;
    InitPropVariantFromInt64(0, &varPosition);
    HRESULT hr = m_sourceReader->SetCurrentPosition(GUID_NULL, varPosition);
    PropVariantClear(&varPosition);

    if (FAILED(hr)) {
        Logger::LogError("SetCurrentPosition failed during Rewind: " + HrToString(hr));
        return false;
    }

    return true;
}
