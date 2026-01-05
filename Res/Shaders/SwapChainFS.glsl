#version 450

layout(location = 0)in vec4 V_Texcoord;
layout(location = 0)out vec4 RT0;

layout(binding = 2)uniform sampler2D U_Texture;

void main()
{
    vec3 color = texture(U_Texture,V_Texcoord.xy).rgb;
    RT0 = vec4(color, 1.f);
}