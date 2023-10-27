#version 330 core


layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;

out vec2 TexCoords;
out vec3 sNormal;
out mat4 ViewMatrix;
out vec3 sFragPos; 

layout (std140) uniform Matrices
{
    mat4 projectionMatrix;
    mat4 viewMatrix;
    mat4 modelMatrix;
    mat4 normalMatrix;
};

 


void main()
{
    TexCoords = aTexCoords;    
    gl_Position = projectionMatrix * viewMatrix * modelMatrix * vec4(aPos, 1.0);
    sNormal = vec3(normalMatrix * vec4(aNormal,0.0));  
    ViewMatrix = viewMatrix;
    sFragPos = vec3(viewMatrix * modelMatrix * vec4(aPos, 1.0));
}