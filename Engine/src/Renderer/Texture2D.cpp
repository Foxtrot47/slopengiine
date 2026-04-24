#include "Engine/Renderer/Texture2D.h"
#include "Engine/Core/Logger.h"
#include <wincodec.h>
#include <vector>

#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "ole32.lib")

namespace SE {

bool Texture2D::LoadFromFile(ID3D11Device* device, const wchar_t* path)
{
    // WIC requires COM. CoInitializeEx is safe to call multiple times per thread.
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);

    ComPtr<IWICImagingFactory> factory;
    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr,
        CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory));
    if (FAILED(hr)) { SE_LOG_ERROR("WIC: failed to create factory: 0x%08X", hr); return false; }

    ComPtr<IWICBitmapDecoder> decoder;
    hr = factory->CreateDecoderFromFilename(path, nullptr, GENERIC_READ,
        WICDecodeMetadataCacheOnLoad, &decoder);
    if (FAILED(hr)) { SE_LOG_ERROR("WIC: failed to open file: 0x%08X", hr); return false; }

    ComPtr<IWICBitmapFrameDecode> frame;
    hr = decoder->GetFrame(0, &frame);
    if (FAILED(hr)) { SE_LOG_ERROR("WIC: failed to get frame: 0x%08X", hr); return false; }

    // Convert whatever pixel format the image uses into RGBA8.
    ComPtr<IWICFormatConverter> converter;
    factory->CreateFormatConverter(&converter);
    hr = converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppRGBA,
        WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom);
    if (FAILED(hr)) { SE_LOG_ERROR("WIC: format conversion failed: 0x%08X", hr); return false; }

    UINT width = 0, height = 0;
    converter->GetSize(&width, &height);

    std::vector<uint8_t> pixels(width * height * 4);
    hr = converter->CopyPixels(nullptr, width * 4,
        static_cast<UINT>(pixels.size()), pixels.data());
    if (FAILED(hr)) { SE_LOG_ERROR("WIC: CopyPixels failed: 0x%08X", hr); return false; }

    return CreateSRV(device, pixels.data(), width, height);
}

bool Texture2D::CreateFromMemory(ID3D11Device* device,
                                  const uint8_t* rgba, uint32_t width, uint32_t height)
{
    return CreateSRV(device, rgba, width, height);
}

void Texture2D::BindPS(ID3D11DeviceContext* ctx, uint32_t slot) const
{
    ctx->PSSetShaderResources(slot, 1, m_srv.GetAddressOf());
}

bool Texture2D::CreateSRV(ID3D11Device* device,
                            const uint8_t* rgba, uint32_t width, uint32_t height)
{
    m_width  = width;
    m_height = height;

    D3D11_TEXTURE2D_DESC td  = {};
    td.Width                 = width;
    td.Height                = height;
    td.MipLevels             = 1;
    td.ArraySize             = 1;
    td.Format                = DXGI_FORMAT_R8G8B8A8_UNORM;
    td.SampleDesc.Count      = 1;
    td.Usage                 = D3D11_USAGE_IMMUTABLE;
    td.BindFlags             = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA init = {};
    init.pSysMem               = rgba;
    init.SysMemPitch           = width * 4;

    HRESULT hr = device->CreateTexture2D(&td, &init, &m_texture);
    if (FAILED(hr)) { SE_LOG_ERROR("Texture2D: CreateTexture2D failed: 0x%08X", hr); return false; }

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format                          = DXGI_FORMAT_R8G8B8A8_UNORM;
    srvDesc.ViewDimension                   = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels            = 1;

    hr = device->CreateShaderResourceView(m_texture.Get(), &srvDesc, &m_srv);
    if (FAILED(hr)) { SE_LOG_ERROR("Texture2D: CreateSRV failed: 0x%08X", hr); return false; }

    SE_LOG_INFO("Texture2D: loaded %ux%u", width, height);
    return true;
}

} // namespace SE
