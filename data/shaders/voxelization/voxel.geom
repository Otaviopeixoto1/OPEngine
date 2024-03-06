#version 440 core

#include "lights.glsl"

layout(triangles) in;
layout(triangle_strip, max_vertices = 3) out;

in vec4 vNormal[3];
in vec2 TexCoords[3];

flat out uint domInd;
out vec2 intTexCoords;
out vec4 shadowCoord;
out vec3 viewNormal;

layout (std140) uniform GlobalMatrices
{
    mat4 projectionMatrix;
    mat4 viewMatrix;
    mat4 inverseViewMatrix;
	
/*/
/ voxelMatrices[3]: these matrices that project the scene in three different ways: 
/ 0 = view along x axis
/ 1 = view along y axis
/ 2 = view along z axis (identity matrix for the "default" view) 
/
/ these matrices just fixed rotations and dont change with the viewer position 
/*/
	mat4 voxelMatrices[3];
	mat4 inverseVoxelMatrix;
};

layout (std140) uniform LocalMatrices
{
    mat4 modelMatrix;
    mat4 normalMatrix;
};











vec3 CalculateNormal(vec3 a, vec3 b, vec3 c) 
{
	vec3 ab = b - a;
	vec3 ac = c - a;
	return normalize(cross(ab,ac));
}
    
void main()
{          
    vec3 dir = CalculateNormal(gl_in[0].gl_Position.xyz, gl_in[1].gl_Position.xyz, gl_in[2].gl_Position.xyz);
	viewNormal = (viewMatrix * vec4(dir, 0.0f)).xyz;
	dir = abs(dir);
	

	float maxComponent = max(dir.x, max(dir.y, dir.z));
	uint ind = maxComponent == dir.x ? 0 : maxComponent == dir.y ? 1 : 2;
	domInd = ind;

	//MAKE A FOR LOOP

	gl_Position = voxelMatrices[ind] * gl_in[0].gl_Position;
	shadowCoord = lightSpaceMatrices[0] * gl_in[0].gl_Position;
	intTexCoords = TexCoords[0];
	//viewNormal = vNormal[0].xyz;
	EmitVertex();
	
	gl_Position = voxelMatrices[ind] * gl_in[1].gl_Position;
	shadowCoord = lightSpaceMatrices[0] * gl_in[1].gl_Position;
	intTexCoords = TexCoords[1];
	//viewNormal = vNormal[1].xyz;
	EmitVertex();

	gl_Position = voxelMatrices[ind] * gl_in[2].gl_Position;
	shadowCoord = lightSpaceMatrices[0] * gl_in[2].gl_Position;
	intTexCoords = TexCoords[2];
	//viewNormal = vNormal[2].xyz;
	EmitVertex();

	EndPrimitive();
}  