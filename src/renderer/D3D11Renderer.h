#pragma once
#include <windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <d3dcompiler.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

// ponytail: [Direct3D 11 Basic Quad Renderer] -> [DirectComposition swapchain with zero-copy Media Foundation video texture rendering]

class D3D11Renderer {
public:
    D3D11Renderer();
    ~D3D11Renderer();

    bool Initialize(HWND hWnd);
    void Cleanup();
    bool RenderTestFrame();

    ID3D11Device* GetDevice() const { return m_device.Get(); }
    ID3D11DeviceContext* GetContext() const { return m_context.Get(); }

private:
    bool CreateDeviceAndSwapChain(HWND hWnd);
    bool CreateRenderTargetView();
    bool InitShadersAndBuffers();
    void HandleDeviceLost();

    HWND m_hWnd = nullptr;
    UINT m_width = 0;
    UINT m_height = 0;
    float m_animTime = 0.0f;

    ComPtr<ID3D11Device> m_device;
    ComPtr<ID3D11DeviceContext> m_context;
    ComPtr<IDXGISwapChain1> m_swapChain;
    ComPtr<ID3D11RenderTargetView> m_renderTargetView;

    ComPtr<ID3D11VertexShader> m_vertexShader;
    ComPtr<ID3D11PixelShader> m_pixelShader;
    ComPtr<ID3D11InputLayout> m_inputLayout;
    ComPtr<ID3D11Buffer> m_vertexBuffer;
};
