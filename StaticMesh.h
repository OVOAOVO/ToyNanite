#pragma once
#include "Vulkan.h"
#include <vector>
#include <string>
#include "float4.h"
#include "Material.h"
#include <unordered_map>

struct StaticMeshVertexData {
	float4 mPosition;
	float4 mTexcoord;
	float4 mNormal;
	float4 mTangent;
};

struct SubMesh {
	unsigned int* mIndexes;
	int mIndexCount;
	Buffer* mIBO;
};
class StaticMesh{
public:
	static std::vector<VkVertexInputBindingDescription> mVertexInputBindingDescriptions;
	static std::vector<VkVertexInputAttributeDescription> mVertexInputAttributeDescriptions;
	static void Init();
	Material mMaterial;
	StaticMeshVertexData* mVertexData;
	int mVertexCount;
	Buffer* mVBO;
	std::unordered_map<std::string, SubMesh*> mSubMeshes;
	void SetVertexCount(int inVertexCount);
	void SetPosition(int inIndex, float inX, float inY, float inZ, float inW = 1.0f);
	void SetTexcoord(int inIndex, float inX, float inY, float inZ=0.0f, float inW = 0.0f);
	void SetNormal(int inIndex, float inX, float inY, float inZ, float inW = 0.0f);
	void SetTangent(int inIndex, float inX, float inY, float inZ, float inW = 0.0f);
	void Draw(VkCommandBuffer inCommandBuffer);
	void InitFromFile(const char* inFilePath);
};