#version 330 core

in vec2 TexCoords;
in vec3 ViewFragPos;
//in vec3 ViewNormal;

out vec4 FragColor;

//layout (std140) uniform MaterialProperties
//{
//    vec4 albedoColor;
//    vec4 emissiveColor;
//    vec4 specular;
//};



void main()
{    
    FragColor = vec4(0.5,0,0,1);
}  