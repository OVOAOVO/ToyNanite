#version 450

#define HIERARCHY_NODE_SLICE_SIZE	((4 + 4 + 4 + 1) * 4 * NANITE_MAX_BVH_NODE_FANOUT)
#define NANITE_MAX_BVH_NODE_FANOUT_BITS						2
#define NANITE_MAX_BVH_NODE_FANOUT							(1 << NANITE_MAX_BVH_NODE_FANOUT_BITS)

#define NANITE_MAX_CLUSTERS_PER_GROUP_BITS					9
#define NANITE_MAX_CLUSTERS_PER_GROUP						((1 << NANITE_MAX_CLUSTERS_PER_GROUP_BITS) - 1)

#define NANITE_MAX_GROUP_PARTS_BITS							5
#define NANITE_MAX_GROUP_PARTS_MASK							((1 << NANITE_MAX_GROUP_PARTS_BITS) - 1)
#define NANITE_MAX_GROUP_PARTS								(1 << NANITE_MAX_GROUP_PARTS_BITS)

#define NANITE_MAX_RESOURCE_PAGES_BITS						16 // 2GB of 32kb root pages or 4GB of 64kb streaming pages

layout(local_size_x=1,local_size_y=1,local_size_z=1)in;
layout(std430,binding=0)buffer FBVHBuffer{
    uint mData[];
}BVHBuffer;
layout(std430,binding=0)buffer FEchoBuffer{
    uint mData[];
}EchoBuffer;

uint BitFieldExtractU32(uint Data, uint Size, uint Offset)
{
	// Shift amounts are implicitly &31 in HLSL, so they should be optimized away on most platforms
	// In GLSL shift amounts < 0 or >= word_size are undefined, so we better be explicit
	Size &= 31;
	Offset &= 31;
	return (Data >> Offset) & ((1u << Size) - 1u);
}

struct FHierarchyNodeSlice
{
	vec4	LODBounds;
	vec3	BoxBoundsCenter;
	vec3	BoxBoundsExtent;
	float   MinLODError;
	float   MaxParentLODError;
	uint	ChildStartReference;	// Can be node (index) or cluster (page:cluster)
	uint	NumChildren;
	uint	StartPageIndex;
	uint	NumPages;
	bool	bEnabled;
	bool	bLoaded;
	bool	bLeaf;
};


FHierarchyNodeSlice UnpackHierarchyNodeSlice(uvec4 RawData0, uvec4 RawData1, uvec4 RawData2, uint RawData3)
{
	const uvec4 Misc0 = RawData1;
	const uvec4 Misc1 = RawData2;
	const uint  Misc2 = RawData3;

	FHierarchyNodeSlice Node;
	Node.LODBounds				= uintBitsToFloat(RawData0);

	Node.BoxBoundsCenter		= uintBitsToFloat(Misc0.xyz);
	Node.BoxBoundsExtent		= uintBitsToFloat(Misc1.xyz);

    vec2 unpacked2Half          = unpackHalf2x16(Misc0.w);

	Node.MinLODError			= unpacked2Half.x;
	Node.MaxParentLODError		= unpacked2Half.y;
	Node.ChildStartReference	= Misc1.w;						// When changing this, remember to also update StoreHierarchyNodeChildStartReference
	Node.bLoaded				= (Misc1.w != 0xFFFFFFFFu);

	Node.NumChildren			= BitFieldExtractU32(Misc2, NANITE_MAX_CLUSTERS_PER_GROUP_BITS, 0);
	Node.NumPages				= BitFieldExtractU32(Misc2, NANITE_MAX_GROUP_PARTS_BITS, NANITE_MAX_CLUSTERS_PER_GROUP_BITS);
	Node.StartPageIndex			= BitFieldExtractU32(Misc2, NANITE_MAX_RESOURCE_PAGES_BITS, NANITE_MAX_CLUSTERS_PER_GROUP_BITS + NANITE_MAX_GROUP_PARTS_BITS);
	Node.bEnabled				= Misc2 != 0u;
	Node.bLeaf					= Misc2 != 0xFFFFFFFFu;

	return Node;
}

//BVH Node 208 -> 4 Child * 52 -> 4 child * uint(4) * 13 
FHierarchyNodeSlice GetHierarchyNodeSlice(uint NodeIndex, uint ChildIndex)
{
	const uint BaseAddress	= (NodeIndex * HIERARCHY_NODE_SLICE_SIZE)/4;

    uint i = BaseAddress + 4 * ChildIndex;
	const uvec4 RawData0	= uvec4(BVHBuffer.mData[i], BVHBuffer.mData[i+1], BVHBuffer.mData[i+2], BVHBuffer.mData[i+3]);
    i = BaseAddress + 16 + 4 * ChildIndex;
	const uvec4 RawData1	= uvec4(BVHBuffer.mData[i], BVHBuffer.mData[i+1], BVHBuffer.mData[i+2], BVHBuffer.mData[i+3]);
    i = BaseAddress + 32 + 4 * ChildIndex;
    const uvec4 RawData2	= uvec4(BVHBuffer.mData[i], BVHBuffer.mData[i+1], BVHBuffer.mData[i+2], BVHBuffer.mData[i+3]);
    i = BaseAddress + 48 + ChildIndex;
	const uint RawData3	= BVHBuffer.mData[i];

	return UnpackHierarchyNodeSlice(RawData0, RawData1, RawData2, RawData3);
}

void main()
{
    uint offset = 0u;
    for(int i = 0; i < 21; i++) // BVHBuffer Node Count
    {
        for(int j = 0;j < 4;j++) // branch tree 4
        {
            FHierarchyNodeSlice node = GetHierarchyNodeSlice(i,j);
            EchoBuffer.mData[offset] = floatBitsToUint(float(i));
            offset++;
            EchoBuffer.mData[offset] = floatBitsToUint(float(j));
            offset++;
            EchoBuffer.mData[offset] = floatBitsToUint(node.LODBounds.x);
            offset++;
            EchoBuffer.mData[offset] = floatBitsToUint(node.BoxBoundsCenter.x);
            offset++;
            EchoBuffer.mData[offset] = floatBitsToUint(float(i));
            offset++;
            EchoBuffer.mData[offset] = floatBitsToUint(float(j));
            offset++;
            EchoBuffer.mData[offset] = floatBitsToUint(node.BoxBoundsExtent.x);
            offset++;
            EchoBuffer.mData[offset] = node.bLeaf?floatBitsToUint(1.0f):floatBitsToUint(0.0f);
            offset++;
        }
    }
}