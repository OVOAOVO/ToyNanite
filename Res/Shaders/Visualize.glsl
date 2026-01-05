#version 450
#extension GL_ARB_gpu_shader_int64 : enable
layout(local_size_x=8,local_size_y=8,local_size_z=1)in;

layout(std430,binding=0)buffer FVisBuffer64{
    uint64_t mData[];
}VisBuffer64;
layout(binding=1, rgba32f)uniform image2D VisualizeTexture;

uint MurmurMix(uint Hash)
{
	Hash ^= Hash >> 16;
	Hash *= 0x85ebca6b;
	Hash ^= Hash >> 13;
	Hash *= 0xc2b2ae35;
	Hash ^= Hash >> 16;
	return Hash;
}

vec3 IntToColor(uint Index)
{
	uint Hash = MurmurMix(Index);

	vec3 Color = vec3
	(
		(Hash >>  0) & 255,
		(Hash >>  8) & 255,
		(Hash >> 16) & 255
	);

	return Color * (1.0f / 255.0f);
}

void main(){
	ivec2 texcoord = ivec2(gl_GlobalInvocationID.xy);
	if(any(greaterThanEqual(texcoord, ivec2(1280,720))))
	{
		return;
	}

    int pixelIndex = texcoord.y*1280+texcoord.x;
    uint64_t pixelValue = VisBuffer64.mData[pixelIndex];// 32(depth) | 32( pageIndex | ClusterIndex)
	uint packedClusterInfo = uint(pixelValue);
	vec3 color = vec3(0.f, 0.f, 0.f);
	if(packedClusterInfo > 0)
	{
		uint pageIndex = packedClusterInfo >> 8;
		uint clusterIndex = (packedClusterInfo & 0xFFu)-1;

		color = IntToColor(clusterIndex);
		color = color * 0.8 + 0.2;
	}
    imageStore(VisualizeTexture, texcoord, vec4(color, 1.f));

}