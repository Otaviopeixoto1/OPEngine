#version 330 core

#include "lights.glsl"


uniform vec3 albedo;

out vec4 FragColor;

in vec2 TexCoords;
in vec3 sNormal;
in vec3 sFragPos; 


layout (std140) uniform MaterialProperties
{
    vec4 albedoColor;
    vec4 emissiveColor;
    vec4 specular;
};



void main()
{    
    vec4 outFrag = vec4(ambientLight.xyz * ambientLight.w,1.0) * albedoColor;

    vec3 norm = normalize(sNormal);
    for(int i = 0; i < numDirLights; i++)
    {
        vec3 viewDir = -normalize(sFragPos);
        outFrag += albedoColor * CalcDirLight(dirLights[i], norm, viewDir, specular.xyz, specular.w);
    }
    for(int i = 0; i < numPointLights; i++)
    {
        outFrag +=  albedoColor * CalcPointLight(pointLights[i], norm, sFragPos, specular.xyz, specular.w);
    }

    FragColor = outFrag;

}