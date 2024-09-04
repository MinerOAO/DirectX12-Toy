#pragma once
#include "stdafx.h"
#include "DXSampleHelper.h"
#include "Tools/MaterialLoader.h"
#include <fstream>
#include <unordered_set>

class GeometryGenerator
{
public:
	struct Vertex
	{
		DirectX::XMFLOAT3 position;
		DirectX::XMFLOAT3 normal;
		DirectX::XMFLOAT3 tangentU;
		DirectX::XMFLOAT2 texCoordinate;
		Vertex() {}
		Vertex(
			const DirectX::XMFLOAT3& p,
			const DirectX::XMFLOAT3& n,
			const DirectX::XMFLOAT3& t,
			const DirectX::XMFLOAT2& uv
		) : position(p), normal(n), tangentU(t), texCoordinate(uv)
		{}
		Vertex(
			float px, float py, float pz,
			float nx, float ny, float nz,
			float tx, float ty, float tz,
			float u, float v
		) : position(px, py, pz), normal(nx, ny, nz), tangentU(tx, ty, tz), texCoordinate(u, v)
		{}
	};
	struct IndicesGroup
	{
		std::string mtlName;
		std::vector<uint32_t> indices;
	};
	struct MeshData
	{
		std::string name;
		std::vector<Vertex> vertices;
		std::vector<IndicesGroup> idxGroups;
		std::vector<uint32_t> indices;
	};
	MeshData BuildCylinder(float bottomR, float topR, float height, uint32_t slice, uint32_t stack);
	MeshData BuildBox(float length, float width, float height);
	MeshData BuildSphere(float radius, uint32_t slice, uint32_t stack);
	MeshData BuildGrid(float width, float depth, uint32_t m, uint32_t n);
	void ReadObjFile(std::string path, std::string fileName, std::vector<GeometryGenerator::MeshData>& storage, std::vector<MaterialLoader::Material>& mtlList);
	void ReadObjFileInOne(std::string path, std::string fileName, GeometryGenerator::MeshData& storage);//deprecated
private:
	void BuildCylinderCap(float bottomR, float topR, float height, uint32_t slice, uint32_t stack, MeshData& meshData);
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