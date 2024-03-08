#version 440 core
layout (location = 0) in vec3 aPos;
    
uniform mat4 modelMatrix;
    
void main()
{
    gl_Position = modelMatrix * vec4(aPos, 1.0);
}