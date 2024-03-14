#version 440 core

#include "lights.glsl"

layout(triangles) in;
layout(triangle_strip, max_vertices = 3) out;

in vec4 vNormal[3];
in vec2 vTexCoords[3];
in vec4 vWorldPos[3];

flat out uint domInd;
out vec2 TexCoords;
out vec4 worldPosition;
out vec4 viewPosition;
out vec3 viewNormal;
out vec3 voxelTexCoord;



layout (std140) uniform GlobalMatrices
{
    mat4 projectionMatrix;
    mat4 viewMatrix;
    mat4 inverseViewMatrix;
	mat4 voxelMatrix;
	mat4 inverseVoxelMatrix;
};

layout (std140) uniform LocalMatrices
{
    mat4 modelMatrix;
    mat4 normalMatrix;
};






vec3 GetNormal(vec3 a, vec3 b, vec3 c) 
{
	vec3 ab = b - a;
	vec3 ac = c - a;
	return normalize(cross(ab,ac));
}


    
void main()
{          
    vec3 dir = GetNormal(gl_in[0].gl_Position.xyz, gl_in[1].gl_Position.xyz, gl_in[2].gl_Position.xyz);
	viewNormal = (viewMatrix * vec4(dir, 0.0f)).xyz;
	dir = abs(dir);
	
	float maxComponent = max(dir.x, max(dir.y, dir.z));
	uint ind = maxComponent == dir.x ? 0 : maxComponent == dir.y ? 1 : 2;
	domInd = ind;

	for (int i = 0; i < 3; i++)
	{
		worldPosition = vWorldPos[i];
		viewPosition = viewMatrix * worldPosition;
		TexCoords = vTexCoords[i];
		voxelTexCoord = (gl_in[i].gl_Position.xyz + vec3(1.0f)) * 0.5f;
		//viewNormal = vNormal[i].xyz;
		

		if (ind == 0) gl_Position = vec4(gl_in[i].gl_Position.zyx, 1);		
		else if (ind == 1) gl_Position = vec4(gl_in[i].gl_Position.xzy, 1);
		else if (ind == 2) gl_Position = vec4(gl_in[i].gl_Position.xyz, 1);

		EmitVertex();
	}

	EndPrimitive();
}  