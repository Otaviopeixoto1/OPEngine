#version 440 core

layout (location = 0) out vec4 gAlbedoSpec;
layout (location = 1) out vec4 gNormal;
layout (location = 2) out vec4 gPosition;

uniform sampler2D texture_diffuse1;
uniform sampler2D texture_specular1;
uniform sampler2D texture_normal1;

in vec2 TexCoords;
in vec3 ViewFragPos;
in vec3 ViewTangent;
in vec3 ViewNormal;


layout (std140) uniform MaterialProperties
{
    vec4 albedoColor;
    vec4 emissiveColor;
    vec4 specular;
};

//We define 2 sampling methods: one for when the model is textured and the other for untextured models. This allows the same shader program to be used
//for both cases, instead of having to build and manage 2 different programs
subroutine vec4 GetColor();

layout(index = 0) subroutine(GetColor) 
vec4 AlbedoColor() {return albedoColor;}

layout(index = 1) subroutine(GetColor)
vec4 TextureColor() {return vec4(texture(texture_diffuse1, TexCoords).rgb, 1.0f);}

layout(location = 0) subroutine uniform GetColor SampleColor;



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

//http://www.thetenthplanet.de/archives/1180
mat3 cotangent_frame( vec3 N, vec3 p, vec2 uv ) 
{ 
    // get edge vectors of the pixel triangle 
    vec3 dp1 = dFdx(p); 
    vec3 dp2 = dFdy(p); 
    vec2 duv1 = dFdx(uv); 
    vec2 duv2 = dFdy(uv);   

    // solve the linear system
    vec3 dp2perp = cross(dp2, N); 
    vec3 dp1perp = cross(N, dp1); 
    vec3 T = dp2perp * duv1.x + dp1perp * duv2.x; 
    vec3 B = dp2perp * duv1.y + dp1perp * duv2.y;   

    // construct a scale-invariant frame but preserve the relative difference between T and B
    float invmax = inversesqrt(max(dot(T,T), dot(B,B)));
    return mat3(T * invmax, B * invmax, N); 

    // construct a scale-invariant frame by normalizing T and B
    //return mat3( normalize(T), normalize(B), N );
}

vec3 perturb_normal(vec3 N, vec3 V, vec2 texcoord) 
{
    // assume N, the interpolated vertex normal and 
    // V, the view vector (vertex to eye) 
    vec3 map = texture2D( texture_normal1, texcoord ).xyz; 
    map = 2.0 * map - 1.0; 
    map.y = -map.y;

    mat3 TBN = cotangent_frame( N, -V, texcoord ); 

    return normalize( TBN * map ); 
}

void main()
{    
    gPosition = vec4(ViewFragPos,1.0);
    
    #ifdef NORMAL_MAPPED
        gNormal = vec4(perturb_normal(normalize(ViewNormal), -ViewFragPos, TexCoords),0.0);
    #else
        gNormal = vec4(normalize(ViewNormal),0.0);
    #endif
    
    gAlbedoSpec.rgb = SampleColor().rgb;
    gAlbedoSpec.a = specular.a;
}  