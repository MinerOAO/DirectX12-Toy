#pragma once
#include "stdafx.h"
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
		std::vector<Vertex> vertices;
		std::vector<uint32_t> indices;

	};
	MeshData BuildCylinder(float bottomR, float topR, float height, int slice, int stack);
	void BuildCylinderCap(float bottomR, float topR, float height, uint32_t slice, uint32_t stack, MeshData& meshData);
	MeshData BuildBox(float length, float width, float height);
};