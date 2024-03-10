#version 440 core

#include "lights.glsl"

layout(triangles, invocations = SHADOW_CASCADE_COUNT) in;
layout(triangle_strip, max_vertices = 3) out;

out vec4 lightSpacePos;

void main()
{          
    for (int i = 0; i < 3; ++i)
    {
        gl_Position = lightSpaceMatrices[gl_InvocationID] * gl_in[i].gl_Position;
        lightSpacePos = gl_Position;
        gl_Layer = gl_InvocationID;
        EmitVertex();
    }
    EndPrimitive();
}  