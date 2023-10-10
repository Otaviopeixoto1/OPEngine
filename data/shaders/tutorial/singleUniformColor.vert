#version 330 core

layout (location = 0) in vec3 vPos;
layout (location = 1) in vec3 vNormal;
layout (location = 2) in vec2 vTexCoord;

//declaration of a variables that will be outputed to the fragment shader:
out vec3 Normal; 
out vec2 TexCoord;

void main()
{
    gl_Position = vec4(vPos.x, vPos.y, vPos.z, 1.0);
    Normal = vNormal; //pass the normal to the fragment shader
    TexCoord = vTexCoord;
}    