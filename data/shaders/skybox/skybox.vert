
#version 440 core
layout (location = 0) in vec3 aPos;

out vec3 TexCoords;

uniform mat4 VPMatrix;

void main()
{
    TexCoords = aPos;
    vec4 pos = VPMatrix * vec4( aPos, 1.0);
    gl_Position = pos.xyww;
}  