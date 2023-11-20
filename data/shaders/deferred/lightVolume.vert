#version 330 core

layout (location = 0) in vec3 aPos;

uniform mat4 modelViewMatrix;

layout (std140) uniform GlobalMatrices
{
    mat4 projectionMatrix;
    mat4 viewMatrix;
};


 


void main()
{    
    gl_Position = projectionMatrix * modelViewMatrix * vec4(aPos, 1.0);

}