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
    vec4 position; 
    vec4 lightColor;

    float constant;
    float linear;
    float quadratic;



    float pad;
    
}; 


layout(std140) uniform Lights
{
    vec4 ambientLight; //(vec3 color, float intensity)
    int numDirLights;
    int numPointLights;
    int pad2;
    int pad3;
    DirLight dirLights[MAX_DIR_LIGHTS]; 
    PointLight pointLights[MAX_POINT_LIGHTS];
}; 




uniform vec3 albedo;

out vec4 FragColor;

in vec2 TexCoords;
in vec3 sNormal;
in mat4 ViewMatrix;
in vec3 sFragPos; 










void main()
{    
    

    vec4 outFrag = vec4(ambientLight.xyz * ambientLight.w,1.0) * vec4(albedo,1.0);

    vec3 norm = normalize(sNormal);
    for(int i = 0; i < numDirLights; i++)
    {
        vec4 ld = normalize(vec4(dirLights[i].direction.xyz, 0.0));
        vec4 lightDir = normalize(ViewMatrix * ld);
        vec4 diffuse = max(dot(norm,lightDir.xyz), 0.0) * dirLights[i].lightColor;

        vec3 viewDir = -normalize(sFragPos);
        vec3 reflectDir = reflect(-lightDir.xyz, norm);  
        float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);
        vec4 specular = 0.5 * spec * dirLights[i].lightColor;  

        outFrag += vec4(albedo,1.0) * (diffuse + specular);
    }
    //FragColor = texture(texture_diffuse1, TexCoords) * dirLights[1].lightColor;
    FragColor = outFrag;

    //FragColor = vec4(albedo,1.0)*lightColor;
    //FragColor = vec4(albedo,1.0);

}