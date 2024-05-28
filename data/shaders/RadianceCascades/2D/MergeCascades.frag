#version 330 core
//Based on https://github.com/Yaazarai/GMShaders-Radiance-Cascades/tree/main

out vec4 FragColor;
in vec2 TexCoords;

uniform sampler2D upperCascade; // Input Distance Field.
uniform sampler2D currentCascade;

uniform float cascadeStorageSize; 
uniform float c0Angular;  // Cascade angular resolution.
uniform int cascadeCount; 
uniform int cascadeIndex; 

#define PI 3.141592


struct ProbeInfo 
{
	float rayCount;
	float probeSize;
	float rayIndex;
	vec2 probeTexel;
	vec2 probePos;
};

ProbeInfo CascadeProbeInfo(vec2 coord, int cascadeIndex) 
{
	float rayCount = c0Angular * float(1 << (2 * cascadeIndex));
	float probeSize = sqrt(rayCount);
	vec2  probeTexel = mod(floor(coord), vec2(probeSize));
	float rayIndex = (probeTexel.y * probeSize) + probeTexel.x;
	vec2 probePos = floor(coord / vec2(probeSize));
	return ProbeInfo(rayCount, probeSize, rayIndex, probeTexel, probePos);
}

vec4 FetchUpperCascade(float probeSize, vec2 texelIndex, float rayIndex) 
{
	vec2 probeTexel = texelIndex * probeSize;
	probeTexel += vec2(mod(rayIndex, probeSize), rayIndex / probeSize);
	vec2 cascadeTexelPosition = probeTexel / cascadeStorageSize;
	
	if (cascadeTexelPosition.x < 0.0 || cascadeTexelPosition.y < 0.0 || cascadeTexelPosition.x >= 1.0 || cascadeTexelPosition.y >= 1.0)
		return vec4(0.0, 0.0, 0.0, 0.0);
	
	return texture2D(upperCascade, cascadeTexelPosition);
}

void main() 
{
    vec2 cascadeCoord = TexCoords * vec2(cascadeStorageSize); 
	ProbeInfo probeInfoCurrent = CascadeProbeInfo(cascadeCoord, cascadeIndex);
	ProbeInfo probeInfoUpper = CascadeProbeInfo(cascadeCoord, cascadeIndex + 1);
	
	vec2 texelIndexUpper = floor((vec2(probeInfoCurrent.probePos) - 1.0) / 2.0);
	vec2 texelLowerLeftUpper = floor((texelIndexUpper * 2.0) + 1.0);
	
	vec4 radiance = texture2D(currentCascade, TexCoords);
	radiance.a = 1.0 - radiance.a;

    // if there was a hit in the current cascade, dont merge with upper
	if (radiance.a != 0.0) 
    {
		vec4 TL = vec4(0.0), TR = vec4(0.0), BL = vec4(0.0), BR = vec4(0.0);
		
		for(float i = 0.0; i < 4; i++) {
			float rayIndexUpper = (probeInfoCurrent.rayIndex * 4.0) + i; //default is 4x ray branch scaling factor between cascades.

			TL += FetchUpperCascade(probeInfoUpper.probeSize, texelIndexUpper + vec2(0.0,0.0), rayIndexUpper);
			TR += FetchUpperCascade(probeInfoUpper.probeSize, texelIndexUpper + vec2(1.0,0.0), rayIndexUpper);
			BL += FetchUpperCascade(probeInfoUpper.probeSize, texelIndexUpper + vec2(0.0,1.0), rayIndexUpper);
			BR += FetchUpperCascade(probeInfoUpper.probeSize, texelIndexUpper + vec2(1.0,1.0), rayIndexUpper);
		}
		
		// Per Specification:
		//vec2 weight = vec2(0.25) + (vec2(probeInfoCurrent.probePos) - texelLowerLeftUpper) * vec2(0.5);
		
		// Smoother Weights:
		vec2 weight = vec2(0.33) + (vec2(probeInfoCurrent.probePos) - texelLowerLeftUpper) * vec2(0.33);
		
		vec4 interpolated = mix(mix(TL, TR, weight.x), mix(BL, BR, weight.x), weight.y) / 4.0;
		interpolated.a = 1.0 - interpolated.a;
		radiance += radiance.a * interpolated;
	}
	
	FragColor = vec4(radiance.rgb, 1.0);
}