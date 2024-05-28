#version 330 core

out vec4 FragColor;
in vec2 TexCoords;

uniform sampler2D mergedCascades;

uniform float cascadeStorageSize; 
uniform float raymarchRegionSize;
uniform vec2 sceneDimensions;

uniform float c0Spacing;  // Cascade 0 probePos spacing.
uniform float c0Interval; // Cascade 0 radiance interval.
uniform float c0Angular;  // Cascade angular resolution.

//uniform int cascadeIndex; //for rendering different cascades thant c0

/*

struct ProbeTexel {
	float count;
	float size;
	float probes;
};

ProbeTexel cascadeProbeTexel(float cascadeIndex) {
	float count = in_CascadeAngular * pow(4.0, cascadeIndex);
	float size = sqrt(count);
	float probes = in_CascadeExtent / size;
	return ProbeTexel(count, size, probes);
}*/

// We fetch radiance intervals within the cascade by angle (thetaIndex) and probe (texelIndex).
vec4 cascadeFetch(float probeSize, vec2 texelIndex, float rayIndex) {
	vec2 probeTexel = texelIndex * probeSize;
	probeTexel += vec2(mod(rayIndex, probeSize), (rayIndex / probeSize));  //DO THE SAME BUT FOR CASCADES INSTEAD OF RAYS ?
	vec2 cascadeTexelPosition = probeTexel / cascadeStorageSize;
	return texture2D(mergedCascades, cascadeTexelPosition);
}

void main() 
{

    vec2 sceneCoord = TexCoords * sceneDimensions;
    float c0ProbeCount = ceil(raymarchRegionSize / c0Spacing);


    vec2 probePos = floor(sceneCoord/(c0Spacing)); //coord of the closest probe
    //mod(floor(sceneCoord), c0Spacing) // coords inside the probe (maybe sum + 1 and this gives offsets into closest probes ??)

    //vec2 probePos = floor(sceneCoord/(c0Spacing)); //coord of the closest probe

    //mod(probePos, 2.0) //loops probes between 0 and 1

    //probePos/(c0ProbeCount - 1) probe uvs
    
    
    //FragColor = vec4(probePos, 0.0, 1.0);
    


    vec4 radiance = vec4(0.0, 0.0, 0.0, 0.0);
	for(float i = 0.0; i < 4.0; i ++)
		// cascadeFetch uses the probe's cell index, which is the same as the mipmap's pixel position.
		radiance += cascadeFetch(2.0, probePos, i);


    FragColor = vec4(radiance.rgb / 4.0, 1.0);



    //maybe just do simple and apply hardware filter ?

    /*
	// Get the mipmap's cascade texel info based on the cascade being rendered.
	ProbeTexel probeInfo = cascadeProbeTexel(in_CascadeIndex);
	vec2 mipmapCoord = vec2(in_TextCoord * in_MipMapExtent);
	
	// Loops through all of the radiance intervals for this mip-map and accumulate.
	vec4 radiance = vec4(0.0, 0.0, 0.0, 0.0);
	for(float i = 0.0; i < probeInfo.count; i ++)
		// cascadeFetch uses the probe's cell index, which is the same as the mipmap's pixel position.
		radiance += cascadeFetch(probeInfo, mipmapCoord, i);
	
	FragColor = vec4(radiance.rgb / probeInfo.count, 1.0);*/
}