//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************

#pragma once
#include <stdexcept>
#include <unordered_map>
// Note that while ComPtr is used to manage the lifetime of resources on the CPU,
// it has no understanding of the lifetime of resources on the GPU. Apps must account
// for the GPU lifetime of resources to avoid destroying objects that may still be
// referenced by the GPU.
using Microsoft::WRL::ComPtr;

const FLOAT white[4] = { 1.0f, 1.0f, 1.0f ,1.0f };
const FLOAT grey[4] = { 0.1f, 0.1f, 0.1f, 1.0f };
const FLOAT lightBlue[4] = { 0.4f, 0.8f, 1.0f, 1.0f };
const FLOAT lightGreen[4] = { 0.4f, 1.0f, 0.8f, 1.0f };

struct Vertex
{
    DirectX::XMFLOAT3 pos;
    DirectX::XMFLOAT3 normal;
    DirectX::XMFLOAT2 texCoordinate;
};
struct Light
{
    DirectX::XMFLOAT3 strength; // Light color
    float falloffStart;     // point/spot light only
    DirectX::XMFLOAT3 direction;// directional/spot light only
    float falloffEnd;      // point/spot light only
    DirectX::XMFLOAT3 position; // point/spot light only
    float spotPower;      // spot light only
};
struct SubmeshGeometry
{
    UINT indexCount = 0;
    UINT startIndexLocation = 0;
    INT baseVertexLocation = 0;
};
struct MeshGeometry
{
    std::string Name;

    //System memory copies
    ComPtr<ID3DBlob> vertexBufferCPU = nullptr;
    ComPtr<ID3DBlob> indexBufferCPU = nullptr;

    //Default heap
    ComPtr<ID3D12Resource> vertexBufferGPU = nullptr;
    ComPtr<ID3D12Resource> indexBufferGPU = nullptr;

    //Upload heap
    ComPtr<ID3D12Resource> vertexBufferUploader = nullptr;
    ComPtr<ID3D12Resource> indexBufferUploader = nullptr;

    //Info about the buffers
    UINT vertexByteStride = 0;
    UINT vertexBufferByteSize = 0;
    DXGI_FORMAT indexFormat = DXGI_FORMAT_R16_UINT;
    UINT indexBufferByteSize = 0;
    
    // A MeshGeometry may store multiple geometries in one vertex/index 
    // buffer.
    // Use this container to define the Submesh geometries so we can draw
    // the Submeshes individually.
    std::unordered_map<std::string, SubmeshGeometry> drawArgs;

    D3D12_VERTEX_BUFFER_VIEW VertexBufferView() const
    {
        D3D12_VERTEX_BUFFER_VIEW vbv;
        vbv.BufferLocation = vertexBufferGPU->GetGPUVirtualAddress();
        vbv.SizeInBytes = vertexBufferByteSize;
        vbv.StrideInBytes = vertexByteStride;
        return vbv;
    }

    D3D12_INDEX_BUFFER_VIEW IndexBufferView() const
    {
        D3D12_INDEX_BUFFER_VIEW ibv;
        ibv.BufferLocation = indexBufferGPU->GetGPUVirtualAddress();
        ibv.Format = indexFormat;
        ibv.SizeInBytes = indexBufferByteSize;
        return ibv;
    }
    //Dispose after upload to GPU
    void DisposeUploaders()
    {
        vertexBufferUploader = nullptr;
        indexBufferUploader = nullptr;
    }
};
struct Texture
{
    //for look up
    std::string name;
    //default?
    ComPtr<ID3D12Resource> resource = nullptr;
    ComPtr<ID3D12Resource> uploadHeap = nullptr;
};
void CreateDefaultBuffer(
    ID3D12Device* device,
    ID3D12GraphicsCommandList* cmdList,
    const void* data,
    UINT64 byteSize,
    ComPtr<ID3D12Resource>& defaultBuffer,
    ComPtr<ID3D12Resource>& uploadBuffer);
inline UINT CalcConstBufferByteSizes(UINT byteSize)
{
    // Constant buffers must be a multiple of the minimum hardware
    // allocation size (usually 256 bytes). 
    // Example: Suppose byteSize = 300.
    // (300 + 255) & ~255
    // 555 & ~255
    // 0x022B & ~0x00ff
    // 0x022B & 0xff00
    // 0x0200
    // 512
    return (byteSize + 255) & ~255;
}
ComPtr<ID3DBlob> CompileShader(const std::wstring& filename, const D3D_SHADER_MACRO* defines, const std::string& entrypointName, const std::string& target);
ComPtr<ID3DBlob> LoadBinary(const std::wstring& filename);

template<typename T>
class UploadBuffer
{
public:
    UploadBuffer(ID3D12Device* device, UINT elementCount, bool isConstantBuffer) : mIsConstantBuffer(isConstantBuffer)
    {
        // Constant buffer elements need to be multiples of 256 bytes.
        // This is because the hardware can only view constant data 
        // at m*256 byte offsets and of n*256 byte lengths. 
        // typedef struct D3D12_CONSTANT_BUFFER_VIEW_DESC {
        // UINT64 OffsetInBytes; // multiple of 256
        // UINT  SizeInBytes;  // multiple of 256
        // } D3D12_CONSTANT_BUFFER_VIEW_DESC;
        if (isConstantBuffer)
            mElementByteSize = CalcConstBufferByteSizes(sizeof(T));
        else
            mElementByteSize = sizeof(T);
        mByteSize = mElementByteSize * elementCount;
        ThrowIfFailed(device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
            D3D12_HEAP_FLAG_NONE,
            &CD3DX12_RESOURCE_DESC::Buffer(mByteSize),
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&mUploadBuffer)));

        ThrowIfFailed(mUploadBuffer->Map(0, nullptr, reinterpret_cast<void**>(&mMappedData)));
        // We do not need to unmap until we are done with the resource.
        // However, we must not write to the resource while it is in use by
        // the GPU (so we must use synchronization techniques).
    }
    UploadBuffer(const UploadBuffer& rhs) = delete; //Constructor prohibit
    UploadBuffer& operator=(const UploadBuffer& rhs) = delete; //value prohibit
    ~UploadBuffer()
    {
        if (mUploadBuffer != nullptr)
            mUploadBuffer->Unmap(0, nullptr);
        mMappedData = nullptr;
    }
    ID3D12Resource* Resource() const
    {
        return mUploadBuffer.Get();
    }
    void CopyData(int elementIndex, const T& data)
    {
        memcpy(&mMappedData[elementIndex * mElementByteSize], &data, sizeof(T));
    }
private:
    ComPtr<ID3D12Resource> mUploadBuffer;
    BYTE* mMappedData = nullptr;
    UINT mElementByteSize = 0;
    UINT mByteSize = 0;
    bool mIsConstantBuffer = false;
};

inline std::string HrToString(HRESULT hr)
{
    char s_str[64] = {};
    sprintf_s(s_str, "HRESULT of 0x%08X", static_cast<UINT>(hr));
    return std::string(s_str);
}

class HrException : public std::runtime_error
{
public:
    HrException(HRESULT hr) : std::runtime_error(HrToString(hr)), m_hr(hr) {}
    HRESULT Error() const { return m_hr; }
private:
    const HRESULT m_hr;
};

#define SAFE_RELEASE(p) if (p) (p)->Release()

inline void ThrowIfFailed(HRESULT hr) // HRESULT -> subname for long type
{
    if (FAILED(hr))
    {
        throw HrException(hr);
    }
}

static std::vector<std::string> SplitString(std::string& s, char separator)
{
    std::vector<std::string> subStrings;
    int i = 0;
    for (int j = 0; j < s.size(); ++j)
    {
        if (s[j] == separator)
        {
            subStrings.push_back(s.substr(i, j - i));//start index, length
            ++j;
            i = j;
        }
    }
    if (i < s.size())//last part
    {
        subStrings.push_back(s.substr(i, s.size() - i));
    }
    return subStrings;
}

#if defined(_DEBUG) || defined(DBG)
inline void SetName(ID3D12Object* pObject, LPCWSTR name)
{
    pObject->SetName(name);
}
inline void SetNameIndexed(ID3D12Object* pObject, LPCWSTR name, UINT index)
{
    WCHAR fullName[50];
    if (swprintf_s(fullName, L"%s[%u]", name, index) > 0)
    {
        pObject->SetName(fullName);
    }
}
#else
inline void SetName(ID3D12Object*, LPCWSTR)
{
}
inline void SetNameIndexed(ID3D12Object*, LPCWSTR, UINT)
{
}
#endif
