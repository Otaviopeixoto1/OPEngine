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


vec4 CalcDirLight(DirLight light, vec3 normal, vec3 viewDir, vec3 specularStrength, float specularPower)
{
    vec3 lightDir = normalize(light.direction.xyz);
    vec4 diffuse = max(dot(normal,lightDir), 0.0) * light.lightColor;

    vec3 reflectDir = reflect(-lightDir, normal);  
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), specularPower);

    vec4 specular = ( spec * light.lightColor) * vec4(specularStrength, 0.0);  

    // if it has specular map:
    //vec4 specular = (0.5 * spec * light.lightColor) * specMap;  

    return (diffuse + specular);
}

vec4 CalcPointLight(PointLight light, vec3 normal, vec3 fragPos, vec3 specularStrength, float specularPower)
{
    vec3 lightPos = light.position.xyz;
    vec3 viewDir  = -normalize(fragPos);
    vec3 lightDir = normalize(lightPos - fragPos);

    // diffuse shading
    float diff = max(dot(normal, lightDir), 0.0);

    // specular shading
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), specularPower);

    // attenuation
    float distance    = length(lightPos - fragPos);
    float attenuation = 1.0 / (light.constant + light.linear * distance + 
  			     light.quadratic * (distance * distance));    


    vec4 diffuse  = light.lightColor * diff;
    vec4 specular = light.lightColor * spec * vec4(specularStrength, 0.0);
    diffuse  *= attenuation;
    specular *= attenuation;
    return (diffuse + specular);
} 









uniform vec3 albedo;

out vec4 FragColor;

in vec2 TexCoords;
in vec3 sNormal;
in vec3 sFragPos; 


layout (std140) uniform MaterialProperties
{
    vec4 albedoColor;
    vec4 emissiveColor;
    vec4 specular;
};



void main()
{    
    vec4 outFrag = vec4(ambientLight.xyz * ambientLight.w,1.0) * albedoColor;

    vec3 norm = normalize(sNormal);
    for(int i = 0; i < numDirLights; i++)
    {
        vec3 viewDir = -normalize(sFragPos);
        outFrag += albedoColor * CalcDirLight(dirLights[i], norm, viewDir, specular.xyz, specular.w);
    }
    for(int i = 0; i < numPointLights; i++)
    {
        outFrag +=  albedoColor * CalcPointLight(pointLights[i], norm, sFragPos, specular.xyz, specular.w);
    }

    FragColor = outFrag;

}