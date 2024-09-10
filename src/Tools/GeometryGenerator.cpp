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
	IndicesGroup group;
	uint32_t ringVertexCount = slice + 1;
	for (uint32_t i = 0; i < stack; ++i)
	{

		for (uint32_t j = 0; j < slice; ++j)
		{
			//ABC
			group.indices.push_back(i * ringVertexCount + j);
			group.indices.push_back((i+1) * ringVertexCount + j);
			group.indices.push_back((i + 1) * ringVertexCount + j + 1);

			//ACD
			group.indices.push_back(i * ringVertexCount + j);
			group.indices.push_back((i + 1) * ringVertexCount + j + 1);
			group.indices.push_back(i * ringVertexCount + j + 1);
		}
	}
	group.mtlName = "default";
	meshData.idxGroups.push_back(group);

	return meshData;
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
	v.push_back(Vertex(-l2, +h2, -w2, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f));
	v.push_back(Vertex(-l2, -h2, -w2, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f));
	v.push_back(Vertex(+l2, -h2, -w2, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f));
	v.push_back(Vertex(+l2, +h2, -w2, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f));
	//back			
	v.push_back(Vertex(+l2, +h2, +w2, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f));
	v.push_back(Vertex(+l2, -h2, +w2, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f));
	v.push_back(Vertex(-l2, -h2, +w2, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f));
	v.push_back(Vertex(-l2, +h2, +w2, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f));
	//top		
	v.push_back(Vertex(-l2, +h2, +w2, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f));
	v.push_back(Vertex(-l2, +h2, -w2, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f));
	v.push_back(Vertex(+l2, +h2, -w2, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f));
	v.push_back(Vertex(+l2, +h2, +w2, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f));
	//Bottom
	v.push_back(Vertex(-l2, -h2, -w2, 0.0f, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f));
	v.push_back(Vertex(-l2, -h2, +w2, 0.0f, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f));
	v.push_back(Vertex(+l2, -h2, +w2, 0.0f, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f));
	v.push_back(Vertex(+l2, -h2, -w2, 0.0f, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f));
	//Left
	v.push_back(Vertex(-l2, +h2, +w2, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f));
	v.push_back(Vertex(-l2, -h2, +w2, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f));
	v.push_back(Vertex(-l2, -h2, -w2, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f));
	v.push_back(Vertex(-l2, +h2, -w2, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f));
	//Right
	v.push_back(Vertex(+l2, +h2, -w2, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f));
	v.push_back(Vertex(+l2, -h2, -w2, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f));
	v.push_back(Vertex(+l2, -h2, +w2, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f));
	v.push_back(Vertex(+l2, +h2, +w2, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f));

	IndicesGroup group;
	//indices
	for (uint32_t i = 0; i < 24; i+=4)
	{
		group.indices.push_back(i + 1);
		group.indices.push_back(i);
		group.indices.push_back(i + 3);

		group.indices.push_back(i + 1);
		group.indices.push_back(i + 3);
		group.indices.push_back(i + 2);
		
	}
	group.mtlName = "default";
	meshData.idxGroups.push_back(group);
	return meshData;
}
GeometryGenerator::MeshData GeometryGenerator::BuildGrid(float width, float depth, uint32_t m, uint32_t n)
{
	GeometryGenerator::MeshData meshData;
	meshData.name = "grid";
	uint32_t vertexCount = m * n;
	uint32_t faceCount = (m - 1) * (n - 1) * 2;
	float halfWidth = 0.5f * width;
	float halfDepth = 0.5f * depth;
	float dx = width / (n - 1);

	float dz = depth / (m - 1);
	float du = 1.0f / (n - 1);
	float dv = 1.0f / (m - 1);

	for (uint32_t i = 0; i < m; ++i)
	{
		float z = halfDepth - i * dz;
		for (uint32_t j = 0; j < n; ++j)
		{
			float x = -halfWidth + j * dx;
			Vertex v;
			v.position = DirectX::XMFLOAT3(x, 0.0f, z);
			v.normal = DirectX::XMFLOAT3(0.0f, 1.0f, 0.0f);
			v.tangentU = DirectX::XMFLOAT3(1.0f, 0.0f, 0.0f);
			// Stretch texture over grid.
			v.texCoordinate.x = j * du;
			v.texCoordinate.y = i * dv;
			meshData.vertices.push_back(v);
		}
	}
	IndicesGroup group;
	// 对于线框绘制，索引缓冲区需要明确定义每条边。与三角形面不同，线框需要显式定义每条边
	//Here is a triangle index list
	for (uint32_t i = 0; i < m - 1; ++i)
	{
		for (uint32_t j = 0; j < n - 1; ++j)
		{
			group.indices.push_back(i * n + j);
			group.indices.push_back(i * n + j + 1);
			group.indices.push_back((i + 1) * n + j);

			group.indices.push_back((i + 1) * n + j);
			group.indices.push_back(i * n + j + 1);
			group.indices.push_back((i + 1) * n + j + 1);

		}
	}
	group.mtlName = "default";
	meshData.idxGroups.push_back(group);
	return meshData;
}
void GeometryGenerator::ReadObjFile(std::string path, std::string fileName, std::vector<GeometryGenerator::MeshData>& storage, std::vector<MaterialLoader::Material>& mtlList)
{
	std::ifstream objFile;
	objFile.open(path + "\\" + fileName);
	std::string line;

	auto& meshDataGroup = storage;

	MeshData currentMeshData;
	IndicesGroup currentIdxGroup;
	bool firstMesh = true;
	bool gSign = false;

	std::string meshName;

	int currentVertexIndex = 0;

	std::vector<DirectX::XMFLOAT3> tempV;
	std::vector<DirectX::XMFLOAT2> tempVt;
	std::vector<DirectX::XMFLOAT3> tempVn;

	std::unordered_map<Vertex, int> vertexIndexMap;

	while (std::getline(objFile, line))
	{
		std::vector<std::string> lineParts = SplitString(line, ' ');
		if (lineParts.size() > 0)
		{
			if (lineParts[0] == "mtllib")
			{
				MaterialLoader mtlLoader;
				mtlLoader.ReadMtlFile(path, lineParts[1], mtlList);
			}
			//MeshData Triangle Indices and corresponding face Matrial
			if (lineParts[0] == "g")
			{
				gSign = true;
				meshName = lineParts[1] != "group" ? lineParts[1] : lineParts[2];
			}
			if (lineParts[0] == "usemtl")
			{
				//Do not push first mesh into groups now. The indices currentIdxGroup are empty now.
				if (firstMesh)
					firstMesh = false;
				else
				{
					//Next indices group coming. Save present.
					currentMeshData.idxGroups.push_back(currentIdxGroup);
					if (gSign)
					{
						//Next mesh data coming. Save
						meshDataGroup.push_back(currentMeshData);
						currentMeshData = MeshData();
						//Wait for next mesh indices(g and usemtl)
						gSign = false;
					}
					//New Idx group
					currentIdxGroup = IndicesGroup();
				}
				//Fill in names
				currentMeshData.name = meshName;
				currentIdxGroup.mtlName = lineParts[1];
			}
			if (lineParts[0] == "f")
			{
				for (int i = 1; i < lineParts.size(); ++i)
				{
					auto vertexInfo = SplitString(lineParts[i], '/');
					int vertexIndex = std::stoi(vertexInfo[0]) - 1;//Index in file starts from 1
					DirectX::XMFLOAT3& position = tempV[vertexIndex]; //v
					DirectX::XMFLOAT2& tex = tempVt[std::stoi(vertexInfo[1]) - 1]; //vt
					DirectX::XMFLOAT3& normal = tempVn[std::stoi(vertexInfo[2]) - 1]; //vn

					Vertex v(position, normal, DirectX::XMFLOAT3(), tex);
					if (vertexIndexMap.find(v) == vertexIndexMap.end())
					{
						currentMeshData.vertices.push_back(v);
						int indexInCurrentMesh = currentMeshData.vertices.size() - 1;
						currentIdxGroup.indices.push_back(indexInCurrentMesh);
						vertexIndexMap.emplace(v, indexInCurrentMesh);
					}
					else
					{
						currentIdxGroup.indices.push_back(vertexIndexMap[v]);
					}
				}
			}
			//MeshData Vertices
			if (lineParts[0] == "v")
			{
				tempV.push_back(DirectX::XMFLOAT3(
					std::stof(lineParts[1]),
					std::stof(lineParts[2]),
					std::stof(lineParts[3])
				));
			}
			if (lineParts[0] == "vt")
			{
				tempVt.push_back(DirectX::XMFLOAT2(
					std::stof(lineParts[1]),
					std::stof(lineParts[2])
				));
			}
			if (lineParts[0] == "vn")
			{
				tempVn.push_back(DirectX::XMFLOAT3(
					std::stof(lineParts[1]),
					std::stof(lineParts[2]),
					std::stof(lineParts[3])
				));
			}
		}
	}
	currentMeshData.idxGroups.push_back(currentIdxGroup);
	meshDataGroup.push_back(currentMeshData); //last one

	objFile.close();
}
void GeometryGenerator::ReadObjFileInOne(std::string path, std::string fileName, GeometryGenerator::MeshData& storage)
{
	std::ifstream objFile;
	objFile.open(path + "\\" + fileName);
	std::string line;

	MeshData& meshData = storage;
	meshData.name = fileName;
	IndicesGroup group;
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
					int vertexIndex = std::stoi(vertexInfo[0]) - 1; //index STARTS from 1 in obj
					if (completionSet.find(vertexIndex) == completionSet.end())
					{
						Vertex& vertex = meshData.vertices[vertexIndex]; //v
						vertex.texCoordinate = tempVt[std::stoi(vertexInfo[1])]; //vt
						vertex.normal = tempVn[std::stoi(vertexInfo[2])]; //vn
						completionSet.insert(vertexIndex);
					}
					group.indices.push_back(vertexIndex);
				}
			}
		}
	}
	meshData.idxGroups.push_back(group);

	OutputDebugString(std::to_wstring(v).c_str());
	OutputDebugString(std::to_wstring(vt).c_str());
	OutputDebugString(std::to_wstring(vn).c_str());

	objFile.close();
}