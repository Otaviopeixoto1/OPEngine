#version 330 core

layout (location = 0) in vec3 vPos;
layout (location = 1) in vec3 vNormal;
layout (location = 2) in vec2 vTexCoord;


uniform mat4 modelMatrix;
uniform mat4 viewMatrix;
uniform mat4 projectionMatrix;

//declaration of a variables that will be outputed to the fragment shader:
out vec3 Normal; 
out vec2 TexCoord;

void main()
{
    //project position to screen
    gl_Position = projectionMatrix * viewMatrix * modelMatrix * vec4(vPos, 1.0f);

    //pass the variables to the fragment shader
    Normal = vNormal; 
    TexCoord = vTexCoord;
}    