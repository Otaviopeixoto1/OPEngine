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




out vec4 FragColor;

in vec2 TexCoords;
in vec3 sNormal;
in mat4 ViewMatrix;
in vec3 sFragPos; 


uniform sampler2D texture_diffuse1;
uniform sampler2D texture_specular1;






vec4 CalcDirLight(DirLight light, vec3 normal, vec3 viewDir)
{
    vec4 ld = normalize(vec4(light.direction.xyz, 0.0));
    vec4 lightDir = normalize(ViewMatrix * ld);
    vec4 diffuse = max(dot(normal,lightDir.xyz), 0.0) * light.lightColor;

    vec3 reflectDir = reflect(-lightDir.xyz, normal);  
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);

    vec4 specular = (0.5 * spec * light.lightColor);  

    // if it has specular map:
    //vec4 specular = (0.5 * spec * light.lightColor) * specMap;  

    return (diffuse + specular);
}

vec4 CalcPointLight(PointLight light, vec3 normal, vec3 fragPos)
{
    vec3 lightPos = vec3(ViewMatrix * light.position);
    vec3 viewDir = -normalize(fragPos);
    vec3 lightDir = normalize(lightPos - fragPos);

    // diffuse shading
    float diff = max(dot(normal, lightDir), 0.0);

    // specular shading
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);

    // attenuation
    float distance    = length(lightPos - fragPos);
    float attenuation = 1.0 / (light.constant + light.linear * distance + 
  			     light.quadratic * (distance * distance));    


    vec4 diffuse  = light.lightColor * diff;
    vec4 specular = light.lightColor * spec;
    diffuse  *= attenuation;
    specular *= attenuation;
    return (diffuse + specular);
} 



void main()
{    
    

    vec4 albedo = texture(texture_diffuse1, TexCoords);
    vec4 specMap = texture(texture_specular1, TexCoords);

    vec4 outFrag = vec4(ambientLight.xyz * ambientLight.w,1.0) * albedo;

    vec3 norm = normalize(sNormal);
    for(int i = 0; i < numDirLights; i++)
    {
        vec3 viewDir = -normalize(sFragPos);
        outFrag += albedo * CalcDirLight(dirLights[i], norm, viewDir);
    }
    for(int i = 0; i < numPointLights; i++)
    {
        outFrag += albedo * CalcPointLight(pointLights[i], norm, sFragPos);
    }

    //FragColor = texture(texture_diffuse1, TexCoords) * dirLights[1].lightColor;
    FragColor = outFrag;

}