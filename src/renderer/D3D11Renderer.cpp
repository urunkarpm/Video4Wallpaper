#include "renderer/D3D11Renderer.h"
#include "core/Logger.h"
#include <cmath>
#include <sstream>
#include <iomanip>

// ponytail: [Direct3D 11 Basic Quad Renderer] -> [DirectComposition swapchain with zero-copy Media Foundation video texture rendering]

namespace {
struct Vertex {
    float x, y, z;
    float u, v;
};

const char g_shaderSource[] = R"(
struct VSInput {
    float3 pos : POSITION;
    float2 tex : TEXCOORD0;
};

struct PSInput {
    float4 pos : SV_POSITION;
    float2 tex : TEXCOORD0;
};

PSInput VSMain(VSInput input) {
    PSInput output;
    output.pos = float4(input.pos, 1.0f);
    output.tex = input.tex;
    return output;
}

float4 PSMain(PSInput input) : SV_TARGET {
    return float4(input.tex.x, input.tex.y, 0.5f, 1.0f);
}
)";

std::string HrToString(HRESULT hr) {
    std::ostringstream ss;
    ss << "0x" << std::hex << std::uppercase << std::setfill('0') << std::setw(8) << static_cast<uint32_t>(hr);
    return ss.str();
}
}

D3D11Renderer::D3D11Renderer() = default;

D3D11Renderer::~D3D11Renderer() {
    Cleanup();
}

void D3D11Renderer::Cleanup() {
    m_vertexBuffer.Reset();
    m_inputLayout.Reset();
    m_pixelShader.Reset();
    m_vertexShader.Reset();
    m_renderTargetView.Reset();
    m_swapChain.Reset();
    m_context.Reset();
    m_device.Reset();
}

bool D3D11Renderer::Initialize(HWND hWnd) {
    m_hWnd = hWnd;
    if (!CreateDeviceAndSwapChain(hWnd)) {
        Logger::LogError("Failed to create D3D11 Device and SwapChain.");
        return false;
    }

    if (!CreateRenderTargetView()) {
        Logger::LogError("Failed to create RenderTargetView.");
        return false;
    }

    if (!InitShadersAndBuffers()) {
        Logger::LogError("Failed to initialize Shaders and Buffers.");
        return false;
    }

    Logger::LogInfo("D3D11Renderer successfully initialized.");
    return true;
}

bool D3D11Renderer::CreateDeviceAndSwapChain(HWND hWnd) {
    UINT creationFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;

    D3D_FEATURE_LEVEL featureLevels[] = {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
    };

    D3D_FEATURE_LEVEL featureLevel;
    HRESULT hr = D3D11CreateDevice(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        creationFlags,
        featureLevels,
        ARRAYSIZE(featureLevels),
        D3D11_SDK_VERSION,
        &m_device,
        &featureLevel,
        &m_context
    );

    if (FAILED(hr)) {
        Logger::LogWarning("D3D11CreateDevice with HARDWARE driver failed (" + HrToString(hr) + "). Fallback to WARP software renderer.");
        hr = D3D11CreateDevice(
            nullptr,
            D3D_DRIVER_TYPE_WARP,
            nullptr,
            creationFlags,
            featureLevels,
            ARRAYSIZE(featureLevels),
            D3D11_SDK_VERSION,
            &m_device,
            &featureLevel,
            &m_context
        );
    }

    if (FAILED(hr)) {
        Logger::LogError("D3D11CreateDevice failed with code: " + HrToString(hr));
        return false;
    }

    ComPtr<IDXGIDevice> dxgiDevice;
    hr = m_device.As(&dxgiDevice);
    if (FAILED(hr)) {
        Logger::LogError("Failed to query IDXGIDevice: " + HrToString(hr));
        return false;
    }

    ComPtr<IDXGIAdapter> dxgiAdapter;
    hr = dxgiDevice->GetAdapter(&dxgiAdapter);
    if (FAILED(hr)) {
        Logger::LogError("Failed to get IDXGIAdapter: " + HrToString(hr));
        return false;
    }

    ComPtr<IDXGIFactory2> dxgiFactory;
    hr = dxgiAdapter->GetParent(IID_PPV_ARGS(&dxgiFactory));
    if (FAILED(hr)) {
        Logger::LogError("Failed to get IDXGIFactory2: " + HrToString(hr));
        return false;
    }

    RECT rect;
    GetClientRect(hWnd, &rect);
    UINT width = rect.right - rect.left;
    UINT height = rect.bottom - rect.top;
    if (width == 0) width = 1920;
    if (height == 0) height = 1080;
    m_width = width;
    m_height = height;

    DXGI_SWAP_CHAIN_DESC1 sd = {};
    sd.Width = width;
    sd.Height = height;
    sd.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.BufferCount = 2;
    sd.Scaling = DXGI_SCALING_STRETCH;
    sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    sd.AlphaMode = DXGI_ALPHA_MODE_IGNORE;

    hr = dxgiFactory->CreateSwapChainForHwnd(
        m_device.Get(),
        hWnd,
        &sd,
        nullptr,
        nullptr,
        &m_swapChain
    );

    if (FAILED(hr)) {
        Logger::LogWarning("CreateSwapChainForHwnd FLIP_DISCARD failed (" + HrToString(hr) + "). Trying DISCARD fallback.");
        sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
        hr = dxgiFactory->CreateSwapChainForHwnd(
            m_device.Get(),
            hWnd,
            &sd,
            nullptr,
            nullptr,
            &m_swapChain
        );
    }

    if (FAILED(hr)) {
        Logger::LogError("CreateSwapChainForHwnd failed with code: " + HrToString(hr));
        return false;
    }

    return true;
}

bool D3D11Renderer::CreateRenderTargetView() {
    ComPtr<ID3D11Texture2D> backBuffer;
    HRESULT hr = m_swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer));
    if (FAILED(hr)) {
        Logger::LogError("m_swapChain->GetBuffer failed: " + HrToString(hr));
        return false;
    }

    hr = m_device->CreateRenderTargetView(backBuffer.Get(), nullptr, &m_renderTargetView);
    if (FAILED(hr)) {
        Logger::LogError("CreateRenderTargetView failed: " + HrToString(hr));
        return false;
    }

    D3D11_VIEWPORT vp = {};
    vp.Width = static_cast<float>(m_width);
    vp.Height = static_cast<float>(m_height);
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    vp.TopLeftX = 0.0f;
    vp.TopLeftY = 0.0f;
    m_context->RSSetViewports(1, &vp);

    return true;
}

bool D3D11Renderer::InitShadersAndBuffers() {
    Vertex vertices[] = {
        { -1.0f,  1.0f, 0.0f, 0.0f, 0.0f },
        {  1.0f,  1.0f, 0.0f, 1.0f, 0.0f },
        { -1.0f, -1.0f, 0.0f, 0.0f, 1.0f },

        { -1.0f, -1.0f, 0.0f, 0.0f, 1.0f },
        {  1.0f,  1.0f, 0.0f, 1.0f, 0.0f },
        {  1.0f, -1.0f, 0.0f, 1.0f, 1.0f },
    };

    D3D11_BUFFER_DESC vbd = {};
    vbd.Usage = D3D11_USAGE_DEFAULT;
    vbd.ByteWidth = sizeof(vertices);
    vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA initData = {};
    initData.pSysMem = vertices;
    HRESULT hr = m_device->CreateBuffer(&vbd, &initData, &m_vertexBuffer);
    if (FAILED(hr)) {
        Logger::LogError("CreateBuffer for VertexBuffer failed: " + HrToString(hr));
        return false;
    }

    ComPtr<ID3DBlob> vsBlob;
    ComPtr<ID3DBlob> errorBlob;

    hr = D3DCompileFromFile(
        L"src/renderer/Shaders.hlsl",
        nullptr,
        nullptr,
        "VSMain",
        "vs_5_0",
        0, 0,
        &vsBlob,
        &errorBlob
    );

    if (FAILED(hr)) {
        hr = D3DCompile(
            g_shaderSource,
            strlen(g_shaderSource),
            "Shaders.hlsl",
            nullptr,
            nullptr,
            "VSMain",
            "vs_5_0",
            0, 0,
            &vsBlob,
            &errorBlob
        );
        if (FAILED(hr)) {
            std::string errStr = errorBlob ? reinterpret_cast<const char*>(errorBlob->GetBufferPointer()) : "Unknown";
            Logger::LogError("Failed to compile Vertex Shader: " + errStr);
            return false;
        }
    }

    hr = m_device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &m_vertexShader);
    if (FAILED(hr)) {
        Logger::LogError("CreateVertexShader failed: " + HrToString(hr));
        return false;
    }

    D3D11_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };

    hr = m_device->CreateInputLayout(
        layout,
        ARRAYSIZE(layout),
        vsBlob->GetBufferPointer(),
        vsBlob->GetBufferSize(),
        &m_inputLayout
    );
    if (FAILED(hr)) {
        Logger::LogError("CreateInputLayout failed: " + HrToString(hr));
        return false;
    }

    ComPtr<ID3DBlob> psBlob;
    errorBlob.Reset();
    hr = D3DCompileFromFile(
        L"src/renderer/Shaders.hlsl",
        nullptr,
        nullptr,
        "PSMain",
        "ps_5_0",
        0, 0,
        &psBlob,
        &errorBlob
    );

    if (FAILED(hr)) {
        hr = D3DCompile(
            g_shaderSource,
            strlen(g_shaderSource),
            "Shaders.hlsl",
            nullptr,
            nullptr,
            "PSMain",
            "ps_5_0",
            0, 0,
            &psBlob,
            &errorBlob
        );
        if (FAILED(hr)) {
            std::string errStr = errorBlob ? reinterpret_cast<const char*>(errorBlob->GetBufferPointer()) : "Unknown";
            Logger::LogError("Failed to compile Pixel Shader: " + errStr);
            return false;
        }
    }

    hr = m_device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &m_pixelShader);
    if (FAILED(hr)) {
        Logger::LogError("CreatePixelShader failed: " + HrToString(hr));
        return false;
    }

    return true;
}

bool D3D11Renderer::RenderTestFrame() {
    m_animTime += 0.016f;

    float r = (std::sin(m_animTime) + 1.0f) * 0.5f;
    float g = (std::cos(m_animTime * 0.7f) + 1.0f) * 0.5f;
    float b = (std::sin(m_animTime * 1.3f) + 1.0f) * 0.5f;
    float clearColor[4] = { r * 0.2f, g * 0.4f, b * 0.6f, 1.0f };

    m_context->OMSetRenderTargets(1, m_renderTargetView.GetAddressOf(), nullptr);
    m_context->ClearRenderTargetView(m_renderTargetView.Get(), clearColor);

    UINT stride = sizeof(Vertex);
    UINT offset = 0;
    m_context->IASetVertexBuffers(0, 1, m_vertexBuffer.GetAddressOf(), &stride, &offset);
    m_context->IASetInputLayout(m_inputLayout.Get());
    m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    m_context->VSSetShader(m_vertexShader.Get(), nullptr, 0);
    m_context->PSSetShader(m_pixelShader.Get(), nullptr, 0);

    m_context->Draw(6, 0);

    HRESULT hr = m_swapChain->Present(1, 0);
    if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET) {
        Logger::LogWarning("D3D11 device lost/reset during Present (" + HrToString(hr) + "). Initiating recovery...");
        HandleDeviceLost();
        return false;
    }

    return SUCCEEDED(hr);
}

void D3D11Renderer::HandleDeviceLost() {
    Cleanup();
    if (m_hWnd) {
        Initialize(m_hWnd);
    }
}
