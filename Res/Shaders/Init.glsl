#version 450
layout(local_size_x=1,local_size_y=1,local_size_z=1)in;
layout(std430,binding=0)buffer FBVHBuffer{
    uint mData[];
}BVHBuffer;
layout(std430,binding=0)buffer FEchoBuffer{
    uint mData[];
}EchoBuffer;
void main()
{
    EchoBuffer.mData[0]=BVHBuffer.mData[0];
    
}