
#ifndef MAX_DIR_LIGHTS
#define MAX_DIR_LIGHTS 3
#endif

#ifndef MAX_POINT_LIGHTS
#define MAX_POINT_LIGHTS 5
#endif

struct DirLight 
{
    vec4 lightColor; 
    mat4 lightMatrix;
    vec4 direction; 
}; 

struct PointLight 
{
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

    #ifndef LIGHT_VOLUMES
        PointLight pointLights[MAX_POINT_LIGHTS];
    #endif
}; 


vec4 CalcDirLight(DirLight light, vec3 normal, vec3 viewDir, vec3 specularStrength, float specularPower)
{
    vec3 lightDir = normalize(light.direction.xyz);
    float diff = max(dot(normal,lightDir), 0.0);
    

    //Phong:
    //vec3 reflectDir = reflect(-lightDir, normal);  
    //float spec = pow(max(dot(viewDir, reflectDir), 0.0), specularPower);

    //Blinn-Phong:
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(normal, halfwayDir), 0.0), specularPower);



    vec4 diffuse = diff * light.lightColor;
    vec4 specular = (spec * light.lightColor) * vec4(specularStrength, 0.0) * (1.0 - max(sign(-diff), 0.0));  

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

    //Phong:
    //vec3 reflectDir = reflect(-lightDir, normal);
    //float spec = pow(max(dot(viewDir, reflectDir), 0.0), specularPower);

    //Blinn-Phong:
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(normal, halfwayDir), 0.0), specularPower) * max(sign(diff),0.0f);

    

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