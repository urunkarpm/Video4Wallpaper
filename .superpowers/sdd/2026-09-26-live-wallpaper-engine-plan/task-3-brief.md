# Task 3 Brief: Media Foundation Hardware Video Decoder & Zero-Copy Texture Pipeline (`VideoEngine`)

## Target Files
- `src/video/VideoDecoder.h`
- `src/video/VideoDecoder.cpp`
- `src/renderer/D3D11Renderer.h`
- `src/renderer/D3D11Renderer.cpp`
- `src/renderer/Shaders.hlsl`
- `CMakeLists.txt`
- `src/main.cpp`

## Requirements
1. `VideoDecoder` (`src/video/VideoDecoder.h` & `src/video/VideoDecoder.cpp`):
   - Struct `DecodedFrame`:
     ```cpp
     struct DecodedFrame {
         ComPtr<ID3D11Texture2D> texture;
         LONGLONG timestamp = 0; // 100-nanosecond units (PTS)
         LONGLONG duration = 0;
         bool isEndOfStream = false;
         bool isHardwareAccelerated = false;
     };
     ```
   - `bool Initialize(ID3D11Device* pDevice)`:
     - Initialize Media Foundation (`MFStartup`).
     - Create DXGI Device Manager (`MFCreateDXGIDeviceManager`).
     - Register D3D11 device (`ResetDevice`).
   - `bool OpenFile(const std::wstring& filePath)`:
     - Create `IMFAttributes` for Source Reader (`MF_SOURCE_READER_ENABLE_ADVANCED_VIDEO_PROCESSING`, `MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS`, `MF_SOURCE_READER_D3D_MANAGER`).
     - Create `IMFSourceReader` via `MFCreateSourceReaderFromURL`.
     - Configure first video stream output type (`MFMediaType_Video`, `MFVideoFormat_RGB32` or `MFVideoFormat_NV12`).
     - Set video format to RGB32 (`MFVideoFormat_RGB32`) or NV12 so decoded surface lands as D3D11 texture (`ID3D11Texture2D`).
   - `DecodedFrame GetNextFrame()`:
     - Call `m_sourceReader->ReadSample(MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, &actualStreamIndex, &flags, &timestamp, &sample)`.
     - Check `MF_SOURCE_READERF_ENDOFSTREAM`: Set `isEndOfStream = true`.
     - Extract `IMFMediaBuffer` -> `IMFDXGIBuffer` -> `GetResource` to retrieve zero-copy `ID3D11Texture2D`.
   - `bool Rewind()`:
     - Seamless zero-allocation loop re-base:
     - Call `PROPVARIANT varPosition; InitPropVariantFromInt64(0, &varPosition);`
     - Call `m_sourceReader->SetCurrentPosition(GUID_NULL, varPosition);`
     - Flush reader if needed to prepare next frame sequence without destroying source reader or D3D device context.
2. `D3D11Renderer` Update:
   - Add method `bool RenderVideoFrame(const DecodedFrame& frame)`:
     - Create or update ShaderResourceView (SRV) bound to `frame.texture`.
     - Render quad using video texture.
   - Update `Shaders.hlsl` to sample texture at register `t0` with sampler at `s0`.
3. `CMakeLists.txt`:
   - Add `src/video/VideoDecoder.cpp` to target.
4. `src/main.cpp`:
   - Add test video playback loop or fallback synthetic video loop if video path is provided.
5. Verification:
   - Compile via CMake + MSVC. Confirm VideoDecoder initializes, opens video/media files, outputs hardware textures, and performs seamless rewind on loop boundary.

## Ponytail Rules
- Use `MFVideoFormat_RGB32` for direct HW conversion to D3D11 RGB texture, ensuring zero CPU readback.
- Include `// ponytail: [ceiling] -> [upgrade path]` annotations for video decoding choices.
