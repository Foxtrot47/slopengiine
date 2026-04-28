#include "Engine/Renderer/ShaderLibrary.h"
#include "Engine/Core/Logger.h"
#include <windows.h>
#include <algorithm>

namespace SE {

void ShaderLibrary::Init(ID3D11Device* device)
{
    m_device = device;
}

std::string ShaderLibrary::MakeKey(const std::wstring& file,
                                    const std::vector<ShaderDefine>& defines) const
{
    // Narrow file path via WideCharToMultiByte + sorted defines → unique cache key.
    int len = WideCharToMultiByte(CP_UTF8, 0, file.c_str(), (int)file.size(), nullptr, 0, nullptr, nullptr);
    std::string key(len, '\0');
    WideCharToMultiByte(CP_UTF8, 0, file.c_str(), (int)file.size(), key.data(), len, nullptr, nullptr);
    key += '|';
    // Sort defines by name so order doesn't affect the key.
    auto sorted = defines;
    std::sort(sorted.begin(), sorted.end(),
        [](const ShaderDefine& a, const ShaderDefine& b) { return a.name < b.name; });
    for (auto& d : sorted)
    {
        key += d.name;
        key += '=';
        key += d.value;
        key += ';';
    }
    return key;
}

const ShaderPermutation* ShaderLibrary::Get(const std::wstring& hlslFile,
                                             const std::vector<ShaderDefine>& defines)
{
    std::string key = MakeKey(hlslFile, defines);
    auto it = m_cache.find(key);
    if (it != m_cache.end())
        return &it->second;

    // Build D3D_SHADER_MACRO array (null-terminated).
    std::vector<D3D_SHADER_MACRO> macros;
    for (auto& d : defines)
        macros.push_back({ d.name.c_str(), d.value.c_str() });
    macros.push_back({ nullptr, nullptr });

    UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifdef SE_DEBUG
    flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    Microsoft::WRL::ComPtr<ID3DBlob> vsBlob, psBlob, errBlob;

    HRESULT hr = D3DCompileFromFile(hlslFile.c_str(),
        macros.data(), D3D_COMPILE_STANDARD_FILE_INCLUDE,
        "VS_Main", "vs_5_0", flags, 0, &vsBlob, &errBlob);
    if (FAILED(hr))
    {
        if (errBlob)
            SE_LOG_ERROR("ShaderLibrary VS [%ls]: %s",
                hlslFile.c_str(), (char*)errBlob->GetBufferPointer());
        return nullptr;
    }

    errBlob.Reset();
    hr = D3DCompileFromFile(hlslFile.c_str(),
        macros.data(), D3D_COMPILE_STANDARD_FILE_INCLUDE,
        "PS_Main", "ps_5_0", flags, 0, &psBlob, &errBlob);
    if (FAILED(hr))
    {
        if (errBlob)
            SE_LOG_ERROR("ShaderLibrary PS [%ls]: %s",
                hlslFile.c_str(), (char*)errBlob->GetBufferPointer());
        return nullptr;
    }

    ShaderPermutation perm;
    perm.vsBlob = vsBlob;
    SE_HR(m_device->CreateVertexShader(
        vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &perm.vs));
    SE_HR(m_device->CreatePixelShader(
        psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &perm.ps));

    auto [insertIt, _] = m_cache.emplace(std::move(key), std::move(perm));
    return &insertIt->second;
}

void ShaderLibrary::Clear()
{
    m_cache.clear();
}

} // namespace SE
