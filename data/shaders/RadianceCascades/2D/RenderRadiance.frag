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

vec4 CascadeFetch(float probeSize, vec2 texelIndex, float rayIndex) 
{
	vec2 probeTexel = texelIndex * probeSize;
	probeTexel += vec2(mod(rayIndex, probeSize), (rayIndex / probeSize));  //DO THE SAME BUT FOR CASCADES INSTEAD OF RAYS ?
	vec2 cascadeTexelPosition = probeTexel / cascadeStorageSize;
	return texture2D(mergedCascades, cascadeTexelPosition);
}

void main() 
{
    vec2 sceneCoord = TexCoords * sceneDimensions;

	float rayCount = c0Angular;
	float probeSize = sqrt(rayCount);

    float c0ProbeCount = ceil(raymarchRegionSize / c0Spacing);

	vec2 probePosBL = floor(sceneCoord/(c0Spacing) - 0.5);
    vec2 probePos = (sceneCoord/(c0Spacing) - 0.5) ; 

    vec4 radianceTL = vec4(0.0), radianceTR = vec4(0.0), radianceBL = vec4(0.0), radianceBR = vec4(0.0);


	radianceBL += CascadeFetch(probeSize, probePosBL + vec2(0.0,0.0), 0);
	radianceBL += CascadeFetch(probeSize, probePosBL + vec2(0.0,0.0), 1);
	radianceBL += CascadeFetch(probeSize, probePosBL + vec2(0.0,0.0), 2);
	radianceBL += CascadeFetch(probeSize, probePosBL + vec2(0.0,0.0), 3);
	radianceBL /= 4.0;

	radianceBR += CascadeFetch(probeSize, probePosBL + vec2(1.0,0.0), 0);
	radianceBR += CascadeFetch(probeSize, probePosBL + vec2(1.0,0.0), 1);
	radianceBR += CascadeFetch(probeSize, probePosBL + vec2(1.0,0.0), 2);
	radianceBR += CascadeFetch(probeSize, probePosBL + vec2(1.0,0.0), 3);
	radianceBR /= 4.0;

	radianceTL += CascadeFetch(probeSize, probePosBL + vec2(0.0,1.0), 0);
	radianceTL += CascadeFetch(probeSize, probePosBL + vec2(0.0,1.0), 1);
	radianceTL += CascadeFetch(probeSize, probePosBL + vec2(0.0,1.0), 2);
	radianceTL += CascadeFetch(probeSize, probePosBL + vec2(0.0,1.0), 3);
	radianceTL /= 4.0;

	radianceTR += CascadeFetch(probeSize, probePosBL + vec2(1.0,1.0), 0);
	radianceTR += CascadeFetch(probeSize, probePosBL + vec2(1.0,1.0), 1);
	radianceTR += CascadeFetch(probeSize, probePosBL + vec2(1.0,1.0), 2);
	radianceTR += CascadeFetch(probeSize, probePosBL + vec2(1.0,1.0), 3);
	radianceTR /= 4.0;

	vec2 weight = (probePos - probePosBL);

	vec4 integratedRadiance = mix(mix(radianceBL, radianceBR, weight.x), mix(radianceTL, radianceTR, weight.x), weight.y);

    FragColor = vec4(integratedRadiance.rgb , 1.0);


}