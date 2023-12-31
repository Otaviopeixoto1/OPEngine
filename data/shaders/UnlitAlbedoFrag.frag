#version 330 core


out vec4 FragColor;

in vec2 TexCoords;

layout (std140) uniform MaterialProperties
{
    vec4 albedoColor;
    vec4 emissiveColor;
    vec4 specular;
};

void main()
{    
    FragColor = albedoColor;
}