#include "Tools/GeometryGenerator.h"

GeometryGenerator::MeshData GeometryGenerator::BuildCylinder(
	float bottomR, float topR, float height, uint32_t slice, uint32_t stack)
{
	MeshData meshData;
	meshData.name = "cylinder";
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
			//��������
			v.tangentU = DirectX::XMFLOAT3(-s, 0.0f, c);

			float dr = bottomR - topR;
			//ָ��Բ���������
			//���㸱����
			DirectX::XMFLOAT3 bitangent(dr * c, -height, dr * s);
			DirectX::XMVECTOR T = DirectX::XMLoadFloat3(&v.tangentU);//����
			DirectX::XMVECTOR B = DirectX::XMLoadFloat3(&bitangent);//������
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
	meshData.name = "box";
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
	v.push_back(Vertex(+l2, +h2, -w2, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f));
	v.push_back(Vertex(+l2, -h2, -w2, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f));
	v.push_back(Vertex(+l2, -h2, +w2, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f));
	v.push_back(Vertex(+l2, +h2, +w2, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f));

	//indices
	for (uint32_t i = 0; i < 24; i+=4)
	{
		meshData.indices.push_back(i + 1);
		meshData.indices.push_back(i);
		meshData.indices.push_back(i + 3);

		meshData.indices.push_back(i + 1);
		meshData.indices.push_back(i + 3);
		meshData.indices.push_back(i + 2);
		
		//meshData.indices.push_back(i + 2);
		//meshData.indices.push_back(i+1);
		//meshData.indices.push_back(i);

		//meshData.indices.push_back(i + 3);
		//meshData.indices.push_back(i+2);
		//meshData.indices.push_back(i);
	}
	return meshData;
}
GeometryGenerator::MeshData GeometryGenerator::BuildSphere(float radius, uint32_t slice, uint32_t stack)
{
	MeshData meshData;
	meshData.name = "sphere";
	float stackHeight = radius / stack;
	float currentY = -0.5 * radius;
	float currentR = 0;

	float angleDelta = 2.0 * DirectX::XM_PI / slice;
	//layers/stacks, n layers need n+1 wireframe
	for (uint32_t i = 1; i <= stack; ++i)
	{
		currentR = sqrtf(powf(radius, 2) - powf((radius - stackHeight * i), 2)); //sphere has a non-linear R

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
			//��������
			v.tangentU = DirectX::XMFLOAT3(-s, 0.0f, c);

			float dr = radius;
			//ָ��Բ���������
			//���㸱����
			DirectX::XMFLOAT3 bitangent(dr * c, -radius, dr * s);
			DirectX::XMVECTOR T = DirectX::XMLoadFloat3(&v.tangentU);//����
			DirectX::XMVECTOR B = DirectX::XMLoadFloat3(&bitangent);//������
			DirectX::XMVECTOR N = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(T, B));
			DirectX::XMStoreFloat3(&v.normal, N);

			meshData.vertices.push_back(v);
		}
		currentY += stackHeight;
	}
	uint32_t ringVertexCount = slice + 1;
	for (uint32_t i = 0; i < stack; ++i)
	{
		for (uint32_t j = 0; j < slice; ++j)
		{
			//ABC
			meshData.indices.push_back(i * ringVertexCount + j);
			meshData.indices.push_back((i + 1) * ringVertexCount + j);
			meshData.indices.push_back((i + 1) * ringVertexCount + j + 1);

			//ACD
			meshData.indices.push_back(i * ringVertexCount + j);
			meshData.indices.push_back((i + 1) * ringVertexCount + j + 1);
			meshData.indices.push_back(i * ringVertexCount + j + 1);
		}
	}
	return meshData;
}
std::vector<std::string> SplitString(std::string& s, char separator)
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
void GeometryGenerator::ReadObjFile(std::string filename, std::vector<GeometryGenerator::MeshData>& storage)
{
	std::ifstream objFile;
	objFile.open(filename);
	std::string line;

	auto& meshDataGroup = storage;

	MeshData currentMeshData;
	bool first = true;
	int currentVertexIndex = 0;
	int v = 0, vt = 0, vn = 0;

	std::vector<DirectX::XMFLOAT3> tempV;
	std::vector<DirectX::XMFLOAT2> tempVt;
	std::vector<DirectX::XMFLOAT3> tempVn;

	tempV.push_back(DirectX::XMFLOAT3());
	tempVt.push_back(DirectX::XMFLOAT2());
	tempVn.push_back(DirectX::XMFLOAT3());

	while (std::getline(objFile, line))
	{
		std::vector<std::string> lineParts = SplitString(line, ' ');
		if (lineParts.size() > 0)
		{
			if (lineParts[0] == "g")
			{
				if (first)
					first = false;
				else
					meshDataGroup.push_back(currentMeshData);
				currentMeshData = MeshData();
				currentMeshData.name = lineParts[1] != "group" ? lineParts[1] : lineParts[2];
			}
			if (lineParts[0] == "v")
			{
				tempV.push_back(DirectX::XMFLOAT3(
					std::stof(lineParts[1]),
					std::stof(lineParts[2]),
					std::stof(lineParts[3])
				));
				++v;
			}
			if (lineParts[0] == "vt")
			{
				tempVt.push_back(DirectX::XMFLOAT2(
					std::stof(lineParts[1]),
					std::stof(lineParts[2])
				));
				++vt;
			}
			if (lineParts[0] == "vn")
			{
				tempVn.push_back(DirectX::XMFLOAT3(
					std::stof(lineParts[1]),
					std::stof(lineParts[2]),
					std::stof(lineParts[3])
				));
				++vn;
			}
			if (lineParts[0] == "f")
			{
				for (int i = 1; i < lineParts.size(); ++i)
				{
					auto vertexInfo = SplitString(lineParts[i], '/');
					DirectX::XMFLOAT3& position = tempV[std::stoi(vertexInfo[0])]; //v
					//Jump over according to mark
					if (position.x == D3D12_MAX_POSITION_VALUE)
					{
						currentMeshData.indices.push_back(position.z);
						continue;
					}
					DirectX::XMFLOAT2& tex = tempVt[std::stoi(vertexInfo[1])]; //vt
					DirectX::XMFLOAT3& normal = tempVn[std::stoi(vertexInfo[2])]; //vn
					currentMeshData.vertices.push_back(Vertex(position, normal, DirectX::XMFLOAT3(), tex));
					currentMeshData.indices.push_back(currentMeshData.vertices.size() - 1);
					//Mark this vertex
					position.x = D3D12_MAX_POSITION_VALUE;
					position.z = std::stof(vertexInfo[0]);
				}
			}
		}
	}
	OutputDebugString(std::to_wstring(v).c_str());
	OutputDebugString(std::to_wstring(vt).c_str());
	OutputDebugString(std::to_wstring(vn).c_str());

	objFile.close();
}

void GeometryGenerator::ReadObjFileInOne(std::string filename, GeometryGenerator::MeshData& storage)
{
	std::ifstream objFile;
	objFile.open(filename);
	std::string line;

	MeshData& meshData = storage;
	meshData.name = "obj";
	int currentVertexIndex = 0;
	int v = 0, vt = 0, vn = 0;

	std::unordered_set<int> completionSet;
	std::vector<DirectX::XMFLOAT2> tempVt;
	std::vector<DirectX::XMFLOAT3> tempVn;

	tempVt.push_back(DirectX::XMFLOAT2());
	tempVn.push_back(DirectX::XMFLOAT3());

	while (std::getline(objFile, line))
	{
		std::vector<std::string> lineParts = SplitString(line, ' ');
		if (lineParts.size() > 0)
		{

			if (lineParts[0] == "v")
			{
				auto& vertex = Vertex();
				vertex.position = DirectX::XMFLOAT3(std::stof(lineParts[1]), std::stof(lineParts[2]), std::stof(lineParts[3]));
				meshData.vertices.push_back(vertex);
				++v;
			}
			if (lineParts[0] == "vt")
			{
				tempVt.push_back(DirectX::XMFLOAT2(
					std::stof(lineParts[1]),
					std::stof(lineParts[2])
				));
				++vt;
			}
			if (lineParts[0] == "vn")
			{
				tempVn.push_back(DirectX::XMFLOAT3(
					std::stof(lineParts[1]),
					std::stof(lineParts[2]),
					std::stof(lineParts[3])
				));
				++vn;
			}
			if (lineParts[0] == "f")
			{
				for (int i = 1; i < lineParts.size(); ++i)
				{
					auto vertexInfo = SplitString(lineParts[i], '/');
					int vertexIndex = std::stoi(vertexInfo[0]) - 1;
					if (completionSet.find(vertexIndex) == completionSet.end())
					{
						Vertex& vertex = meshData.vertices[vertexIndex]; //v
						vertex.texCoordinate = tempVt[std::stoi(vertexInfo[1])]; //vt
						vertex.normal = tempVn[std::stoi(vertexInfo[2])]; //vn
						completionSet.insert(vertexIndex);
					}
					meshData.indices.push_back(vertexIndex);
				}
			}
		}
	}
	OutputDebugString(std::to_wstring(v).c_str());
	OutputDebugString(std::to_wstring(vt).c_str());
	OutputDebugString(std::to_wstring(vn).c_str());

	objFile.close();
}