#version 330 core
//Based on https://github.com/Yaazarai/GMShaders-Radiance-Cascades/tree/main

out vec4 FragColor;
in vec2 TexCoords;

uniform sampler2D sdf; // Input Distance Field.

uniform vec2 sceneDimensions;
uniform float raymarchRegionSize;
uniform float cascadeStorageSize; 

uniform float c0Spacing;  // Cascade 0 probePos spacing.
uniform float c0Interval; // Cascade 0 radiance interval.
uniform float c0Angular;  // Cascade angular resolution.
uniform int cascadeIndex; 

#define PI 3.141592



vec4 MarchInterval(float rayCount, vec2 probePos, vec2 spacing, float rayIndex, float minimum, float range) 
{
	vec2 probeUVPos = vec2((probePos+0.5) * spacing) / sceneDimensions;// multiply by the inverse viewport sizes (convert to global uvs)
	//           world coords                                      convert to uvs (0 .. 1)
	
	float theta = 2.0 * PI * ((rayIndex + 0.5) / rayCount);
	vec2 dir = vec2(cos(theta), -sin(theta));
	vec2 start = probeUVPos + ((dir * minimum) / raymarchRegionSize);
	

	for(float ii = 0.0, dd = 0.0, rd = 0.0, rt = range / raymarchRegionSize; ii < range; ii++) {
		vec2 ray = start + dir * min(rd, rt);
        vec4 data = texture2D(sdf, ray).rgba;
		rd += dd = data.r;
		
		if (rd >= rt || ray.x < 0.0 || ray.y < 0.0 || ray.x >= 1.0 || ray.y >= 1.0) return vec4(0.0, 0.0, 0.0, 0.0);
		
		if (dd < 0.0001) return vec4(data.gba, 1.0);
	}
	
	return vec4(0.0, 0.0, 0.0, 0.0);
}

void main() 
{
	vec2 texel = TexCoords * vec2(cascadeStorageSize); 

	float rayCount = c0Angular * float(1 << (2 * cascadeIndex));
	float probeSize = sqrt(rayCount);
	vec2 probePos = floor(texel / vec2(probeSize)); 
	vec2 spacing = vec2(c0Spacing * float(1 << cascadeIndex));
	
	vec2  probeTexel = mod(floor(texel), vec2(probeSize));
	float rayIndex = (probeTexel.y * probeSize) + probeTexel.x;

	/*
	FragColor = vec4(probeTexel/(probeSize - 1), 0.0, 1.0);
	*/
	
	float minimum = (c0Interval  * (1.0 - float(1 << (2 * cascadeIndex)))) / (1.0 - 4.0);
	float range = c0Interval * float(1 << (2 * cascadeIndex));
	float maximum = minimum + range;
	
	
	// Forces overlap between N and N-1 radiance intervals. 
	// Fixes light leak and some ringing artifacts.
	minimum -= range / 4.0 * sign(cascadeIndex - 1);
	range = maximum - minimum;

	FragColor = MarchInterval(rayCount, probePos, spacing, rayIndex, minimum, range);

}