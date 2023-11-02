#version 330 core

#include "lights.glsl"


out vec4 FragColor;

in vec2 TexCoords;
in vec3 sNormal;
in vec3 sFragPos; 


uniform sampler2D texture_diffuse1;
uniform sampler2D texture_specular1;

layout (std140) uniform MaterialProperties
{
    vec4 albedoColor;
    vec4 emissiveColor;
    vec4 specular;
};




void main()
{    
    vec4 albedo = texture(texture_diffuse1, TexCoords);
    vec4 specMap = texture(texture_specular1, TexCoords);


    vec4 outFrag = vec4(ambientLight.xyz * ambientLight.w,1.0) * albedo;

    vec3 norm = normalize(sNormal);
    for(int i = 0; i < numDirLights; i++)
    {
        vec3 viewDir = -normalize(sFragPos);
        outFrag += albedo * CalcDirLight(dirLights[i], norm, viewDir, specular.xyz, specular.w);
    }
    for(int i = 0; i < numPointLights; i++)
    {
        outFrag +=  albedo * CalcPointLight(pointLights[i], norm, sFragPos, specular.xyz, specular.w);
    }

    FragColor = outFrag;

}