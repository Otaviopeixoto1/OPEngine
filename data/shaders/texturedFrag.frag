#version 330 core


// MOVE ALL LIGHT STRUCTS AND CALCULATIONS TO A HEADER FILE

#define MAX_DIR_LIGHTS 3
#define MAX_POINT_LIGHTS 5

struct DirLight {
    vec4 lightColor; 
    mat4 lightMatrix;
    vec4 direction; 
}; 

struct PointLight {
    vec3 position; 

    vec3 Ka; 
    vec3 Kd; 
    vec3 Ks;

    float constant;
    float linear;
    float quadratic;
    
}; 








out vec4 FragColor;
in vec2 TexCoords;
in vec3 Normal;

uniform sampler2D texture_diffuse1;
uniform sampler2D texture_specular1;
uniform vec4 lightColor;


layout(std140) uniform Lights
{
    vec4 ambientLight; //(vec3 color, float intensity)
    int numDirLights;
    int pad1;
    int pad2;
    int pad3;
    DirLight dirLights[MAX_DIR_LIGHTS]; 
}; 


void main()
{    
    

    vec4 albedo = texture(texture_diffuse1, TexCoords);
    vec4 outFrag = vec4(ambientLight.xyz * ambientLight.w,1.0) * albedo;

    vec3 norm = normalize(Normal);
    for(int i = 0; i < numDirLights; i++)
    {
        vec3 lDir = normalize(dirLights[i].direction.xyz);
        vec4 diffuse = max(dot(norm,lDir), 0.0) * dirLights[i].lightColor;
        outFrag += albedo * diffuse;
    }
    //FragColor = texture(texture_diffuse1, TexCoords) * dirLights[1].lightColor;
    FragColor = outFrag;

}