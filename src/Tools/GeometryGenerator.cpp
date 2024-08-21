#include "Tools/GeometryGenerator.h"

GeometryGenerator::MeshData GeometryGenerator::BuildCylinder(
	float bottomR, float topR, float height, int slice, int stack)
{
	MeshData meshData;
	float stackHeight = height / stack;
	float stepR = (topR - bottomR) / slice;//sphere has a non-linear R

	float angleDelta = 2.0 * DirectX::XM_PI / slice;
	//layers/stacks, n layers need n+1 wireframe
	for (uint32_t i = 0; i <= stack; ++i)
	{
		//y ranges in [-h/2, h/2]
		float currentY = -0.5 * height + stackHeight * i;
		float currentR = bottomR + stepR * i;

		for (uint32_t j = 0; j <= slice; ++j)
		{
			Vertex v;
			float c = cosf(angleDelta * j);
			float s = sinf(angleDelta * j);
			v.position = DirectX::XMFLOAT3(
				c * currentR,
				currentY,
				s * currentR
			);
			//Range from (0, 1)
			v.texCoordinate = DirectX::XMFLOAT2(
				(float)j / slice,
				1.0f - (float)i / stack
			);
			//unit length ?
			//Perpendicular to (c,0,s)?
			//切线向量
			v.tangentU = DirectX::XMFLOAT3(-s, 0.0f, c);

			float dr = bottomR - topR;
			//指向圆柱体的轴线
			//计算副切线
			DirectX::XMFLOAT3 bitangent(dr * c, -height, dr * s);
			DirectX::XMVECTOR T = DirectX::XMLoadFloat3(&v.tangentU);//切线
			DirectX::XMVECTOR B = DirectX::XMLoadFloat3(&bitangent);//副切线
			DirectX::XMVECTOR N = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(T, B));
			DirectX::XMStoreFloat3(&v.normal, N);

			meshData.vertices.push_back(v);
		}
	}
	uint32_t ringVertexCount = slice + 1;
	for (uint32_t i = 0; i < stack; ++i)
	{

		for (uint32_t j = 0; j < slice; ++j)
		{
			//ABC
			meshData.indices.push_back(i * ringVertexCount + j);
			meshData.indices.push_back((i+1) * ringVertexCount + j);
			meshData.indices.push_back((i + 1) * ringVertexCount + j + 1);

			//ACD
			meshData.indices.push_back(i * ringVertexCount + j);
			meshData.indices.push_back((i + 1) * ringVertexCount + j + 1);
			meshData.indices.push_back(i * ringVertexCount + j + 1);
		}
	}
	BuildCylinderCap(bottomR, topR, height, slice, stack, meshData);
	return meshData;
}
void GeometryGenerator::BuildCylinderCap(
	float bottomR, float topR, float height, 
	uint32_t slice, uint32_t stack, MeshData& meshData)
{
	//Top
	uint32_t baseIndex = (uint32_t)meshData.vertices.size();
	float y = 0.5f * height;
	float dTheta = 2.0f * DirectX::XM_PI / slice;
	// Duplicate cap ring vertices because the texture coordinates and 
	// normals differ.
	for (uint32_t i = 0; i <= slice; ++i)
	{
		float x = topR * cosf(i * dTheta);
		float z = topR * sinf(i * dTheta);
		// Scale down by the height to try and make top cap texture coord
		// area proportional to base.
		float u = x / height + 0.5f;
		float v = z / height + 0.5f;
		meshData.vertices.push_back(
			Vertex(x, y, z, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, u, v));
	}
	// Cap center vertex.
	meshData.vertices.push_back(Vertex(0.0f, y, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.5f,0.5f));
	// Index of center vertex.
	uint32_t centerIndex = (uint32_t)meshData.vertices.size() - 1;
	for (uint32_t i = 0; i < slice; ++i)
	{
		meshData.indices.push_back(centerIndex);
		meshData.indices.push_back(baseIndex + i + 1);
		meshData.indices.push_back(baseIndex + i);
	}

	//Bottom
	baseIndex = (uint32_t)meshData.vertices.size();
	y = -0.5f * height;
	for (uint32_t i = 0; i <= slice; ++i)
	{
		float x = bottomR * cosf(i * dTheta);
		float z = bottomR * sinf(i * dTheta);
		// Scale down by the height to try and make top cap texture coord
		// area proportional to base.
		float u = x / height + 0.5f;
		float v = z / height + 0.5f;
		meshData.vertices.push_back(
			Vertex(x, y, z, 0.0f, -1.0f, 0.0f, -1.0f, 0.0f, 0.0f, u, v));
	}
	// Cap center vertex.
	meshData.vertices.push_back(Vertex(0.0f, -y, 0.0f, 0.0f, -1.0f, 0.0f, -1.0f, 0.0f, 0.0f, 0.5f, 0.5f));
	// Index of center vertex.
	centerIndex = (uint32_t)meshData.vertices.size() - 1;
	for (uint32_t i = 0; i < slice; ++i)
	{
		meshData.indices.push_back(centerIndex);
		meshData.indices.push_back(baseIndex + i + 1);
		meshData.indices.push_back(baseIndex + i);
	}
}

GeometryGenerator::MeshData GeometryGenerator::BuildBox(float length, float width, float height)
{
	MeshData meshData;

	float l2 = 0.5f * length;
	float h2 = 0.5f * height;
	float w2 = 0.5f * width;

	auto& v = meshData.vertices;

	// Fill in the front face vertex data.
	v.push_back(Vertex(-l2, +h2, -w2, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f));
	v.push_back(Vertex(-l2, -h2, -w2, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f));
	v.push_back(Vertex(+l2, -h2, -w2, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f));
	v.push_back(Vertex(+l2, +h2, -w2, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f));
	//back			
	v.push_back(Vertex(+l2, +h2, +w2, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f));
	v.push_back(Vertex(+l2, -h2, +w2, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f));
	v.push_back(Vertex(-l2, -h2, +w2, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f));
	v.push_back(Vertex(-l2, +h2, +w2, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f));
	//top		
	v.push_back(Vertex(-l2, +h2, +w2, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f));
	v.push_back(Vertex(-l2, +h2, -w2, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f));
	v.push_back(Vertex(+l2, +h2, -w2, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f));
	v.push_back(Vertex(+l2, +h2, +w2, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f));
	//Bottom
	v.push_back(Vertex(-l2, -h2, -w2, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f));
	v.push_back(Vertex(-l2, -h2, +w2, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f));
	v.push_back(Vertex(+l2, -h2, +w2, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f));
	v.push_back(Vertex(+l2, -h2, -w2, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f));
	//Left
	v.push_back(Vertex(-l2, +h2, +w2, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f));
	v.push_back(Vertex(-l2, -h2, +w2, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f));
	v.push_back(Vertex(-l2, -h2, -w2, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f));
	v.push_back(Vertex(-l2, +h2, -w2, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f));
	//Right
	v.push_back(Vertex(+l2, +h2, +w2, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f));
	v.push_back(Vertex(+l2, -h2, +w2, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f));
	v.push_back(Vertex(+l2, -h2, -w2, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f));
	v.push_back(Vertex(+l2, +h2, -w2, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f));

	//indices
	for (uint32_t i = 0; i < 24; i+=4)
	{
		meshData.indices.push_back(i);
		meshData.indices.push_back(i+1);
		meshData.indices.push_back(i+2);

		meshData.indices.push_back(i);
		meshData.indices.push_back(i+2);
		meshData.indices.push_back(i+3);
	}
	return meshData;
}