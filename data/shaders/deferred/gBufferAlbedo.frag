#version 330 core

layout (location = 0) out vec4 gAlbedoSpec;
layout (location = 1) out vec4 gNormal;
layout (location = 2) out vec4 gPosition;

in vec2 TexCoords;
in vec3 ViewFragPos;
in vec3 ViewNormal;

layout (std140) uniform MaterialProperties
{
    vec4 albedoColor;
    vec4 emissiveColor;
    vec4 specular;
};



void main()
{    
    gPosition = vec4(ViewFragPos,1.0);
    gNormal = vec4(normalize(ViewNormal),0.0);


    gAlbedoSpec.rgb = albedoColor.rgb;
    gAlbedoSpec.a = specular.a;
}  