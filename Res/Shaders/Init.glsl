#version 450
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
void main(){
	WorkArgs0.mData[5]=0u;
	WorkArgs0.mData[6]=1u;
	WorkArgs1.mData[5]=0u;
	WorkArgs1.mData[6]=1u;
	for(int i=0;i<128;i++){
		MainAndPostNodeAndClusterBatches.mData[i]=0;
	}
}