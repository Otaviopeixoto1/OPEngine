#version 330 core

#include "lights.glsl"


out vec4 FragColor;

in vec2 TexCoords;
in vec3 ViewNormal;
in vec3 ViewFragPos; 


layout (std140) uniform MaterialProperties
{
    vec4 albedoColor;
    vec4 emissiveColor;
    vec4 specular;
};



void main()
{    
    vec4 outFrag = vec4(ambientLight.xyz * ambientLight.w,1.0) * albedoColor;

    vec3 norm = normalize(ViewNormal);
    for(int i = 0; i < numDirLights; i++)
    {
        vec3 viewDir = -normalize(ViewFragPos);
        outFrag += albedoColor * CalcDirLight(dirLights[i], norm, viewDir, specular.xyz, specular.w);
    }
    for(int i = 0; i < numPointLights; i++)
    {
        outFrag +=  albedoColor * CalcPointLight(pointLights[i], norm, ViewFragPos, specular.xyz, specular.w);
    }

    //Tonemapping:
    //outFrag = vec4(vec3(1.0) - exp(-outFrag.xyz * 1.0f),1.0);
    FragColor = outFrag;

}