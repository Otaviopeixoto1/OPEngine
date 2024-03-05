#version 440

layout(location = 0) uniform vec3 diffColor;//a diffuse value set for untextured models
layout(binding = 0) uniform sampler2D texture_diffuse1; //the model diffuse texture (if there is one)


layout(binding = 2, r32ui) uniform uimage2DArray voxelTextures;
layout(binding = 3, r32ui) uniform uimage3D voxelData;
layout(binding = 5) uniform sampler2DShadow shadowMap;


in vec2 intTexCoords;
in vec4 shadowCoord;
in vec3 viewNormal;

flat in uint domInd;


#include "lights.glsl"



layout (std140) uniform GlobalMatrices
{
    mat4 projectionMatrix;
    mat4 viewMatrix;
    mat4 inverseViewMatrix;
	mat4 SceneMatrices[3];
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

layout(std430, binding = 0) buffer DrawCmdBuffer {
	DrawElementsIndirectCommand drawCmd[10];
};

struct ComputeIndirectCommand {
	uint workGroupSizeX;
	uint workGroupSizeY;
	uint workGroupSizeZ;
};

layout(std430, binding = 1) buffer ComputeCmdBuffer {
	ComputeIndirectCommand compCmd[10];
};

layout(std430, binding = 2) writeonly buffer SparseBuffer {
	uint sparseList[];
};





// same data storage scheme described by: Wahlén, Conrad. "Global Illumination in Real-Time using Voxel Cone Tracing on Mobile Devices." (2016).
// found at: https://liu.diva-portal.org/smash/get/diva2:1148572/FULLTEXT01.pdf

struct VoxelData 
{
	vec4 color;
	uint light; // voxels in shadow have light = 0 and lit voxels have light = 1
	uint count; // the amount of voxels 
};

uint packARGB8(VoxelData dataIn) 
{
	uint result = 0;

	uvec3 uiColor = uvec3(dataIn.color.rgb * 31.0f * float(dataIn.count));

	result |= (dataIn.light & 0x0F) << 28;
	result |= (dataIn.count & 0x0F) << 24;
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





//We define 2 sampling methods: one for when the model is textured and the other for untextured models
subroutine vec4 SampleColor();

layout(index = 0) subroutine(SampleColor) 
vec4 DiffuseColor() {
	return albedoColor;
}

layout(index = 1) subroutine(SampleColor)
vec4 TextureColor() {
	return vec4(texture(texture_diffuse1, intTexCoords).rgb, 1.0f);
}

layout(location = 0) subroutine uniform SampleColor GetColor;






float GetMainDirLightShadow(vec3 worldPos, vec3 worldNormal)
{
	//use view normal instead of world normal
	//float bias = max(0.005 * (1.0 - dot(worldNormal, dirLights[lightIndex].direction.xyz)), 0.0005);
	float bias = 0.00050;
	vec2 texelSize = 1.0 / textureSize(shadowMap0, 0).xy;  
	vec3 normalBias = worldNormal * max(texelSize.x, texelSize.y) * 1.4142136f * 100;


	vec4 posClipSpace = lightSpaceMatrices[0] * vec4(worldPos.xyz + normalBias, 1);

	// Perspective division is only realy necessary with perspective projection, 
	// it wont affect ortographic projection be we do it anyway:
	vec3 ndcPos = posClipSpace.xyz / posClipSpace.w;
	ndcPos = ndcPos * 0.5 + 0.5; 

	// Depth samples to compare   
	float currentDepth = ndcPos.z; 

	
	// One sample:
	//float closestDepth = texture(shadowMap0, vec3(ndcPos.xy, 0)).r;
	//float shadow = currentDepth - bias > closestDepth  ? 0.0 : 1.0;



	// Multiple samples (4x4 kernel):
	float shadow = 0.0f;
	for(float x = -1.5; x <= 1.5; ++x)
	{
		for(float y = -1.5; y <= 1.5; ++y)
		{
			//float pcfDepth = texture(shadowMap0, vec3(ndcPos.xy + vec2(x, y) * texelSize, 0) ).r; 
			//shadow += currentDepth - bias > pcfDepth ? 0.0 : 1.0; 
			
			//for hardware pcf       
			shadow += texture(shadowMap0, vec4(ndcPos.xy + vec2(x, y) * texelSize, 0, currentDepth - bias));
		}    
	}
	shadow /= 9.0f;

	return shadow;
}





void main()
{	                           //color, light, count
	VoxelData data = VoxelData(vec4(0.0f), 0x0, 0x8);

	ivec3 voxelCoord;					//256 = voxelres. Add variable for this
	int depthCoord = int(gl_FragCoord.z * 256);

	if(domInd == 0) 
		voxelCoord = ivec3(depthCoord, gl_FragCoord.y, 256 - gl_FragCoord.x); 
	else if (domInd == 1) 
		voxelCoord = ivec3(gl_FragCoord.x, depthCoord, 256 - gl_FragCoord.y);
	else 
		voxelCoord = ivec3(gl_FragCoord.x, gl_FragCoord.y, depthCoord);
	
	vec3 vNormal = normalize(viewNormal);  

    vec3 vLight = normalize(dirLights[0].direction.xyz);

	vec3 worldNormal = (inverseViewMatrix * vec4(vNormal, 0.0f)).xyz;
	float shadow = GetMainDirLightShadow(gl_FragCoord.xyz, worldNormal);
	data.light = uint(8.0f * shadow);

	//simple diffuse lighting
	float NdotL = dot(vNormal,vLight);
    float diff = max(NdotL, 0.0);
	data.color = GetColor() *diff;
	
	
	uint outData = packARGB8(data);

	//writes the "most lit" voxel to the 2d texture
	imageAtomicMax(voxelTextures, ivec3(ivec2(gl_FragCoord.xy), domInd), outData);
	uint prevData = imageAtomicMax(voxelData, voxelCoord, outData);

	// if this voxel was empty before, add it to the sparse list
	if(prevData == 0) 
	{
		uint prevVoxelCount = atomicAdd(drawCmd[0].instanceCount, 1);
		
		// Calculate and store number of workgroups needed
		// compWorkGroups = (prevVoxelCount + 1)//64 + 1;  each workgroup has a size of 64
		uint compWorkGroups = ((prevVoxelCount + 1) >> 6) + 1; // 6 = log2(workGroupSize = 64)
		atomicMax(compCmd[0].workGroupSizeX, compWorkGroups);

		// Write to position buffer (sparse list)
		sparseList[prevVoxelCount + drawCmd[0].baseInstance] = packRG11B10(uvec3(voxelCoord));
	}
}