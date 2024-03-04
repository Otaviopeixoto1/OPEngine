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
uniform uint voxelTextureInd;

layout (std140) uniform GlobalMatrices
{
    mat4 projectionMatrix;
    mat4 viewMatrix;
    mat4 inverseViewMatrix;
	mat4 SceneMatrices[3];
};



// same data storage scheme described by: Wahlén, Conrad. "Global Illumination in Real-Time using Voxel Cone Tracing on Mobile Devices." (2016).
// found at: https://liu.diva-portal.org/smash/get/diva2:1148572/FULLTEXT01.pdf

struct VoxelData {
	vec4 color;
	uint light;
	uint count;
};


VoxelData unpackARGB8(uint bytesIn) {
	VoxelData data;
	uvec3 uiColor;

	// Put a first to improve max operation but it should not be very noticable
	data.light = (bytesIn & 0xF0000000) >> 28;
	data.count = (bytesIn & 0x0F000000) >> 24;
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


	vec4 total = vec4(0.0f);
	
	//iterate and sample over a 2 x 2 x 2 cube                   
	for(int i = 0; i < 8; i++) // i < 8
	{
		vec3 voxelPos = (positionWhole + offsets[i]) / scale;

		//trilinear sampling coeficient:     *improve this part by making it clear* 
		vec3 offset = mix(1.0f - offsets[i], offsets[i], disp); 
        float factor = offset.x * offset.y * offset.z;


        VoxelData voxel = unpackARGB8(textureLod(voxel3DData, voxelPos, mip).r);
		
		//check if voxelpos is out of bounds
        if(any(greaterThan(voxelPos, vec3(1.0f))) || any(lessThan(voxelPos, vec3(0.0f)))) 
		{
            factor = 0.0f;
        }

		total.rgb += voxel.color.rgb * factor * float(sign(int(voxel.light)));
		total.a += voxel.color.a * factor;
	}


	return total;
}

//this will trace the cones with 60 degree apperture angles, which allows to increment the LOD by one on each trace and simplifies the math for sampling voxels
vec4 ConeTrace60(vec3 startPos, vec3 dir, float aoDist, float maxDist, float voxelSize) 
{
	vec4 accum = vec4(0.0f);
	float opacity = 0.0f;
	vec3 samplePos;
	vec4 sampleValue;
	float sampleWeight;
	float sampleLOD = 0.0f;
	
	for(float dist = voxelSize; dist < maxDist && accum.a < 1.0f;) 
	{
		samplePos = startPos + dir * dist;
		sampleValue = voxelSampleLevel(samplePos, sampleLOD);
		sampleWeight = 1.0f - accum.a;
		accum += sampleValue * sampleWeight;

		//for ambient occlusion we only consider close samples
		opacity = (dist < aoDist) ? accum.a : opacity; // higher accumulated opacity near the sampling point means a bigger AO effect 

		sampleLOD += 1.0f;
		dist *= 2.0f;
	}

	return vec4(accum.rgb, 1.0f - opacity);
}

vec4 DiffuseTrace(vec3 worldPos, vec3 worldNormal, float maxDistance) 
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
	weight[0] = 0.25f;
	weight[1] = (1.0f - weight[0]) / 5.0f;
	
	float voxelSize = 2.0f / float(voxelRes);
	float aoDistance = min(0.03f, maxDistance);

	worldPos += worldNormal * voxelSize;

	//constructing a vector basis using the world normal:
	vec3 U = (abs(worldNormal.y) < 0.7) ? vec3(0.0f, 1.0f, 0.0f) : vec3(0.0f, 0.0f, 1.0f); 
	vec3 R = normalize(cross(U, worldNormal));
	U = normalize(cross(worldNormal, R)); // U was not orthonormal


	vec4 total = vec4(0.0f);
	for(int i = 0; i < 6; i++) 
	{
		vec3 direction = dirs[i].x * R + dirs[i].y * worldNormal + dirs[i].z * U;
		total += weight[int(i != 0)] * ConeTrace60(worldPos, direction, aoDistance, maxDistance, voxelSize);
	}

	return total;
}







/*


vec4 voxelSampleBetween(vec3 position, float level) {
	float levelLow = floor(level);
	float levelHigh = levelLow + 1.0f;
	float intPol = level - levelLow;

	vec4 voxelLow = voxelSampleLevel(position, levelLow);
	vec4 voxelHigh = voxelSampleLevel(position, levelHigh);

	return mix(voxelLow, voxelHigh, intPol);
}


vec4 ConeTrace(vec3 startPos, vec3 dir, float coneRatio, float maxDist,	float voxelSize) {

	vec4 accum = vec4(0.0f);
	vec3 samplePos;
	vec4 sampleValue;
	float sampleRadius;
	float sampleDiameter;
	float sampleWeight;
	float sampleLOD = 0.0f;
	
	for(float dist = voxelSize; dist < maxDist && accum.a < 1.0f;) {
		sampleRadius = coneRatio * dist;
		sampleDiameter = max(2.0f * sampleRadius, voxelSize);
		sampleLOD = log2(sampleDiameter * float(scene.voxelRes));
		samplePos = startPos + dir * (dist + sampleRadius);
		sampleValue = voxelSampleBetween(samplePos, sampleLOD);
		sampleWeight = 1.0f - accum.a;
		accum += sampleValue * sampleWeight;
		dist += sampleRadius;
	}

	return accum;
}





vec4 AngleTrace(vec3 dir, float theta) {
	float voxelSize = 1.0f / float(scene.voxelRes);
	float maxDistance = sqrt(3.0f);
	
	vec3 pos = texture(gPosition, TexCoords).xyz;
	vec3 norm = texture(gNormal, TexCoords).xyz;
	pos += norm * voxelSize * 2.0f;

	
	float halfTheta = sin(radians(theta)/2.0f);
	float coneRatio = halfTheta / (1.0f - halfTheta);
	return ConeTrace(pos, dir, coneRatio, maxDistance, voxelSize);
}*/








// Shadows
/*
float ShadowMapping(vec3 p, vec3 n) {
    p = p * 2.0f + vec3(-1.0f, -1.0f, -1.0f);
    vec4 s = scene.MTShadowMatrix * vec4(p, 1.0f);
    s = s * 0.5f + 0.5f;

    vec3 l = l = dirLights[0].direction.xyz;
    float cosTheta = max(dot(l,n), 0.0f);
    float b = 0.005f; //*tan(acos(cosTheta));
    b = clamp(b, 0.0f,0.02f);
    s.z = s.z - b;

    return SceneShadow(s.xyz);
}

vec4 SoftShadows() {
	vec4 result = AngleTrace(scene.lightDir, 5.0f);
	vec4 color = texture(gColorSpec, TexCoords);
	return vec4(color.rgb * (1.0f - result.a), 1.0f);
}
*/

// Remove the basic stuff and keep the best looking one

subroutine vec4 DrawScene();

layout(index = 0) subroutine(DrawScene)
vec4 BasicScene() 
{
    vec4 ViewFragPos = texture(gPosition, TexCoords);
    vec3 viewNormal = texture(gNormal, TexCoords).xyz;

    if (length(viewNormal) < 0.5f) 
        return vec4(0.0f);
    
    vec4 cs = texture(gColorSpec, TexCoords);
	vec4 c = vec4(1.0f);
	c.rgb = cs.rgb;
	
	vec4 f = c * CalcDirLight(0, viewNormal, -normalize(ViewFragPos.xyz), vec3(1,1,1), cs.a);
    return f;
}


layout(index = 1) subroutine(DrawScene)
vec4 BasicShadows() 
{
    vec4 ViewFragPos = texture(gPosition, TexCoords);
    vec3 viewNormal = texture(gNormal, TexCoords).xyz;

	vec3 worldNormal = (inverseViewMatrix * vec4(viewNormal,0.0)).xyz;
    vec4 worldPos = inverseViewMatrix * ViewFragPos;

    if (length(viewNormal) < 0.5f) 
        return vec4(0.0f);
    
	vec4 cs = texture(gColorSpec, TexCoords);
	vec4 c = vec4(1.0f);
	c.rgb = cs.rgb;
	
	vec4 f = c * CalcDirLight(0, viewNormal, -normalize(ViewFragPos.xyz), vec3(1,1,1), cs.a);
	f = f * GetDirLightShadow(0, ViewFragPos.xyz, worldPos.xyz, worldNormal);

    return f;
}
/*
layout(index = 2) subroutine(DrawScene)
vec4 BasicAOShadows() 
{
	vec4 ViewFragPos = texture(gPosition, TexCoords);
    vec3 viewNormal = texture(gNormal, TexCoords).xyz;

	vec4 worldPos = inverseViewMatrix * ViewFragPos;
	vec3 worldNormal = (inverseViewMatrix * vec4(viewNormal,0.0)).xyz;
    

    if (length(n) < 0.5f) {
        return vec4(0.0f);
    }


    float maxDst = 0.03f;
	
    vec4 c = texture(gColorSpec, TexCoords);
    vec3 l = BasicLight(p, n);
    float s = 1.0f - ShadowMapping(p, n);
	
    vec4 d = DiffuseTrace(worldPos, worldNormal, maxDst);
	
	s = mix(d.w, s * d.w, 0.9);

	vec4 f = vec4(1.0f);
	f.xyz = c.xyz * (0.05f * d.w + (l.y + 0.2f * l.z) * s);

	//return texture(gNormal, TexCoords);
    return f;
}*/

layout(index = 3) subroutine(DrawScene)
vec4 GIAOShadows() 
{
    vec4 ViewFragPos = texture(gPosition, TexCoords);
    vec3 viewNormal = texture(gNormal, TexCoords).xyz;

	vec4 worldPos = inverseViewMatrix * ViewFragPos;
	vec3 worldNormal = (inverseViewMatrix * vec4(viewNormal,0.0)).xyz;


    if (length(viewNormal) < 0.5f) 
        return vec4(0.0f);
    

    float maxDst = 1.5f;

	vec4 cs = texture(gColorSpec, TexCoords);
	vec4 c = vec4(1.0f);
	c.rgb = cs.rgb;

    vec4 l = CalcDirLight(0, viewNormal, -normalize(ViewFragPos.xyz), vec3(1,1,1), cs.a);
    float s = GetDirLightShadow(0, ViewFragPos.xyz, worldPos.xyz, worldNormal);

	//the coordinates of world position need to be scaled in the same way as the coordinates we used to store the voxels
	// (using the scenematrix[2])
	//the result is also view dependent, which doesnt make sense
    vec4 d = DiffuseTrace((SceneMatrices[2] * worldPos).xyz, worldNormal, maxDst);

	//s = mix(d.w, s * d.w, 0.9);

	vec4 f = vec4(1.0f);
	//f.xyz = l.xyz * s * c.xyz + 2.0f * d.xyz * c.xyz;
	//f.xyz = l.xyz * s * c.xyz;
	f.xyz =  d.xyz;

    return f;
}


/*
layout(index = 4) subroutine(DrawScene)
vec4 GIAOSoftShadows() {
    vec3 p = texture(gPosition, TexCoords).xyz;
    vec3 n = texture(gNormal, TexCoords).xyz;

    if (length(n) < 0.5f) {
        return vec4(0.0f);
    }

    vec3 t = SceneTangent().xyz;
    vec3 b = SceneBiTangent().xyz;
    float maxDst = 1.5f;
	float a = 0.9;
    vec4 c = texture(gColorSpec, TexCoords);
    vec3 l = BasicLight(p, n);
    float s = 1.0f - AngleTrace(scene.lightDir, 5.0f).a;
    vec4 d = DiffuseTrace(p, n, maxDst);

	s = mix(d.w, s * d.w, a);
	vec4 f = vec4(1.0f);
	f.xyz = l.y * s * c.xyz + 2.0f * d.xyz * c.xyz;

	//return vec4(vec3(1.0f - ShadowMapping(p,n)), 1.0f);
    return f;
}*/

layout(location = 0) subroutine uniform DrawScene SampleScene;

void main()
{	
	outColor = SampleScene();
}