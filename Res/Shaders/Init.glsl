#version 450
#extension GL_ARB_gpu_shader_int64 : enable

layout(local_size_x=1,local_size_y=1,local_size_z=1)in;

layout(std430,binding=0)buffer FWorkArgs0{
    uint mData[];
}WorkArgs0;
layout(std430,binding=1)buffer FWorkArgs1{
    uint mData[];
}WorkArgs1;
layout(std430,binding=2)buffer FMainAndPostNodeAndClusterBatches{
    uint mData[];
}MainAndPostNodeAndClusterBatches;
layout(std430,binding=3)buffer FVisBuffer64{
    uint64_t mData[];
}VisBuffer64;
void main(){
	WorkArgs0.mData[5]=0u;
	WorkArgs0.mData[6]=1u;
	WorkArgs0.mData[7]=0u;
	WorkArgs1.mData[5]=0u;
	WorkArgs1.mData[6]=1u;
	WorkArgs1.mData[7]=0u;
	for(int x = 0; x < 1280; x++)
	{
		for(int y = 0; y < 720 ; y++)
		{
			int pixelIndex = y*1280+x;
			VisBuffer64.mData[pixelIndex] = pixelIndex;
		}
	}
}