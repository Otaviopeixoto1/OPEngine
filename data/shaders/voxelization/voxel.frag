#version 440


uniform sampler2D texture_diffuse1;
layout(r32ui) uniform uimage2DArray voxelTextures;
layout(r32ui) uniform uimage3D voxel3DData;

uniform uint voxelRes;


in vec2 TexCoords;
in vec4 worldPosition;
in vec4 viewPosition;
in vec3 viewNormal;
in vec3 voxelTexCoord;

flat in uint domInd;


#include "lights.glsl"



layout (std140) uniform GlobalMatrices
{
    mat4 projectionMatrix;
    mat4 viewMatrix;
    mat4 inverseViewMatrix;
	mat4 voxelMatrix;
	mat4 inverseVoxelMatrix;
};

layout (std140) uniform MaterialProperties
{
    vec4 albedoColor;
    vec4 emissiveColor;
    vec4 specular;
};




struct DrawElementsIndirectCommand {
	uint vertexCount;
	uint instanceCount;
	uint firstVertex;
	uint baseVertex;
	uint baseInstance;
};

struct ComputeIndirectCommand {
	uint workGroupSizeX;
	uint workGroupSizeY;
	uint workGroupSizeZ;
};

layout(std430, binding = 0) buffer DrawCmdBuffer 
{
	DrawElementsIndirectCommand drawCmd[10];
};

layout(std430, binding = 1) buffer ComputeCmdBuffer 
{
	ComputeIndirectCommand compCmd[10];
};

layout(std430, binding = 2) writeonly buffer SparseBuffer 
{
	uint sparseList[];
};





// Data storage scheme based on the work by Wahlén, Conrad: "Global Illumination in Real-Time using Voxel Cone Tracing on Mobile Devices." (2016).
// found at: https://liu.diva-portal.org/smash/get/diva2:1148572/FULLTEXT01.pdf

struct VoxelData 
{
	vec4 color;
	uint count; // the count is used during mipmapping to average the voxel color of the next mip
};

uint packARGB8(VoxelData dataIn) 
{
	uint result = 0;

	uvec3 uiColor = uvec3(dataIn.color.rgb * 31.0f * float(dataIn.count));

	//result |= (dataIn.light & 0x0F) << 28;
	//result |= (dataIn.count & 0x0F) << 24;
	result |= (dataIn.count & 0xFF) << 24;
	result |= (uiColor.r & 0xFF) << 16;
	result |= (uiColor.g & 0xFF) << 8;
	result |= (uiColor.b & 0xFF);

	return result;
}

uint packRG11B10(uvec3 dataIn) 
{
	uint result = 0;

	result |= (dataIn.r & 0x7FF) << 21;
	result |= (dataIn.g & 0x7FF) << 10;
	result |= (dataIn.b & 0x3FF);

	return result;
}





//We define 2 sampling methods: one for when the model is textured and the other for untextured models. This allows the same shader program to be used
//for both cases, instead of having to build and manage 2 different programs
subroutine vec4 GetColor();

layout(index = 0) subroutine(GetColor) 
vec4 AlbedoColor() {return albedoColor;}

layout(index = 1) subroutine(GetColor)
vec4 TextureColor() {return vec4(texture(texture_diffuse1, TexCoords).rgb, 1.0f);}

layout(location = 0) subroutine uniform GetColor SampleColor;








void main()
{	                       
	ivec3 voxelCoord = ivec3(voxelTexCoord * voxelRes);	
	
	vec3 vNormal = normalize(viewNormal);  
    vec3 vLight = normalize(dirLights[0].direction.xyz);

	vec4 worldNormal = (inverseViewMatrix * vec4(vNormal, 0.0f));
	float shadow = GetDirLightShadow(0, viewPosition.xyz, worldPosition.xyz, worldNormal.xyz);

	//simple diffuse lighting
	float NdotL = dot(vNormal,vLight);
    float diff = max(NdotL, 0.0);

	vec4 color = SampleColor().rgba;
	//color.rgb *= shadow;
	color.rgb *= diff * shadow;
	
	//VoxelData data = VoxelData(vec4(1.0f), 0x8);
	//uint outData = packARGB8(data);

	//writes the "most lit" voxel to the 2d debug texture:
	//imageAtomicMax(voxelTextures, ivec3(ivec2(gl_FragCoord.xy), domInd), outData);

	// Store the the "most lit" value:
	//uint prevData = imageAtomicMax(voxel3DData, voxelCoord, outData);
	
	// Storing average color:
	//atomic moving average         color assumed to be floats between 0.0 and 1.0
	uint nextUint = packUnorm4x8(vec4(color.rgb, 1.0 / 255.0f));
	uint prevUint = 0;
	uint curUint = imageAtomicCompSwap(voxel3DData, voxelCoord, prevUint, nextUint);

	// if this voxel was empty before, add it to the sparse list
	if(curUint == 0) 
	{
		uint prevVoxelCount = atomicAdd(drawCmd[0].instanceCount, 1);
		
		// Calculate and store number of workgroups needed
		// compWorkGroups = (prevVoxelCount + 1)//64 + 1;  each workgroup has a size of 64
		uint compWorkGroups = ((prevVoxelCount + 1) >> 6) + 1; // 6 = log2(workGroupSize = 64)
		atomicMax(compCmd[0].workGroupSizeX, compWorkGroups);

		// Write to position buffer (sparse list)
		sparseList[prevVoxelCount + drawCmd[0].baseInstance] = packRG11B10(uvec3(voxelCoord));
	}

	while(curUint != prevUint) 
	{
		prevUint = curUint;

		// Unpack newly read value, and counter.
		vec4 avg = unpackUnorm4x8(curUint);
		uint count = uint(avg.a * 255.0f);
		// Calculate and pack new average.
		avg = vec4((avg.rgb * count + color.rgb) / float(count + 1), float(count + 1) / 255.0f);
		nextUint = packUnorm4x8(avg);

		curUint = imageAtomicCompSwap(voxel3DData, voxelCoord, prevUint, nextUint);
	}								  //image      Position    compare    data
}