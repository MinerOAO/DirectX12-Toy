#pragma once
#include "stdafx.h"
#include <fstream>
#include <sstream>
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
	struct MeshData
	{
		std::string name;
		std::vector<Vertex> vertices;
		std::vector<uint32_t> indices;
		std::string textureName;

	};
	MeshData BuildCylinder(float bottomR, float topR, float height, uint32_t slice, uint32_t stack);
	void BuildCylinderCap(float bottomR, float topR, float height, uint32_t slice, uint32_t stack, MeshData& meshData);
	MeshData BuildBox(float length, float width, float height);
	MeshData BuildSphere(float radius, uint32_t slice, uint32_t stack);
	MeshData BuildGrid(float width, float depth, uint32_t wSeg, uint32_t dSeg);
	void ReadObjFile(std::string filename, std::vector<GeometryGenerator::MeshData>& storage);
	void ReadObjFileInOne(std::string filename, GeometryGenerator::MeshData& storage);
};