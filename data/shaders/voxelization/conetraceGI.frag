#version 440


in vec2 TexCoords;
out vec4 outColor;

uniform usampler2DArray voxel2DTextures;
uniform usampler3D voxel3DData;
uniform sampler2D gColorSpec; 
uniform sampler2D gNormal;
uniform sampler2D gPosition;  

#include "lights.glsl"


uniform uint voxelRes;
uniform float maxConeDistance;
uniform float aoDistance;
uniform float accumThr;
uniform float maxLOD;

layout (std140) uniform GlobalMatrices
{
    mat4 projectionMatrix;
    mat4 viewMatrix;
    mat4 inverseViewMatrix;
	mat4 voxelMatrix;
	mat4 inverseVoxelMatrix;
};



// same data storage scheme described by: Wahlén, Conrad. "Global Illumination in Real-Time using Voxel Cone Tracing on Mobile Devices." (2016).
// found at: https://liu.diva-portal.org/smash/get/diva2:1148572/FULLTEXT01.pdf

struct VoxelData {
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


	float div[] = { 0.0f, 1.0f / float(data.count) };
	int index = int(data.count > 0);

	data.color.rgb = vec3(uiColor) / 31.0f * div[index];
	data.color.a = float(sign(data.count));

	return data;
}





vec4 voxelSampleLevel(vec3 position, float level) 
{
	float mip = round(level);
	float scale = float(voxelRes >> int(mip)); // the resolution of the current mip level

	position *= scale;
	position = position - vec3(0.5f);
	vec3 positionWhole = floor(position);
	vec3 disp = position - positionWhole;

	//Due to the voxel textures being packed as uints we cant use hardware accelerated trilinear/quadrilinear sampling, so most of the cost
	//comes from the 8 samples required here:

	vec3 offsets[] = 
	{  
		vec3(0.0f, 0.0f, 0.0f),
		vec3(0.0f, 0.0f, 1.0f),
		vec3(0.0f, 1.0f, 0.0f),
		vec3(0.0f, 1.0f, 1.0f),
		vec3(1.0f, 0.0f, 0.0f),
		vec3(1.0f, 0.0f, 1.0f),
		vec3(1.0f, 1.0f, 0.0f),
		vec3(1.0f, 1.0f, 1.0f) 
	};


	


	vec3 voxelPos = (positionWhole) / scale;

	//calculating trilinear sampling coeficient: 
	vec3 offset = 1.0f - disp; 
	float factor = offset.x * offset.y * offset.z;

	VoxelData voxel = unpackARGB8(textureLod(voxel3DData, voxelPos, mip).r);

	vec4 total = voxel.color * factor;// * float(sign(int(voxel.light)));

	//vec4 total = vec4(0.0f);
	

///////////////////////////////////////////////////////////////////////////////////////////////////
	//something goes really wrong when summing up the contributions of all 8 nearest voxels !!!
	//////////////////////////////////////////////////////////////////////////////////////////////
	
	for(int i = 1; i < 7; i++) // i < 8
	{
		voxelPos = (positionWhole + offsets[i]) / scale;

		
		//calculating trilinear sampling coeficient: 
		offset = mix(1.0f - offsets[i], offsets[i], disp); 
        factor = offset.x * offset.y * offset.z;


        VoxelData voxel = unpackARGB8(textureLod(voxel3DData, voxelPos, mip).r);
		
	

		total += voxel.color * factor;// * float(sign(int(voxel.light)));

		//total += voxel.color;// * float(sign(int(voxel.light)));
	}

	//return total/8.0f;

	return total;
}

//this will trace the cones with 60 degree apperture angles, which allows to increment the LOD by one on each trace and simplifies the math for sampling voxels
vec4 ConeTrace60(vec3 startPos, vec3 dir, float aoDist, float voxelSize) 
{
	vec4 accum = vec4(0.0f);
	float opacity = 0.0f;
	vec3 samplePos;
	vec4 sampleValue;
	float sampleLOD = 0.0f;

	
	for(float dist = voxelSize ; dist < maxConeDistance && accum.a < accumThr && sampleLOD <= maxLOD;) 
	{
		samplePos = startPos + dir * dist;
		sampleValue = voxelSampleLevel(samplePos, sampleLOD);

		accum.rgb =   accum.rgb + (1.0f - accum.a) * sampleValue.a * sampleValue.rgb;
		accum.a = accum.a + (1-accum.a) * sampleValue.a;
		//accum.a = accum.a +  sampleValue.a;


		

		//for ambient occlusion we only consider close samples
		opacity = (dist < aoDist) ? accum.a : opacity; // higher accumulated opacity near the sampling point means a bigger AO effect 

		sampleLOD += 1.0f;
		dist *= 2.0f;
	}

	return vec4(sampleValue.rgb, 1.0f - opacity);
}

vec4 DiffuseTrace(vec3 voxelPos, vec3 worldNormal) 
{
	// cone trace directions:
	vec3 dirs[] = {  
		vec3( 0.000000f, 1.000000f,  0.000000f), //up

		//60 degree spaced vectors:
		vec3( 0.000000f, 0.500000f,  0.866025f), 
		vec3( 0.823639f, 0.500000f,  0.267617f), 
		vec3( 0.509037f, 0.500000f, -0.700629f), 
		vec3(-0.509037f, 0.500000f, -0.700629f), 
		vec3(-0.823639f, 0.500000f,  0.267617f) 
	};

	float weight[2];
	//the weight of the "up" cone is bigger than the other cones
	weight[0] = 0.1666666f;
	weight[1] = (1.0f - weight[0]) / 5.0f;
	
	float voxelSize = 2.0f / float(voxelRes);
	float aoMaxDist = min(aoDistance, maxConeDistance);

	voxelPos += worldNormal * voxelSize * 2;

	//constructing a vector basis using the world normal:
	vec3 U = (abs(worldNormal.y) < 0.9) ? vec3(0.0f, 1.0f, 0.0f) : vec3(0.0f, 0.0f, 1.0f); 
	vec3 R = normalize(cross(U, worldNormal));
	U = normalize(cross(worldNormal, R)); 
	//vec3 di = dirs[0].x * R + dirs[0].y * worldNormal + dirs[0].z * U;
	//return vec4(voxelPos,1.0);

	vec4 total = vec4(0.0f);
	for(int i = 0; i < 1; i++) 
	{
		vec3 direction = dirs[i].x * R + dirs[i].y * worldNormal + dirs[i].z * U;
		total += weight[int(i != 0)] * ConeTrace60(voxelPos, direction, aoMaxDist, voxelSize);
	}

	
	return total;
}



void main()
{	
	vec4 ViewFragPos = texture(gPosition, TexCoords);
    vec4 viewNormal = texture(gNormal, TexCoords);

	vec4 worldPos = inverseViewMatrix * ViewFragPos;
	vec4 worldNormal = (inverseViewMatrix * vec4(viewNormal.xyz,0.0));


	vec4 cs = texture(gColorSpec, TexCoords);
	vec4 c = vec4(1.0f);
	c.rgb = cs.rgb;

    vec4 l = CalcDirLight(0, viewNormal.xyz, -normalize(ViewFragPos.xyz), vec3(1,1,1), cs.a);
    float s = GetDirLightShadow(0, ViewFragPos.xyz, worldPos.xyz, worldNormal.xyz);
	
	vec4 voxelPos = voxelMatrix * worldPos;
	voxelPos.xyz  = (voxelPos.xyz + vec3(1.0f)) * 0.5f;
    vec4 d = DiffuseTrace(voxelPos.xyz, worldNormal.xyz);

	s = mix(d.w, s * d.w, 0.9);

	vec4 f = vec4(1.0f);
	//f.xyz = l.xyz * s * c.xyz + 2.0 * d.xyz * c.xyz;
	//f.xyz = l.xyz * s * c.xyz;
	//f.x = s;
	f.xyz =  d.xyz;

	outColor = f;
}