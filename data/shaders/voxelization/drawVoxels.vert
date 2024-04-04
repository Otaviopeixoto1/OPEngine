#version 440

layout (location = 0) in vec3 inPosition;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec3 aTangent;
layout (location = 3) in vec2 aTexCoords;
layout (location = 4) in uint inVoxelPos;

//out vec4 outPosition;
//out vec3 outNormal;
out vec4 outColor;


uniform usampler2DArray voxel2DTextures;
uniform usampler3D voxel3DData;

uniform uint mipLevel;
uniform uint voxelRes;






layout (std140) uniform GlobalMatrices
{
    mat4 projectionMatrix;
    mat4 viewMatrix;
    mat4 inverseViewMatrix;
	mat4 voxelMatrix;
	mat4 inverseVoxelMatrix;
};



struct VoxelData 
{
	vec4 color;
	//uint light;
	uint count;
};

VoxelData unpackARGB8(uint bytesIn) 
{
	VoxelData data;
	uvec3 uiColor;

	data.count = (bytesIn & 0xFF000000) >> 24;
	uiColor.r =  (bytesIn & 0x00FF0000) >> 16;
	uiColor.g =  (bytesIn & 0x0000FF00) >> 8;
	uiColor.b =  (bytesIn & 0x000000FF);

	data.color.rgb = vec3(uiColor) / float(data.count) / 31.0f;
	data.color.a = 1.0f;

	return data;
}


uvec3 unpackRG11B10(uint bytesIn) 
{
	uvec3 outVec;

	outVec.r = (bytesIn & 0xFFE00000) >> 21;
	outVec.g = (bytesIn & 0x001FFC00) >> 10;
	outVec.b = (bytesIn & 0x000003FF);

	return outVec;
}

void main(void)
{
	float size = float(voxelRes >> mipLevel);
	vec3 voxelPos = vec3(unpackRG11B10(inVoxelPos)) / size;

	uint voxelBytes = textureLod(voxel3DData, voxelPos, float(mipLevel)).r;
	VoxelData data = unpackARGB8(voxelBytes);
	outColor = data.color;

	vec3 temp = inPosition / size + 2.0f * voxelPos - vec3(1.0f);

	gl_Position = projectionMatrix * viewMatrix * inverseVoxelMatrix * vec4(temp,1.0f);
}