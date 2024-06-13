#version 440


in vec2 TexCoords;
out vec4 outColor;

uniform sampler3D voxelColorTex;
uniform sampler2D gColorSpec; 
uniform sampler2D gNormal;
uniform sampler2D gPosition;  

#include "lights.glsl"


uniform uint voxelRes;
uniform float giIntensity;
uniform float stepMultiplier;
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

/*

vec4 voxelSampleLevel(vec3 position, float level) 
{
	float mip = round(level);
	float scale = float(voxelRes >> int(mip)); // the resolution of the current mip level

	position *= scale;
	position = position - vec3(0.5f);
	vec3 positionInt = floor(position);
	vec3 disp = position - positionInt;

	//Due to the voxel textures being packed as uints we cant use hardware accelerated trilinear/quadrilinear sampling, so most of the cost
	//comes from the 8 samples required here:

	vec3 offsets[] = 
	{  
		vec3(0.0f, 0.0f, 1.0f),
		vec3(0.0f, 1.0f, 0.0f),
		vec3(0.0f, 1.0f, 1.0f),
		vec3(1.0f, 0.0f, 0.0f),
		vec3(1.0f, 0.0f, 1.0f),
		vec3(1.0f, 1.0f, 0.0f),
		vec3(1.0f, 1.0f, 1.0f) 
	};

	vec3 voxelPos = (positionInt) / scale;

	//calculating trilinear sampling coeficient: 
	vec3 offset = 1.0f - disp; 
	float factor = offset.x * offset.y * offset.z;

	VoxelData voxel = unpackARGB8(textureLod(voxel3DData, voxelPos, mip).r);

	vec4 total = voxel.color * factor;
	
	for(int i = 0; i < 7; i++) // i < 7
	{
		voxelPos = (positionInt + offsets[i]) / scale;

		
		//calculating trilinear sampling coeficient: 
		offset = mix(1.0f - offsets[i], offsets[i], disp); 
        factor = offset.x * offset.y * offset.z;


        VoxelData voxel = unpackARGB8(textureLod(voxel3DData, voxelPos, int(mip)).r);
		
		total += voxel.color * factor;
		//total += voxel.color;
	}

	//return total/8.0f;

	return total;
}*/

//this will trace the cones with 60 degree apperture angles, which allows to increment the LOD by one on each trace and simplifies the math for sampling voxels
vec4 ConeTrace60(vec3 startPos, vec3 dir, float aoDist, float voxelSize) 
{
	vec4 accum = vec4(0.0f);
	float opacity = 0.0f;
	vec3 samplePos;
	vec4 sampleValue;

	float coneDiameter;
	float sampleDiameter;
	float sampleLod;

	for(float dist = voxelSize ; dist < maxConeDistance && accum.a < accumThr;) 
	{
		//coneDiameter = 2.0 * tan(coneAngle) * distFromStart;
		//            2.0 * tan(60) = 3.464101615
		coneDiameter = 3.464101615 * dist;
		sampleDiameter = max(voxelSize, coneDiameter);
		sampleLod = log2(sampleDiameter / voxelSize);

		samplePos = startPos + dir * dist;

		if (any(lessThan(samplePos, vec3(0.0))) || any(greaterThanEqual(samplePos, vec3(1.0))) || sampleLod > maxLOD )
        {
            break; //Or just sample the skybox
        }
		//sampleValue = voxelSampleLevel(samplePos, sampleLOD);
		sampleValue = textureLod(voxelColorTex, samplePos, sampleLod).rgba;

		accum += (1.0f - accum.a) * sampleValue;


		//if (sampleValue.a > 0.01){break;}

		//for ambient occlusion we only consider close samples
		opacity = (dist < aoDist) ? accum.a : opacity; // higher accumulated opacity near the sampling point means a bigger AO effect 

		dist += sampleDiameter * stepMultiplier;
	}

	return vec4(accum.rgb, 1.0f - opacity);
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
	
	float voxelSize = 2.0f / float(voxelRes); //size of a single voxel in the texture coords
	float aoMaxDist = min(aoDistance, maxConeDistance);

	voxelPos += worldNormal * voxelSize;// Offset for avoiding hiting the same voxel

	//constructing a vector basis using the world normal:
	vec3 U = (abs(worldNormal.y) < 0.9) ? vec3(0.0f, 1.0f, 0.0f) : vec3(0.0f, 0.0f, 1.0f); 
	vec3 R = normalize(cross(U, worldNormal));
	U = normalize(cross(worldNormal, R)); 

	//return vec4(voxelPos,1.0);

	vec3 direction = dirs[0].x * R + dirs[0].y * worldNormal + dirs[0].z * U;
	vec4 total = weight[0] * ConeTrace60(voxelPos, direction, aoMaxDist, voxelSize);

	//return vec4(direction, 1.0);

	for(int i = 1; i < 6; i++) 
	{
		direction = dirs[i].x * R + dirs[i].y * worldNormal + dirs[i].z * U;
		total += weight[1] * ConeTrace60(voxelPos, direction, aoMaxDist, voxelSize);
		//Sample the skybox here as well based on alpha
		//coneTrace += (1.0 - coneTrace.a) * (texture(skyBoxUBO.Albedo, coneRay.Direction) * settingsUBO.GISkyBoxBoost);
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
	
	vec4 voxelPos = voxelMatrix * worldPos;// in voxel space [-1, 1]
	voxelPos.xyz  = (voxelPos.xyz + vec3(1.0f)) * 0.5f; //remap to [0, 1] (texture coordinates)
    vec4 d = DiffuseTrace(voxelPos.xyz, worldNormal.xyz);

	//s = mix(d.w, s * d.w, 0.2);

	vec4 f = vec4(1.0f);
	f.xyz = (l.xyz * s * c.xyz + giIntensity * d.xyz * c.xyz) * d.w;
	//f.x = l.xyz * s * c.xyz;
	//f.xyz = d.xyz; 
	//f.xyz = d.www;

	outColor = f;
	//outColor = textureLod(voxelColorTex, voxelPos.xyz , 0);
}