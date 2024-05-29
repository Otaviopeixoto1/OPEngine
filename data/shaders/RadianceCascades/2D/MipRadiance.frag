#version 330 core

out vec4 FragColor;
in vec2 TexCoords;

uniform sampler2D mergedCascades;

uniform float cascadeStorageSize; 
uniform float mipStorageSize; 
uniform float raymarchRegionSize;
uniform vec2 sceneDimensions;

uniform float c0Spacing;  // Cascade 0 probePos spacing.
uniform float c0Interval; // Cascade 0 radiance interval.
uniform float c0Angular;  // Cascade angular resolution.

uniform int cascadeIndex;     // Cascade index.

struct ProbeTexel 
{
	float count;
	float size;
	float probes;
};

ProbeTexel CascadeProbeTexel(int index) 
{
	float count = c0Angular * float(1 << (2 * index));
	float size = sqrt(count);
	float probes = cascadeStorageSize / size;
	return ProbeTexel(count, size, probes);
}

vec4 CascadeFetch(ProbeTexel info, vec2 texelIndex, float thetaIndex) 
{
	vec2 probeTexel = texelIndex * info.size;
	probeTexel += vec2(mod(thetaIndex, info.size), (thetaIndex / info.size));
	vec2 cascadeTexelPosition = probeTexel / cascadeStorageSize;
	return texture2D(mergedCascades, cascadeTexelPosition);
}

void main() 
{
	// Get the mipmap's cascade texel info based on the cascade being rendered.
	ProbeTexel probeInfo = CascadeProbeTexel(cascadeIndex);
	vec2 mipmapCoord = vec2(TexCoords * mipStorageSize);
	
	// Loops through all of the radiance intervals for this mip-map and accumulate.
	vec4 radiance = vec4(0.0, 0.0, 0.0, 0.0);
	for(float i = 0.0; i < probeInfo.count; i ++)
		// CascadeFetch uses the probe's cell index, which is the same as the mipmap's pixel position.
		radiance += CascadeFetch(probeInfo, mipmapCoord, i);
	
	FragColor = vec4(radiance.rgb / probeInfo.count, 1.0);
}