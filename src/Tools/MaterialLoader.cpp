#include "Tools/MaterialLoader.h"
#include <fstream>
#include <functional>


void MaterialLoader::CreateTextureFromFile(std::wstring fileName, ComPtr<ID3D12Device>& device, ComPtr<ID3D12GraphicsCommandList>& cmdList, ComPtr<ID3D12Resource>& res, ComPtr<ID3D12Resource>& uploadHeap)
{
	//UpdateSubresources also used in CreateDefaultBuffer()
	//DirectX::CreateWICTextureFromFile();

    //https://github.com/Microsoft/DirectXTK12/wiki/WICTextureLoader

    std::unique_ptr<uint8_t[]> decodedData;
    D3D12_SUBRESOURCE_DATA subresource;

    ThrowIfFailed(DirectX::LoadWICTextureFromFile(device.Get(), fileName.c_str(), &res, decodedData, subresource));

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
void MaterialLoader::ReadMtlFile(std::string path, std::string fileName, std::vector<Material>& mtlList)
{
	std::ifstream mtlFile;
	mtlFile.open(path + "\\" + fileName);
	std::string line;

    Material currentMtl;
    bool firstMtl = true;
    std::unordered_map<std::string, std::function<void(std::vector<std::string>&)>> parsers = {
        {"newmtl", [&](auto& parts) {
            if (firstMtl)
                firstMtl = false;
            else
            {
                mtlList.push_back(currentMtl);
                currentMtl = Material();
            }
            //Both situation need to fill in mtlName
            currentMtl.mtlName = parts[1];
        }},
        {"map_Kd", [&](auto& parts) { currentMtl.texName = parts[1]; }},
        {"Ka", [&](auto& parts) { currentMtl.ka = {std::stof(parts[1]), std::stof(parts[2]), std::stof(parts[3])}; }},
        {"Kd", [&](auto& parts) { currentMtl.kd = {std::stof(parts[1]), std::stof(parts[2]), std::stof(parts[3])}; }},
        {"Ks", [&](auto& parts) { currentMtl.ks = {std::stof(parts[1]), std::stof(parts[2]), std::stof(parts[3])}; }},
        {"Tf", [&](auto& parts) { currentMtl.tf = {std::stof(parts[1]), std::stof(parts[2]), std::stof(parts[3])}; }},
        {"Ni", [&](auto& parts) { currentMtl.ni = std::stof(parts[1]); }},
        {"Ns", [&](auto& parts) { currentMtl.ns = std::stof(parts[1]); }}
    };

    while (std::getline(mtlFile, line))
    {
        std::vector<std::string> lineParts = SplitString(line, ' ');
        if (!lineParts.empty()) {
            if (parsers.find(lineParts[0]) != parsers.end())
            {
                auto& lineParser = parsers[lineParts[0]];
                lineParser(lineParts);
            }
        }
    }

    //Original version
    /*
	while (std::getline(mtlFile, line))
	{
		std::vector<std::string> lineParts = SplitString(line, ' ');
		if (!lineParts.empty())
		{
			if (lineParts[0] == "newmtl")
			{
                if (firstMtl)
                    firstMtl = false;
                else
                {
                    mtlList.push_back(currentMtl);
                    currentMtl = Material();
                }
                //Both situation need to fill in mtlName
                currentMtl.mtlName = lineParts[1];
			}
			if (lineParts[0] == "map_Kd")
			{
                currentMtl.texName = lineParts[1];
			}
            if (lineParts[0] == "Ka")
            {
                currentMtl.ka = DirectX::XMFLOAT3(
                    std::stof(lineParts[1]),
                    std::stof(lineParts[2]),
                    std::stof(lineParts[3]));
            }
            if (lineParts[0] == "Kd")
            {
                currentMtl.kd = DirectX::XMFLOAT3(
                    std::stof(lineParts[1]),
                    std::stof(lineParts[2]),
                    std::stof(lineParts[3]));
            }
            if (lineParts[0] == "Ks")
            {
                currentMtl.ks = DirectX::XMFLOAT3(
                    std::stof(lineParts[1]),
                    std::stof(lineParts[2]),
                    std::stof(lineParts[3]));
            }
            if (lineParts[0] == "Tf")
            {
                currentMtl.tf = DirectX::XMFLOAT3(
                    std::stof(lineParts[1]),
                    std::stof(lineParts[2]),
                    std::stof(lineParts[3]));
            }
            if (lineParts[0] == "Ni")
            {
                currentMtl.ni = std::stof(lineParts[1]);
            }
            if (lineParts[0] == "Ns")
            {
                currentMtl.ns = std::stof(lineParts[1]);
            }
		}
	}
    */

    //last one
    mtlList.push_back(currentMtl);
	mtlFile.close();
}