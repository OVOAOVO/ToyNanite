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

layout(binding=0)uniform GlobalConstants {
	mat4 mProjectionMatrix;
	mat4 mViewMatrix;//View => translate
	mat4 mModelMatrix;
	uvec4 mMisc0;//0xFFFFFFFFu
	vec4 mNanite_ViewOrigin;//x,y,z,w => lodScale
	vec4 mNanite_ViewForward;//x,y,z,w => lodScaleHW
};
layout(std430,binding=1)buffer FMainAndPostNodeAndClusterBatches{
    uint mData[];
}MainAndPostNodeAndClusterBatches;//21
layout(std430,binding=2)buffer FVisibleClusterSHWH{
    uint mData[];
}VisibleClusterSHWH;
layout(std430,binding=3)buffer FWorkArgs0{
    uint mData[];
}WorkArgs0;
uint BitFieldExtractU32(uint Data, uint Size, uint Offset)
{
	// Shift amounts are implicitly &31 in HLSL, so they should be optimized away on most platforms
	// In GLSL shift amounts < 0 or >= word_size are undefined, so we better be explicit
	Size &= 31;
	Offset &= 31;
	return (Data >> Offset) & ((1u << Size) - 1u);
}
void main(){//
	for(int i=0;i<932;i++){
		VisibleClusterSHWH.mData[1024+i*2]=MainAndPostNodeAndClusterBatches.mData[1024+i*2];
		VisibleClusterSHWH.mData[1024+i*2+1]=MainAndPostNodeAndClusterBatches.mData[1024+i*2+1];
	}
}