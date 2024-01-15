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



//http://www.thetenthplanet.de/archives/1180
mat3 cotangent_frame( vec3 N, vec3 p, vec2 uv ) { 
    // get edge vectors of the pixel triangle 
    vec3 dp1 = dFdx( p ); 
    vec3 dp2 = dFdy( p ); 
    vec2 duv1 = dFdx( uv ); 
    vec2 duv2 = dFdy( uv );   

    // solve the linear system
    vec3 dp2perp = cross( dp2, N ); 
    vec3 dp1perp = cross( N, dp1 ); 
    vec3 T = dp2perp * duv1.x + dp1perp * duv2.x; 
    vec3 B = dp2perp * duv1.y + dp1perp * duv2.y;   

    // construct a scale-invariant frame but preserve the relative difference between T and B
    float invmax = inversesqrt( max( dot(T,T), dot(B,B) ) );
    return mat3( T * invmax, B * invmax, N ); 

    // construct a scale-invariant frame by normalizing T aand B
    //return mat3( normalize(T), normalize(B), N );
}

vec3 perturb_normal( vec3 N, vec3 V, vec2 texcoord ) {
    // assume N, the interpolated vertex normal and 
    // V, the view vector (vertex to eye) 
    vec3 map = texture2D( texture_normal1, texcoord ).xyz; 
    map.y = -map.y;
    mat3 TBN = cotangent_frame( N, -V, texcoord ); 

    return normalize( TBN * map ); 
}




vec3 CalcBumpedNormal()
{
    vec3 Normal = normalize(ViewNormal);
    vec3 Tangent = normalize(ViewTangent);
    Tangent = normalize(Tangent - dot(Tangent, Normal) * Normal);
    vec3 Bitangent = normalize(cross(Tangent, Normal));
    vec3 BumpMapNormal = texture(texture_normal1, TexCoords).xyz;
    BumpMapNormal = 2.0 * BumpMapNormal - 1.0;
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
        //vec3 norm = CalcBumpedNormal();
        vec3 norm = perturb_normal(normalize(ViewNormal), -ViewFragPos, TexCoords);
        //vec3 norm = normalize(ViewNormal);
    #else
        vec3 norm = normalize(ViewNormal);
    #endif

    //calculate world norm and world pos in vertex shader as well ???
    vec3 worldNorm = (inverseViewMatrix * vec4(norm, 0.0)).xyz;
    vec4 worldPos = inverseViewMatrix * vec4(ViewFragPos,1);

    for(int i = 0; i < numDirLights; i++)
    {
        vec3 viewDir = -normalize(ViewFragPos);
        vec4 lighting = albedo * CalcDirLight(i, norm, viewDir, specular.xyz, specular.w);
        outFrag += lighting * GetDirLightShadow(i, ViewFragPos.xyz, worldPos.xyz, worldNorm);
    }
    for(int i = 0; i < numPointLights; i++)
    {
        outFrag +=  albedo * CalcPointLight(i, norm, ViewFragPos, specular.xyz, specular.w);
    }

    FragColor = outFrag;
    //FragColor = vec4(norm,1.0);
    //FragColor = vec4(TexCoords,0.0,1.0);
}