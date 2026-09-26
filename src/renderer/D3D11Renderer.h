#pragma once
#include <windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include "video/VideoDecoder.h"

using Microsoft::WRL::ComPtr;

// ponytail: [Direct3D 11 Aspect-Scaling Quad Renderer] -> [DirectComposition swapchain with zero-copy Media Foundation video texture rendering and per-monitor viewport clipping]

enum class ScalingMode {
    Fill,
    Fit,
    Stretch,
    Crop,
    Original
};

class D3D11Renderer {
public:
    D3D11Renderer();
    ~D3D11Renderer();

    bool Initialize(HWND hWnd);
    void Cleanup();
    bool RenderTestFrame();
    bool RenderVideoFrame(const DecodedFrame& frame);
    bool OnResize(UINT newWidth, UINT newHeight);

    void SetScalingMode(ScalingMode mode);
    ScalingMode GetScalingMode() const { return m_scalingMode; }
    void SetVideoDimensions(UINT width, UINT height);

    ID3D11Device* GetDevice() const { return m_device.Get(); }
    ID3D11DeviceContext* GetContext() const { return m_context.Get(); }
    HWND GetHWnd() const { return m_hWnd; }

private:
    bool CreateDeviceAndSwapChain(HWND hWnd);
    bool CreateRenderTargetView();
    bool InitShadersAndBuffers();
    bool CreateDefaultTexture();
    void UpdateGeometry();
    void HandleDeviceLost();

    HWND m_hWnd = nullptr;
    UINT m_width = 0;
    UINT m_height = 0;
    UINT m_videoWidth = 1920;
    UINT m_videoHeight = 1080;
    ScalingMode m_scalingMode = ScalingMode::Fill;
    float m_animTime = 0.0f;

    ComPtr<ID3D11Device> m_device;
    ComPtr<ID3D11DeviceContext> m_context;
    ComPtr<IDXGISwapChain1> m_swapChain;
    ComPtr<ID3D11RenderTargetView> m_renderTargetView;

    ComPtr<ID3D11VertexShader> m_vertexShader;
    ComPtr<ID3D11PixelShader> m_pixelShader;
    ComPtr<ID3D11InputLayout> m_inputLayout;
    ComPtr<ID3D11Buffer> m_vertexBuffer;

    ComPtr<ID3D11SamplerState> m_samplerState;
    ComPtr<ID3D11ShaderResourceView> m_videoSRV;
    ComPtr<ID3D11Texture2D> m_currentTexture;
    ComPtr<ID3D11ShaderResourceView> m_defaultSRV;
};
