#version 330 core

// FXAA 3.11. Algorithm adapted from Timothy Lottes: https://developer.download.nvidia.com/assets/gamedev/files/sdk/11/FXAA_WhitePaper.pdf

out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D screenTexture;
uniform vec2 pixelSize;

// Trims the algorithm from processing darks.
//   0.0833 - upper limit (default, the start of visible unfiltered edges)
//   0.0625 - high quality (faster)
//   0.0312 - visible limit (slower)
uniform float contrastThreshold;

// The minimum amount of local contrast required to apply algorithm.
//   0.333 - too little (faster)
//   0.250 - low quality
//   0.166 - default
//   0.125 - high quality 
//   0.063 - overkill (slower)
uniform float brightnessThreshold;


float SampleLuminance (vec2 uv, float uOffset, float vOffset) 
{
    uv += pixelSize * vec2(uOffset, vOffset);
    return texture(screenTexture, uv).a;
}

struct NeighborLuminance 
{
    //"cross" neighbors
    float c, t, b, r, l;
    //diagonal neighbors
    float tl, tr, bl, br;
};

NeighborLuminance SampleLuminanceNeighborhood (vec2 uv) 
{
    NeighborLuminance l;
    l.c = SampleLuminance(uv, 0, 0);
    l.t = SampleLuminance(uv, 0,  1);
    l.b = SampleLuminance(uv, 0, -1);
    l.r = SampleLuminance(uv, 1,  0);
    l.l = SampleLuminance(uv,-1,  0);

    l.tr = SampleLuminance(uv,  1,  1);
    l.tl = SampleLuminance(uv, -1,  1);
    l.br = SampleLuminance(uv,  1, -1);
    l.bl = SampleLuminance(uv, -1, -1);
    return l;
}

// Get pixel blend using kernel:
// 1 2 1
// 2 0 2
// 1 2 1

float GetPixelBlend(NeighborLuminance l, float contrast)
{
    float blend = 2 * (l.t + l.r + l.b + l.l);
    blend += l.tr + l.tl + l.br + l.bl;
    blend *= 1.0 / 12;
    blend = abs(blend - l.c);
    blend = clamp(blend / contrast, 0, 1);
    blend = smoothstep(0, 1, blend);
    return blend * blend;
}

void main()
{    
    NeighborLuminance l = SampleLuminanceNeighborhood(TexCoords);
    float highest = max(max(max(max(l.t, l.r), l.b), l.l), l.c);
	float lowest = min(min(min(min(l.t, l.r), l.b), l.l), l.c);
	float contrast = highest - lowest;
    float threshold = max(contrastThreshold, brightnessThreshold * highest);

    //if(contrast - threshold < 0) discard; //return l.c

    float pixelBlend = GetPixelBlend(l, contrast);

    // Determining if the pixel is in a horizontal or vertical edge
    float horizontal =
        abs(l.t + l.b - 2 * l.c) * 2 +
        abs(l.tr + l.br - 2 * l.r) +
        abs(l.tl + l.bl - 2 * l.l);

    float vertical =
        abs(l.r + l.l - 2 * l.c) * 2 +
        abs(l.tr + l.tl - 2 * l.t) +
        abs(l.br + l.bl - 2 * l.b);

    bool isHorizontal = horizontal >= vertical;

    float pixelStep = isHorizontal ? pixelSize.y : pixelSize.x;
    
    // after determining if the edge is horizontal or vertical, we need know 
    // if its in the positive or negative direction:
    float pLuminance = isHorizontal ? l.t : l.r;
	float nLuminance = isHorizontal ? l.b : l.l;
    float pGradient = abs(pLuminance - l.c);
	float nGradient = abs(nLuminance - l.c);

    float oppositeLuminance;
	float gradient;

    if (pGradient < nGradient)
    {
        pixelStep = -pixelStep;
        oppositeLuminance = nLuminance;
        gradient = nGradient;
    }
    else
    {
        oppositeLuminance = pLuminance;
        gradient = pGradient;
    }
    

    // Adding blending on the edges
    // ----------------------------
    vec2 uvEdge = TexCoords;
    vec2 edgeStep;
    if (isHorizontal) 
    {
        uvEdge.y += pixelStep * 0.5;
        edgeStep = vec2(pixelSize.x, 0);
    }
    else 
    {
        uvEdge.x += pixelStep * 0.5;
        edgeStep = vec2(0, pixelSize.y);
    }
    float edgeLuminance = (l.c + oppositeLuminance) * 0.5;
    float gradientThreshold = gradient * 0.25;
    
    // positive direction of the edge
    vec2 puv = uvEdge + edgeStep;
    float pLuminanceDelta = SampleLuminance(puv,0,0) - edgeLuminance;
    bool pAtEnd = abs(pLuminanceDelta) >= gradientThreshold;

    for (int i = 0; i < 16 && !pAtEnd; i++) 
    {
        puv += edgeStep * 2;
        pLuminanceDelta = SampleLuminance(puv,0,0) - edgeLuminance;
        pAtEnd = abs(pLuminanceDelta) >= gradientThreshold;
    }

    // negative dircetion of the edge
    vec2 nuv = uvEdge - edgeStep;
    float nLuminanceDelta = SampleLuminance(nuv, 0, 0) - edgeLuminance;
    bool nAtEnd = abs(nLuminanceDelta) >= gradientThreshold;

    for (int i = 0; i < 16 && !nAtEnd; i++) 
    {
        nuv -= edgeStep * 2;
        nLuminanceDelta = SampleLuminance(nuv, 0, 0) - edgeLuminance;
        nAtEnd = abs(nLuminanceDelta) >= gradientThreshold;
    }

    float pDistance, nDistance;
    if (isHorizontal) 
    {
        pDistance = puv.x - TexCoords.x;
        nDistance = TexCoords.x - nuv.x;
    }
    else 
    {
        pDistance = puv.y - TexCoords.y;
        nDistance = TexCoords.y - nuv.y;
    }
    
    float shortestDistance;
    bool deltaSign;
    if (pDistance <= nDistance) 
    {
        shortestDistance = pDistance;
        deltaSign = pLuminanceDelta >= 0;
    }
    else 
    {
        shortestDistance = nDistance;
        deltaSign = nLuminanceDelta >= 0;
    }

     
    bool blendPixel = deltaSign == (l.c - edgeLuminance >= 0);
    float edgeblendFactor = blendPixel ? 0 : 0.5 - shortestDistance / (pDistance + nDistance);

    float finalBlend = max(pixelBlend, edgeblendFactor);


    vec2 uv = TexCoords;
    if (isHorizontal) 
    {
        uv.y += pixelStep * finalBlend;
    }
    else 
    {
        uv.x += pixelStep * finalBlend;
    }


    FragColor =  vec4(texture(screenTexture, uv).rgb, l.c);

    

}