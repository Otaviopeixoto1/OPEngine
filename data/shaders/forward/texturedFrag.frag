#version 330 core

#include "lights.glsl"


out vec4 FragColor;

in vec2 TexCoords;
in vec3 ViewNormal;
in vec3 ViewTangent;
in vec3 ViewFragPos; 


uniform sampler2D texture_diffuse1;
uniform sampler2D texture_specular1;
uniform sampler2D texture_normal1;


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

vec3 CalcBumpedNormal()
{
    vec3 Normal = normalize(ViewNormal);
    vec3 Tangent = normalize(ViewTangent);
    Tangent = normalize(Tangent - dot(Tangent, Normal) * Normal);
    vec3 Bitangent = cross(Tangent, Normal);
    vec3 BumpMapNormal = texture(texture_normal1, TexCoords).xyz;
    BumpMapNormal = 2.0 * BumpMapNormal - vec3(1.0, 1.0, 1.0);
    vec3 NewNormal;
    mat3 TBN = mat3(Tangent, Bitangent, Normal);
    NewNormal = TBN * BumpMapNormal;
    NewNormal = normalize(NewNormal);
    return NewNormal;
}


void main()
{    
    vec4 albedo = texture(texture_diffuse1, TexCoords);
    vec4 specMap = texture(texture_specular1, TexCoords);


    vec4 outFrag = vec4(ambientLight.xyz * ambientLight.w,1.0) * albedo;

    #ifdef NORMAL_MAPPED
        vec3 norm = CalcBumpedNormal();
    #else
        vec3 norm = normalize(ViewNormal);
    #endif

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