#include "renderer/D3D11Renderer.h"
#include "core/Logger.h"
#include <cmath>
#include <sstream>
#include <iomanip>

// ponytail: [Direct3D 11 Aspect-Scaling Quad Renderer] -> [DirectComposition swapchain with zero-copy Media Foundation video texture rendering and per-monitor viewport clipping]

namespace {
struct Vertex {
    float x, y, z;
    float u, v;
};

const char g_shaderSource[] = R"(
Texture2D g_texture : register(t0);
SamplerState g_sampler : register(s0);

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
    return g_texture.Sample(g_sampler, input.tex);
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
    m_defaultSRV.Reset();
    m_currentTexture.Reset();
    m_videoSRV.Reset();
    m_samplerState.Reset();
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
    Logger::LogInfo("D3D11Renderer::Initialize starting...");
    m_hWnd = hWnd;
    if (!CreateDeviceAndSwapChain(hWnd)) {
        Logger::LogError("Failed to create D3D11 Device and SwapChain.");
        return false;
    }
    Logger::LogInfo("Device and SwapChain created.");

    if (!CreateRenderTargetView()) {
        Logger::LogError("Failed to create RenderTargetView.");
        return false;
    }
    Logger::LogInfo("RenderTargetView created.");

    if (!InitShadersAndBuffers()) {
        Logger::LogError("Failed to initialize Shaders and Buffers.");
        return false;
    }

    Logger::LogInfo("D3D11Renderer successfully initialized.");
    return true;
}

bool D3D11Renderer::CreateDeviceAndSwapChain(HWND hWnd) {
    Logger::LogInfo("CreateDeviceAndSwapChain starting...");
    UINT creationFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;

    D3D_FEATURE_LEVEL featureLevels[] = {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
    };

    D3D_FEATURE_LEVEL featureLevel;

    Logger::LogInfo("Attempting D3D11CreateDevice (HARDWARE)...");
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
        Logger::LogInfo("Attempting D3D11CreateDevice (WARP)...");
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

    Logger::LogInfo("D3D11 Device created successfully. Querying DXGI Factory...");

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

    Logger::LogInfo("Creating DXGI SwapChain (" + std::to_string(width) + "x" + std::to_string(height) + ")...");

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

    Logger::LogInfo("SwapChain created successfully.");
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

bool D3D11Renderer::CreateDefaultTexture() {
    uint32_t pixels[4] = {
        0xFF0000FF, 0xFF00FF00, // RGBA format
        0xFFFF0000, 0xFFFFFFFF
    };

    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = 2;
    desc.Height = 2;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA initData = {};
    initData.pSysMem = pixels;
    initData.SysMemPitch = 2 * sizeof(uint32_t);

    ComPtr<ID3D11Texture2D> texture;
    HRESULT hr = m_device->CreateTexture2D(&desc, &initData, &texture);
    if (FAILED(hr)) {
        Logger::LogError("CreateTexture2D for default texture failed: " + HrToString(hr));
        return false;
    }

    hr = m_device->CreateShaderResourceView(texture.Get(), nullptr, &m_defaultSRV);
    if (FAILED(hr)) {
        Logger::LogError("CreateShaderResourceView for default texture failed: " + HrToString(hr));
        return false;
    }

    return true;
}

bool D3D11Renderer::InitShadersAndBuffers() {
    D3D11_BUFFER_DESC vbd = {};
    vbd.Usage = D3D11_USAGE_DYNAMIC;
    vbd.ByteWidth = sizeof(Vertex) * 6;
    vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    vbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    HRESULT hr = m_device->CreateBuffer(&vbd, nullptr, &m_vertexBuffer);
    if (FAILED(hr)) {
        Logger::LogError("CreateBuffer for dynamic VertexBuffer failed: " + HrToString(hr));
        return false;
    }

    D3D11_SAMPLER_DESC sampDesc = {};
    sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    sampDesc.MinLOD = 0;
    sampDesc.MaxLOD = D3D11_FLOAT32_MAX;

    hr = m_device->CreateSamplerState(&sampDesc, &m_samplerState);
    if (FAILED(hr)) {
        Logger::LogError("CreateSamplerState failed: " + HrToString(hr));
        return false;
    }

    if (!CreateDefaultTexture()) {
        Logger::LogError("CreateDefaultTexture failed.");
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

    UpdateGeometry();
    return true;
}

void D3D11Renderer::SetScalingMode(ScalingMode mode) {
    m_scalingMode = mode;
    std::string modeStr = "Fill";
    switch (mode) {
    case ScalingMode::Fill: modeStr = "Fill"; break;
    case ScalingMode::Fit: modeStr = "Fit"; break;
    case ScalingMode::Stretch: modeStr = "Stretch"; break;
    case ScalingMode::Crop: modeStr = "Crop"; break;
    case ScalingMode::Original: modeStr = "Original"; break;
    }
    Logger::LogInfo("SetScalingMode: " + modeStr);
    UpdateGeometry();
}

void D3D11Renderer::SetVideoDimensions(UINT width, UINT height) {
    if (width == 0 || height == 0) return;
    if (m_videoWidth != width || m_videoHeight != height) {
        m_videoWidth = width;
        m_videoHeight = height;
        Logger::LogInfo("Video dimensions updated: " + std::to_string(width) + "x" + std::to_string(height));
        UpdateGeometry();
    }
}

void D3D11Renderer::UpdateGeometry() {
    float vpW = static_cast<float>(m_width > 0 ? m_width : 1920);
    float vpH = static_cast<float>(m_height > 0 ? m_height : 1080);
    float vidW = static_cast<float>(m_videoWidth > 0 ? m_videoWidth : 1920);
    float vidH = static_cast<float>(m_videoHeight > 0 ? m_videoHeight : 1080);

    float vpAspect = vpW / vpH;
    float vidAspect = vidW / vidH;

    float xMin = -1.0f, xMax = 1.0f;
    float yMin = -1.0f, yMax = 1.0f;
    float uMin = 0.0f, uMax = 1.0f;
    float vMin = 0.0f, vMax = 1.0f;

    switch (m_scalingMode) {
    case ScalingMode::Stretch:
        xMin = -1.0f; xMax = 1.0f; yMin = -1.0f; yMax = 1.0f;
        uMin = 0.0f; uMax = 1.0f; vMin = 0.0f; vMax = 1.0f;
        break;

    case ScalingMode::Fill:
        if (vidAspect > vpAspect) {
            float scaleU = vpAspect / vidAspect;
            uMin = 0.5f - 0.5f * scaleU;
            uMax = 0.5f + 0.5f * scaleU;
        } else {
            float scaleV = vidAspect / vpAspect;
            vMin = 0.5f - 0.5f * scaleV;
            vMax = 0.5f + 0.5f * scaleV;
        }
        break;

    case ScalingMode::Fit:
        if (vidAspect > vpAspect) {
            float scaleY = vpAspect / vidAspect;
            yMin = -scaleY;
            yMax = scaleY;
        } else {
            float scaleX = vidAspect / vpAspect;
            xMin = -scaleX;
            xMax = scaleX;
        }
        break;

    case ScalingMode::Crop:
    case ScalingMode::Original: {
        float scaleX = vidW / vpW;
        float scaleY = vidH / vpH;

        if (scaleX >= 1.0f) {
            uMin = 0.5f - 0.5f / scaleX;
            uMax = 0.5f + 0.5f / scaleX;
        } else {
            xMin = -scaleX;
            xMax = scaleX;
        }

        if (scaleY >= 1.0f) {
            vMin = 0.5f - 0.5f / scaleY;
            vMax = 0.5f + 0.5f / scaleY;
        } else {
            yMin = -scaleY;
            yMax = scaleY;
        }
        break;
    }
    }

    Logger::LogInfo("UpdateGeometry: Viewport=[" + std::to_string(static_cast<int>(vpW)) + "x" + std::to_string(static_cast<int>(vpH)) +
                    "], Video=[" + std::to_string(static_cast<int>(vidW)) + "x" + std::to_string(static_cast<int>(vidH)) +
                    "], Pos=[" + std::to_string(xMin) + "," + std::to_string(yMin) + " to " + std::to_string(xMax) + "," + std::to_string(yMax) +
                    "], UV=[" + std::to_string(uMin) + "," + std::to_string(vMin) + " to " + std::to_string(uMax) + "," + std::to_string(vMax) + "]");

    Vertex vertices[] = {
        { xMin,  yMax, 0.0f, uMin, vMin },
        { xMax,  yMax, 0.0f, uMax, vMin },
        { xMin,  yMin, 0.0f, uMin, vMax },

        { xMin,  yMin, 0.0f, uMin, vMax },
        { xMax,  yMax, 0.0f, uMax, vMin },
        { xMax,  yMin, 0.0f, uMax, vMax },
    };

    if (m_context && m_vertexBuffer) {
        D3D11_MAPPED_SUBRESOURCE mapped = {};
        HRESULT hr = m_context->Map(m_vertexBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
        if (SUCCEEDED(hr)) {
            memcpy(mapped.pData, vertices, sizeof(vertices));
            m_context->Unmap(m_vertexBuffer.Get(), 0);
        }
    }
}

bool D3D11Renderer::OnResize(UINT newWidth, UINT newHeight) {
    if (newWidth == 0 || newHeight == 0) return false;
    if (m_width == newWidth && m_height == newHeight && m_renderTargetView) return true;

    Logger::LogInfo("D3D11Renderer::OnResize (" + std::to_string(newWidth) + "x" + std::to_string(newHeight) + ")");

    m_width = newWidth;
    m_height = newHeight;

    if (!m_swapChain) return false;

    m_context->OMSetRenderTargets(0, nullptr, nullptr);
    m_renderTargetView.Reset();

    HRESULT hr = m_swapChain->ResizeBuffers(0, m_width, m_height, DXGI_FORMAT_UNKNOWN, 0);
    if (FAILED(hr)) {
        Logger::LogError("m_swapChain->ResizeBuffers failed: " + HrToString(hr));
        return false;
    }

    if (!CreateRenderTargetView()) {
        Logger::LogError("CreateRenderTargetView failed during OnResize.");
        return false;
    }

    UpdateGeometry();
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

    D3D11_VIEWPORT vp = {};
    vp.Width = static_cast<float>(m_width);
    vp.Height = static_cast<float>(m_height);
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    vp.TopLeftX = 0.0f;
    vp.TopLeftY = 0.0f;
    m_context->RSSetViewports(1, &vp);

    UINT stride = sizeof(Vertex);
    UINT offset = 0;
    m_context->IASetVertexBuffers(0, 1, m_vertexBuffer.GetAddressOf(), &stride, &offset);
    m_context->IASetInputLayout(m_inputLayout.Get());
    m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    m_context->VSSetShader(m_vertexShader.Get(), nullptr, 0);
    m_context->PSSetShader(m_pixelShader.Get(), nullptr, 0);

    m_context->PSSetShaderResources(0, 1, m_defaultSRV.GetAddressOf());
    m_context->PSSetSamplers(0, 1, m_samplerState.GetAddressOf());

    m_context->Draw(6, 0);

    ID3D11ShaderResourceView* nullSRV[] = { nullptr };
    m_context->PSSetShaderResources(0, 1, nullSRV);

    HRESULT hr = m_swapChain->Present(1, 0);
    if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET) {
        Logger::LogWarning("D3D11 device lost/reset during Present (" + HrToString(hr) + "). Initiating recovery...");
        HandleDeviceLost();
        return false;
    }

    return SUCCEEDED(hr);
}

bool D3D11Renderer::RenderVideoFrame(const DecodedFrame& frame) {
    if (!frame.texture) {
        return RenderTestFrame();
    }

    D3D11_TEXTURE2D_DESC texDesc = {};
    frame.texture->GetDesc(&texDesc);
    if (texDesc.Width > 0 && texDesc.Height > 0) {
        SetVideoDimensions(texDesc.Width, texDesc.Height);
    }

    if (m_currentTexture.Get() != frame.texture.Get()) {
        m_videoSRV.Reset();
        HRESULT hr = m_device->CreateShaderResourceView(frame.texture.Get(), nullptr, &m_videoSRV);
        if (FAILED(hr)) {
            Logger::LogError("CreateShaderResourceView for video frame texture failed: " + HrToString(hr));
            return false;
        }
        m_currentTexture = frame.texture;
    }

    float clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
    m_context->OMSetRenderTargets(1, m_renderTargetView.GetAddressOf(), nullptr);
    m_context->ClearRenderTargetView(m_renderTargetView.Get(), clearColor);

    D3D11_VIEWPORT vp = {};
    vp.Width = static_cast<float>(m_width);
    vp.Height = static_cast<float>(m_height);
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    vp.TopLeftX = 0.0f;
    vp.TopLeftY = 0.0f;
    m_context->RSSetViewports(1, &vp);

    UINT stride = sizeof(Vertex);
    UINT offset = 0;
    m_context->IASetVertexBuffers(0, 1, m_vertexBuffer.GetAddressOf(), &stride, &offset);
    m_context->IASetInputLayout(m_inputLayout.Get());
    m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    m_context->VSSetShader(m_vertexShader.Get(), nullptr, 0);
    m_context->PSSetShader(m_pixelShader.Get(), nullptr, 0);

    m_context->PSSetShaderResources(0, 1, m_videoSRV.GetAddressOf());
    m_context->PSSetSamplers(0, 1, m_samplerState.GetAddressOf());

    m_context->Draw(6, 0);

    ID3D11ShaderResourceView* nullSRV[] = { nullptr };
    m_context->PSSetShaderResources(0, 1, nullSRV);

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
