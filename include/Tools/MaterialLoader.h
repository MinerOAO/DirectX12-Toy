#pragma once
#include "stdafx.h"
#include <WICTextureLoader.h>
#include "DXSampleHelper.h"

class MaterialLoader
{
public:
	struct Material
	{
		std::string mtlName;
		std::string texName;
		DirectX::XMFLOAT3 ka, kd, ks;//ambient, diffuse, spec
		DirectX::XMFLOAT3 tf; //transmission filter   (transparent)
		float ni; //refraction
		float ns; //Specular, 1 / shininess
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
	void CreateTextureFromFile(std::wstring fileName, ComPtr<ID3D12Device>& device, ComPtr<ID3D12GraphicsCommandList>& cmdList, ComPtr<ID3D12Resource>& res, ComPtr<ID3D12Resource>& uploadHeap);
	void ReadMtlFile(std::string path, std::string fileName, std::vector<Material>& mtlList);
};