#pragma once
#include <windows.h>
#include <d3d11.h>
#include <d3d10.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <wrl/client.h>
#include <string>

using Microsoft::WRL::ComPtr;

// ponytail: [MFVideoFormat_RGB32 direct HW conversion] -> [NV12 hardware decoding with compute shader YUV-to-RGB conversion]
// ponytail: [Media Foundation Source Reader synchronous ReadSample] -> [Asynchronous IMFSourceReaderCallback with triple buffered D3D11 queue]

struct DecodedFrame {
    ComPtr<ID3D11Texture2D> texture;
    LONGLONG timestamp = 0; // 100-nanosecond units (PTS)
    LONGLONG duration = 0;
    bool isEndOfStream = false;
    bool isHardwareAccelerated = false;
};

class VideoDecoder {
public:
    VideoDecoder();
    ~VideoDecoder();

    bool Initialize(ID3D11Device* pDevice);
    void Cleanup();
    bool OpenFile(const std::wstring& filePath);
    DecodedFrame GetNextFrame();
    bool Rewind();

    bool IsInitialized() const { return m_initialized; }
    bool IsFileOpen() const { return m_sourceReader != nullptr; }

private:
    bool m_initialized = false;
    UINT m_resetToken = 0;
    ComPtr<IMFDXGIDeviceManager> m_dxgiManager;
    ComPtr<IMFSourceReader> m_sourceReader;
};
