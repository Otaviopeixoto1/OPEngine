#version 330 core

#include "lights.glsl"


out vec4 FragColor;

in vec2 TexCoords;
in vec3 ViewNormal;
in vec3 ViewFragPos; 


uniform sampler2D texture_diffuse1;
uniform sampler2D texture_specular1;


layout (std140) uniform GlobalMatrices
{
    mat4 projectionMatrix;
    mat4 viewMatrix;
    mat4 inverseViewMatrix;
};

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

    vec3 norm = normalize(ViewNormal);
    vec3 worldNorm = (inverseViewMatrix * vec4(norm, 0.0)).xyz;
    vec4 worldPos = inverseViewMatrix * vec4(ViewFragPos,1);

    for(int i = 0; i < numDirLights; i++)
    {
        vec3 viewDir = -normalize(ViewFragPos);
        vec4 lighting = albedo * CalcDirLight(i, norm, viewDir, specular.xyz, specular.w);
        outFrag += lighting * GetDirLightShadow(i, worldPos, worldNorm);
    }
    for(int i = 0; i < numPointLights; i++)
    {
        outFrag +=  albedo * CalcPointLight(i, norm, ViewFragPos, specular.xyz, specular.w);
    }

    FragColor = outFrag;

}