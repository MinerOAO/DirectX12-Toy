#pragma once
#include "stdafx.h"
#include <WICTextureLoader.h>
#include "DXSampleHelper.h"
#include <string.h>
#include <codecvt>

struct Texture
{
	//Index in SRV heap for diffuse texture
	int diffuseSRVHeapIndex = -1;
	//default heap?
	ComPtr<ID3D12Resource> resource = nullptr;
	ComPtr<ID3D12Resource> uploadHeap = nullptr;
};
class MaterialLoader
{
public:
	struct Material
	{
		std::string mtlName;
		std::string texPath;
		DirectX::XMFLOAT3 ka, kd, ks;//ambient, diffuse, spec
		DirectX::XMFLOAT3 tf; //transmission filter   (transparent)
		float ni; //refraction
		float ns; //Specular
	};
	MaterialLoader()
	{
		//According to https://github.com/Microsoft/DirectXTK12/wiki/WICTextureLoader
		//classic Windows desktop application you have to do this explicitly:
		ThrowIfFailed(Windows::Foundation::Initialize(RO_INIT_MULTITHREADED));
#if (_WIN32_WINNT >= 0x0A00 /*_WIN32_WINNT_WIN10*/)
		ThrowIfFailed(Microsoft::WRL::Wrappers::RoInitializeWrapper(RO_INIT_MULTITHREADED));
#else
		HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
		ThrowIfFailed(initialize);
#endif
	}
	static void MaterialLoader::CreateTextureFromFile(std::string fileName, ComPtr<ID3D12Device>& device, ComPtr<ID3D12GraphicsCommandList>& cmdList, ComPtr<ID3D12Resource>& res, ComPtr<ID3D12Resource>& uploadHeap)
	{
		//UpdateSubresources also used in CreateDefaultBuffer()
		//DirectX::CreateWICTextureFromFile();

		//https://github.com/Microsoft/DirectXTK12/wiki/WICTextureLoader

		std::unique_ptr<uint8_t[]> decodedData;
		D3D12_SUBRESOURCE_DATA subresource;

		std::wstring path = std::wstring_convert<std::codecvt_utf8<wchar_t>>().from_bytes(fileName);

		ThrowIfFailed(DirectX::LoadWICTextureFromFile(device.Get(), path.c_str(), &res, decodedData, subresource));

		const UINT64 uploadBufferSize = GetRequiredIntermediateSize(res.Get(), 0, 1);

		// Create the GPU upload buffer.
		ThrowIfFailed(
			device->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
				D3D12_HEAP_FLAG_NONE,
				&CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize),
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(uploadHeap.GetAddressOf())));

		UpdateSubresources(cmdList.Get(), res.Get(), uploadHeap.Get(), 0, 0, 1, &subresource);

		cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(res.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));

		//ThrowIfFailed(commandList->Close());
		//m_deviceResources->GetCommandQueue()->ExecuteCommandLists(1,
		//    CommandListCast(&commandList));
	}
	void ReadMtlFile(std::string path, std::string fileName, std::vector<Material>& mtlList);
};