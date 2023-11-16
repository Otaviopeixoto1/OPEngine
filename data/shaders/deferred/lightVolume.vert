#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;

out vec2 TexCoords;
//out vec3 ViewNormal;
out vec3 ViewFragPos; 

uniform mat4 modelViewMatrix;

layout (std140) uniform GlobalMatrices
{
    mat4 projectionMatrix;
    mat4 viewMatrix;
};


 


void main()
{
    TexCoords = aTexCoords;    
    gl_Position = projectionMatrix * modelViewMatrix * vec4(aPos, 1.0);
    //ViewNormal = vec3(normalMatrix * vec4(aNormal,0.0));  
    ViewFragPos = vec3(modelViewMatrix * vec4(aPos, 1.0));
}