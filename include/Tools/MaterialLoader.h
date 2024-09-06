#pragma once
#include "stdafx.h"
#include "DXSampleHelper.h"
#include "Tools/stb_image.h"

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
	}
	static void CreateTextureFromFile(std::string fileName, ComPtr<ID3D12Device>& device, ComPtr<ID3D12GraphicsCommandList>& cmdList, ComPtr<ID3D12Resource>& res, ComPtr<ID3D12Resource>& uploadHeap)
	{
		int texWidth, texHeight;
		//Real components num of tex(RGB/RGBA)
		int numComponents;
		int singlePixelSize = sizeof(uint32_t); //R8G8B8A8
		//OpenGL bottom-left, DirectX top-left
		//So the texture in the buffer should be flipped manually/automatically in DirectX.
		stbi_set_flip_vertically_on_load(true); 
		stbi_uc* tex = stbi_load(fileName.c_str(), &texWidth, &texHeight, &numComponents, STBI_rgb_alpha);

		//https://github.com/microsoft/DirectX-Graphics-Samples/blob/master/Samples/Desktop/D3D12HelloWorld/src/HelloTexture/D3D12HelloTexture.cpp
		//Similar to CreateDefaultBuffer()
		D3D12_RESOURCE_DESC texDesc = {};
		texDesc.MipLevels = 1;
		texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		texDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
		texDesc.DepthOrArraySize = 1;
		texDesc.SampleDesc.Count = 1;
		texDesc.SampleDesc.Quality = 0;
		texDesc.Width = texWidth;
		texDesc.Height = texHeight;

		ThrowIfFailed(device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&texDesc,
			D3D12_RESOURCE_STATE_COPY_DEST,
			nullptr,
			IID_PPV_ARGS(&res)
		));

		const UINT64 uploadBufferSize = GetRequiredIntermediateSize(res.Get(), 0, 1);

		//upload buffer
		ThrowIfFailed(device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&uploadHeap)
		));

		D3D12_SUBRESOURCE_DATA textureData = {};
		textureData.pData = tex;
		textureData.RowPitch = texWidth * singlePixelSize;
		//?
		textureData.SlicePitch = textureData.RowPitch * texHeight;

		//CommandList*, defaultHeap*, uploadHeap*(intermediate), intermediateOffset, firstSubRes, NumSubRes, SubResource*
		UpdateSubresources(cmdList.Get(), res.Get(), uploadHeap.Get(), 0, 0, 1, &textureData);
		//Transit from copy_dest to pixel_shader_res
		cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(res.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));
	}
	void ReadMtlFile(std::string path, std::string fileName, std::vector<Material>& mtlList);
};


//MaterialLoader()
//{
//	//According to https://github.com/Microsoft/DirectXTK12/wiki/WICTextureLoader
//	//classic Windows desktop application you have to do this explicitly:
//	ThrowIfFailed(Windows::Foundation::Initialize(RO_INIT_MULTITHREADED));
//#if (_WIN32_WINNT >= 0x0A00 /*_WIN32_WINNT_WIN10*/)
//	ThrowIfFailed(Microsoft::WRL::Wrappers::RoInitializeWrapper(RO_INIT_MULTITHREADED));
//#else
//	HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
//	ThrowIfFailed(initialize);
//#endif
//}
//static void MaterialLoader::CreateTextureFromFileWIC(std::string fileName, ComPtr<ID3D12Device>& device, ComPtr<ID3D12GraphicsCommandList>& cmdList, ComPtr<ID3D12Resource>& res, ComPtr<ID3D12Resource>& uploadHeap)
//{
//	//UpdateSubresources also used in CreateDefaultBuffer()
//	//DirectX::CreateWICTextureFromFile();
//
//	//https://github.com/Microsoft/DirectXTK12/wiki/WICTextureLoader
//
//	std::unique_ptr<uint8_t[]> decodedData;
//	D3D12_SUBRESOURCE_DATA subresource;
//
//	std::wstring path = std::wstring_convert<std::codecvt_utf8<wchar_t>>().from_bytes(fileName);
//
//	ThrowIfFailed(DirectX::LoadWICTextureFromFileEx(device.Get(), path.c_str(), 0, D3D12_RESOURCE_FLAG_NONE,
//		DirectX::WIC_LOADER_DEFAULT, &res, decodedData, subresource));
//
//	const UINT64 uploadBufferSize = GetRequiredIntermediateSize(res.Get(), 0, 1);
//	// Create the GPU upload buffer.
//	ThrowIfFailed(
//		device->CreateCommittedResource(
//			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
//			D3D12_HEAP_FLAG_NONE,
//			&CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize),
//			D3D12_RESOURCE_STATE_GENERIC_READ,
//			nullptr,
//			IID_PPV_ARGS(uploadHeap.GetAddressOf())));
//
//	UpdateSubresources(cmdList.Get(), res.Get(), uploadHeap.Get(), 0, 0, 1, &subresource);
//
//	cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(res.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));
//
//	//ThrowIfFailed(commandList->Close());
//	//m_deviceResources->GetCommandQueue()->ExecuteCommandLists(1,
//	//    CommandListCast(&commandList));
//}