#version 330 core

layout (location = 0) out vec4 gAlbedoSpec;
layout (location = 1) out vec4 gNormal;
layout (location = 2) out vec4 gPosition;

in vec2 TexCoords;
in vec3 ViewFragPos;
in vec3 ViewTangent;
in vec3 ViewNormal;

uniform sampler2D texture_diffuse1;
uniform sampler2D texture_specular1;
uniform sampler2D texture_normal1;

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
    gPosition = vec4(ViewFragPos,1.0);
    
    #ifdef NORMAL_MAPPED
        gNormal = vec4(CalcBumpedNormal(),0.0);
    #else
        gNormal = vec4(normalize(ViewNormal),0.0);
    #endif
    
    gAlbedoSpec.rgb = texture(texture_diffuse1, TexCoords).rgb;
    gAlbedoSpec.a = specular.a;
}  