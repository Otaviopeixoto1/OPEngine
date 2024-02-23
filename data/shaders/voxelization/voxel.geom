#version 440 core

#include "lights.glsl"

layout(triangles) in;
layout(triangle_strip, max_vertices = 3) out;


in vec2 TexCoords[3];

flat out uint domInd;
out vec2 intTexCoords;
out vec4 shadowCoord;
out vec3 exNormal;

layout (std140) uniform GlobalMatrices
{
    mat4 projectionMatrix;
    mat4 viewMatrix;
    mat4 inverseViewMatrix;
	
/*/
/ SceneMatrices[3]: these matrices that project the scene in three different ways: 
/ 0 = view along x axis
/ 1 = view along y axis
/ 2 = view along z axis (identity matrix for the "default" view) 
/
/ these matrices just fixed rotations and dont change with the viewer position 
/*/
	mat4 SceneMatrices[3];
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
	exNormal = dir;
	dir = abs(dir);
	float maxComponent = max(dir.x, max(dir.y, dir.z));
	uint ind = maxComponent == dir.x ? 0 : maxComponent == dir.y ? 1 : 2;
	domInd = ind;

	gl_Position = SceneMatrices[ind] * gl_in[0].gl_Position;
	shadowCoord = lightSpaceMatrices[0] * gl_in[0].gl_Position;
	intTexCoords = TexCoords[0];
	EmitVertex();
	
	gl_Position = SceneMatrices[ind] * gl_in[1].gl_Position;
	shadowCoord = lightSpaceMatrices[0] * gl_in[1].gl_Position;
	intTexCoords = TexCoords[1];
	EmitVertex();

	gl_Position = SceneMatrices[ind] * gl_in[2].gl_Position;
	shadowCoord = lightSpaceMatrices[0] * gl_in[2].gl_Position;
	intTexCoords = TexCoords[2];
	EmitVertex();

	EndPrimitive();
}  