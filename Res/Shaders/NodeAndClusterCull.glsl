#version 450
layout(local_size_x=1,local_size_y=1,local_size_z=1)in;
#define NANITE_MAX_GROUP_PARTS_BITS							5
#define NANITE_MAX_GROUP_PARTS_MASK							((1 << NANITE_MAX_GROUP_PARTS_BITS) - 1)
#define NANITE_MAX_GROUP_PARTS								(1 << NANITE_MAX_GROUP_PARTS_BITS)

#define NANITE_MAX_RESOURCE_PAGES_BITS						16

#define NANITE_MAX_CLUSTERS_PER_GROUP_BITS					9
#define NANITE_MAX_CLUSTERS_PER_GROUP						((1 << NANITE_MAX_CLUSTERS_PER_GROUP_BITS) - 1)

#define NANITE_MAX_BVH_NODE_FANOUT_BITS						2
#define NANITE_MAX_BVH_NODE_FANOUT							(1 << NANITE_MAX_BVH_NODE_FANOUT_BITS)
#define HIERARCHY_NODE_SLICE_SIZE	((4 + 4 + 4 + 1) * 4 * NANITE_MAX_BVH_NODE_FANOUT)

layout(std430,binding=0)buffer FBVHBuffer{
    uint mData[];
}BVHBuffer;//21
layout(std430,binding=1)buffer FEchoBuffer{
    uint mData[];
}EchoBuffer;
layout(std430,binding=2)buffer FMainAndPostNodeAndClusterBatches{
    uint mData[];
}MainAndPostNodeAndClusterBatches;
layout(std430,binding=3)buffer FCurrentWorkArgs{
    uint mData[];
}CurrentWorkArgs;
layout(std430,binding=4)buffer FNextWorkArgs{
    uint mData[];
}NextWorkArgs;
layout(binding=5)uniform GlobalConstants {
	mat4 mProjectionMatrix;
	mat4 mViewMatrix;//View => translate
	mat4 mModelMatrix;
	uvec4 mMisc0;//0xFFFFFFFFu,x:Manual MipLevel
	vec4 mNanite_ViewOrigin;//x,y,z,w => lodScale
	vec4 mNanite_ViewForward;//x,y,z,w => lodScaleHW
} U_GlobalConstants;
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
	float	MinLODError;
	float	MaxParentLODError;
	uint	ChildStartReference;
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

	Node.MinLODError			= unpacked2Half.x;//little endian
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
//bvh node 208 => 4 child x 52 => 4 child x uint x 13
FHierarchyNodeSlice GetHierarchyNodeSlice(uint NodeIndex, uint ChildIndex)
{
	const uint BaseAddress	= (NodeIndex * HIERARCHY_NODE_SLICE_SIZE)/4;
    uint i=BaseAddress+4*ChildIndex;
	const uvec4 RawData0	= uvec4(BVHBuffer.mData[i],BVHBuffer.mData[i+1],BVHBuffer.mData[i+2],BVHBuffer.mData[i+3]);
	i=BaseAddress+16+ChildIndex*4;
    const uvec4 RawData1	= uvec4(BVHBuffer.mData[i],BVHBuffer.mData[i+1],BVHBuffer.mData[i+2],BVHBuffer.mData[i+3]);
	i=BaseAddress+32+ChildIndex*4;
    const uvec4 RawData2	= uvec4(BVHBuffer.mData[i],BVHBuffer.mData[i+1],BVHBuffer.mData[i+2],BVHBuffer.mData[i+3]);
	i=BaseAddress+48+ChildIndex;
    const uint  RawData3	= BVHBuffer.mData[i];
	
	return UnpackHierarchyNodeSlice(RawData0, RawData1, RawData2, RawData3);
}
vec2 GetProjectedEdgeScales(vec4 Bounds)
{
	if(U_GlobalConstants.mProjectionMatrix[3][3] >= 1.0f )
	{
		return vec2(1.0f, 1.0f);
	}
	vec3 Center = Bounds.xyz;
	float Radius = Bounds.w;

	float ZNear = 10.0f;
	float DistToClusterSq = dot(Center,Center);	// camera origin in (0,0,0)
	
	float Z = dot(U_GlobalConstants.mNanite_ViewForward.xyz, Center);
	float XSq = DistToClusterSq - Z * Z;
	float X = sqrt( max(0.0f, XSq) );
	float DistToTSq = DistToClusterSq - Radius * Radius;
	float DistToT = sqrt( max(0.0f, DistToTSq) );
	float ScaledCosTheta = DistToT;
	float ScaledSinTheta = Radius;
	float ScaleToUnit = 1.0f/ DistToClusterSq;
	float By = (  ScaledSinTheta * X + ScaledCosTheta * Z ) * ScaleToUnit;
	float Ty = ( -ScaledSinTheta * X + ScaledCosTheta * Z ) * ScaleToUnit;
	
	float MinZ = max( Z - Radius, ZNear );
	float MaxZ = max( Z + Radius, ZNear );
	float MinCosAngle = Ty;
	float MaxCosAngle = By;

	if(Z + Radius > ZNear)
		return vec2( MinZ * MinCosAngle, MaxZ * MaxCosAngle );
	else
		return vec2( 0.0f, 0.0f );
}
bool ShouldVisitChild(FHierarchyNodeSlice inHierarchyNodeSlice){
	vec4 boundingSphereWS=inHierarchyNodeSlice.LODBounds;
	vec4 positionWS=U_GlobalConstants.mModelMatrix*vec4(boundingSphereWS.xyz,1.0f);
	boundingSphereWS.xyz=positionWS.xyz-U_GlobalConstants.mNanite_ViewOrigin.xyz;
	float lodScale=U_GlobalConstants.mNanite_ViewOrigin.w;
	vec2 ProjectedEdgeScales = GetProjectedEdgeScales(boundingSphereWS);
	float Threshold = lodScale * inHierarchyNodeSlice.MaxParentLODError;
	if( ProjectedEdgeScales.x <= Threshold ){
		return true;
	}
	return false;
}
void main(){//
	//uint uint uint uint uint | => 
	uint nodeOffset=CurrentWorkArgs.mData[5];//index in MainAndPostNodeAndClusterBatches 0 1
	uint nodeCount=CurrentWorkArgs.mData[6];//1 4

	uint nextNodeOffsetInBuffer=nodeOffset+nodeCount;//index in MainAndPostNodeAndClusterBatches,1 5
	uint nodeOutputOffset=nextNodeOffsetInBuffer;//1 5
	uint nextNodeCount=0;//next level bvh node count;

	uint clusterOutputOffset=CurrentWorkArgs.mData[1];
	for(int nodeIndexOffset=0;nodeIndexOffset<nodeCount;nodeIndexOffset++){
		uint currentNodeIndex=MainAndPostNodeAndClusterBatches.mData[nodeOffset+nodeIndexOffset];//1
		for(int i=0;i<4;i++){
			FHierarchyNodeSlice slice=GetHierarchyNodeSlice(currentNodeIndex,i);
			uint currentSliceMipLevel=slice.NumPages;
			if(slice.bEnabled){
				bool bShouldVisitChild=ShouldVisitChild(slice);
				if(false==slice.bLeaf){
					MainAndPostNodeAndClusterBatches.mData[nodeOutputOffset]=slice.ChildStartReference;
					nodeOutputOffset++;
					nextNodeCount++;
				}else{
					//if(currentSliceMipLevel==U_GlobalConstants.mMisc0.x)
					if(bShouldVisitChild)
					{
						uint clusterCountInLeafNode=slice.NumChildren;//
						uint pageIndex=slice.ChildStartReference>>8;
						uint clusterOffsetInPage=slice.ChildStartReference & 0xFFu;
						for(uint i=0u;i<clusterCountInLeafNode;i++){
							MainAndPostNodeAndClusterBatches.mData[1024+clusterOutputOffset*2]=pageIndex;
							MainAndPostNodeAndClusterBatches.mData[1024+clusterOutputOffset*2+1]=clusterOffsetInPage+i;
							clusterOutputOffset++;
						}
					}
				}
			}
		}
	}
	NextWorkArgs.mData[5]=nextNodeOffsetInBuffer;
	NextWorkArgs.mData[6]=nextNodeCount;
	NextWorkArgs.mData[1]=clusterOutputOffset;
}