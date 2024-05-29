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
	vec2 texelTopLeftUpper = floor((texelIndexUpper * 2.0) + 1.0);


	vec4 radiance = texture2D(currentCascade, TexCoords);

    // if there was a hit in the current cascade, dont merge with upper
	if (radiance.a != 1.0) 
    {
		vec4 ProbeRadianceBL = vec4(0.0), ProbeRadianceTR = vec4(0.0), ProbeRadianceTL = vec4(0.0), ProbeRadianceBR = vec4(0.0);
		
		for(float i = 0.0; i < 4.0; i++) 
		{
			float rayIndexUpper = (probeInfoCurrent.rayIndex * 4.0) + i ; //default is 4x ray branch scaling factor between cascades.

			ProbeRadianceBL += FetchUpperCascade(probeInfoUpper.probeSize, texelIndexUpper + vec2(0.0,0.0), rayIndexUpper)/ 4.0;
			ProbeRadianceBR += FetchUpperCascade(probeInfoUpper.probeSize, texelIndexUpper + vec2(1.0,0.0), rayIndexUpper)/ 4.0;
			ProbeRadianceTL += FetchUpperCascade(probeInfoUpper.probeSize, texelIndexUpper + vec2(0.0,1.0), rayIndexUpper)/ 4.0;
			ProbeRadianceTR += FetchUpperCascade(probeInfoUpper.probeSize, texelIndexUpper + vec2(1.0,1.0), rayIndexUpper)/ 4.0;
		}
		
		vec2 weight = vec2(0.25) + (probeInfoCurrent.probePos - texelTopLeftUpper) * vec2(0.5);
		
		vec4 interpolated = mix(mix(ProbeRadianceBL, ProbeRadianceBR, weight.x), mix(ProbeRadianceTL, ProbeRadianceTR, weight.x), weight.y);
		radiance += interpolated;
	}
	
	FragColor = vec4(radiance.rgb, 1.0);
}