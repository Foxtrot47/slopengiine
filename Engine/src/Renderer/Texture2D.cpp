#include "Engine/Renderer/Texture2D.h"
#include "Engine/Core/Logger.h"
#include <wincodec.h>
#include <vector>
#include <DirectXTex.h>

#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "ole32.lib")

namespace SE {

bool Texture2D::LoadFromFile(ID3D11Device* device, ID3D11DeviceContext* ctx, const wchar_t* path)
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

    return CreateSRV(device, ctx, pixels.data(), width, height);
}

bool Texture2D::LoadFromDDS(ID3D11Device* device, const wchar_t* path)
{
    using namespace DirectX;

    ScratchImage image;
    HRESULT hr = LoadFromDDSFile(path, DDS_FLAGS_NONE, nullptr, image);
    if (FAILED(hr))
    {
        char narrow[MAX_PATH];
        WideCharToMultiByte(CP_UTF8, 0, path, -1, narrow, MAX_PATH, nullptr, nullptr);
        SE_LOG_ERROR("Texture2D: DDS load failed '%s': 0x%08X", narrow, hr);
        return false;
    }

    TexMetadata meta = image.GetMetadata();
    m_width  = static_cast<uint32_t>(meta.width);
    m_height = static_cast<uint32_t>(meta.height);

    // Some legacy DDS files (DX9-era) produce DXGI_FORMAT_UNKNOWN which D3D11 rejects.
    // Decompress/convert them to RGBA8 before upload.
    UINT fmtSupport = 0;
    ScratchImage converted;
    if (meta.format == DXGI_FORMAT_UNKNOWN ||
        (IsCompressed(meta.format) &&
         (FAILED(device->CheckFormatSupport(meta.format, &fmtSupport)) ||
          !(fmtSupport & D3D11_FORMAT_SUPPORT_TEXTURE2D))))
    {
        hr = Decompress(image.GetImages(), image.GetImageCount(), meta,
                        DXGI_FORMAT_R8G8B8A8_UNORM, converted);
        if (SUCCEEDED(hr)) { image = std::move(converted); meta = image.GetMetadata(); }
    }

    ComPtr<ID3D11Resource> resource;
    hr = CreateTexture(device, image.GetImages(), image.GetImageCount(), meta,
                       resource.GetAddressOf());
    if (FAILED(hr))
    {
        // Try once more after forcing to RGBA8
        ScratchImage fallback;
        if (IsCompressed(meta.format))
            Decompress(image.GetImages(), image.GetImageCount(), meta,
                       DXGI_FORMAT_R8G8B8A8_UNORM, fallback);
        else
            Convert(*image.GetImages(), DXGI_FORMAT_R8G8B8A8_UNORM,
                    TEX_FILTER_DEFAULT, TEX_THRESHOLD_DEFAULT, fallback);

        if (fallback.GetImageCount() > 0)
        {
            meta = fallback.GetMetadata();
            hr   = CreateTexture(device, fallback.GetImages(), fallback.GetImageCount(),
                                 meta, resource.ReleaseAndGetAddressOf());
        }

        if (FAILED(hr))
        {
            char narrow[MAX_PATH];
            WideCharToMultiByte(CP_UTF8, 0, path, -1, narrow, MAX_PATH, nullptr, nullptr);
            SE_LOG_ERROR("Texture2D: DDS CreateTexture failed '%s' fmt=%u: 0x%08X",
                         narrow, (unsigned)meta.format, hr);
            return false;
        }
    }

    if (FAILED(resource.As(&m_texture)))
    {
        SE_LOG_ERROR("Texture2D: DDS resource is not a Texture2D");
        return false;
    }

    hr = device->CreateShaderResourceView(m_texture.Get(), nullptr, &m_srv);
    if (FAILED(hr)) { SE_LOG_ERROR("Texture2D: DDS CreateSRV failed: 0x%08X", hr); return false; }

    // Detect if the format carries alpha data
    m_hasAlpha = (meta.format == DXGI_FORMAT_BC3_UNORM ||
                  meta.format == DXGI_FORMAT_BC3_UNORM_SRGB ||
                  meta.format == DXGI_FORMAT_BC7_UNORM ||
                  meta.format == DXGI_FORMAT_BC7_UNORM_SRGB ||
                  meta.format == DXGI_FORMAT_R8G8B8A8_UNORM ||
                  meta.format == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB ||
                  meta.format == DXGI_FORMAT_B8G8R8A8_UNORM ||
                  meta.format == DXGI_FORMAT_R16G16B16A16_FLOAT);

    return true;
}

bool Texture2D::CreateFromMemory(ID3D11Device* device, ID3D11DeviceContext* ctx,
                                  const uint8_t* rgba, uint32_t width, uint32_t height)
{
    return CreateSRV(device, ctx, rgba, width, height);
}

void Texture2D::BindPS(ID3D11DeviceContext* ctx, uint32_t slot) const
{
    ctx->PSSetShaderResources(slot, 1, m_srv.GetAddressOf());
}

bool Texture2D::CreateSRV(ID3D11Device* device, ID3D11DeviceContext* ctx,
                            const uint8_t* rgba, uint32_t width, uint32_t height)
{
    m_width  = width;
    m_height = height;

    // MipLevels=0 lets D3D compute the full chain. GENERATE_MIPS requires both
    // SHADER_RESOURCE and RENDER_TARGET bind flags and DEFAULT usage.
    D3D11_TEXTURE2D_DESC td = {};
    td.Width            = width;
    td.Height           = height;
    td.MipLevels        = 0;
    td.ArraySize        = 1;
    td.Format           = DXGI_FORMAT_R8G8B8A8_UNORM;
    td.SampleDesc.Count = 1;
    td.Usage            = D3D11_USAGE_DEFAULT;
    td.BindFlags        = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
    td.MiscFlags        = D3D11_RESOURCE_MISC_GENERATE_MIPS;

    HRESULT hr = device->CreateTexture2D(&td, nullptr, &m_texture);
    if (FAILED(hr)) { SE_LOG_ERROR("Texture2D: CreateTexture2D failed: 0x%08X", hr); return false; }

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format                          = DXGI_FORMAT_R8G8B8A8_UNORM;
    srvDesc.ViewDimension                   = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels            = (UINT)-1; // all mip levels

    hr = device->CreateShaderResourceView(m_texture.Get(), &srvDesc, &m_srv);
    if (FAILED(hr)) { SE_LOG_ERROR("Texture2D: CreateSRV failed: 0x%08X", hr); return false; }

    // Upload mip 0 then let the GPU generate the rest.
    ctx->UpdateSubresource(m_texture.Get(), 0, nullptr, rgba, width * 4, 0);
    ctx->GenerateMips(m_srv.Get());

    m_hasAlpha = true; // RGBA8 always has alpha channel

    SE_LOG_INFO("Texture2D: loaded %ux%u with mip chain", width, height);
    return true;
}

} // namespace SE
