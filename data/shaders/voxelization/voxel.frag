#version 440

layout(location = 0) uniform vec3 diffColor;//a diffuse value set for untextured models
layout(binding = 0) uniform sampler2D texture_diffuse1; //the model diffuse texture (if there is one)


layout(binding = 2, r32ui) uniform uimage2DArray voxelTextures;
layout(binding = 3, r32ui) uniform uimage3D voxelData;
layout(binding = 5) uniform sampler2DShadow shadowMap;


in vec2 intTexCoords;
in vec4 shadowCoord;
in vec3 exNormal;

flat in uint domInd;


#include "lights.glsl"



layout (std140) uniform GlobalMatrices
{
    mat4 projectionMatrix;
    mat4 viewMatrix;
    mat4 inverseViewMatrix;
	mat4 SceneMatrices[3];
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







struct VoxelData 
{
	vec4 color;
	uint light;
	uint count;
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
	

	// Calculate shadows
	vec3 s = shadowCoord.xyz * 0.5f + 0.5f;
    vec3 l = dirLights[0].direction.xyz;
    float cosTheta = max(dot(l,normalize(exNormal)), 0.0f);
    float b = 0.005f*tan(acos(cosTheta));
    b = clamp(b, 0.0f,0.02f);
    s.z = s.z - b;

	float shadow = texture(shadowMap0, vec4(s.xy, 0, s.z));
	data.light = uint(8.0f * (1.0f - shadow));

	// Set color
	data.color = GetColor() * cosTheta;
	
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