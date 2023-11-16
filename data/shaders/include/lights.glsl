
#ifndef MAX_DIR_LIGHTS
#define MAX_DIR_LIGHTS 5
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

    float radius;
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
    float NdotL = dot(normal,lightDir);
    float diff = max(NdotL, 0.0);
    

    //Phong:
    //vec3 reflectDir = reflect(-lightDir, normal);  
    //float spec = pow(max(dot(viewDir, reflectDir), 0.0), specularPower);

    //Blinn-Phong:
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(normal, halfwayDir), 0.0), specularPower);



    vec4 diffuse = diff * light.lightColor;                               // max(sign(NdotL-0.001),0.0f)
    vec4 specular = (spec * light.lightColor) * vec4(specularStrength, 0.0) * smoothstep(-0.1,0.1,NdotL);  

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
    float NdotL = dot(normal,lightDir);
    float diff = max(NdotL, 0.0);

    // specular shading

    //Phong:
    //vec3 reflectDir = reflect(-lightDir, normal);
    //float spec = pow(max(dot(viewDir, reflectDir), 0.0), specularPower);

    //Blinn-Phong:
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(normal, halfwayDir), 0.0), specularPower) *  smoothstep(-0.1,0.1,NdotL);

    

    // attenuation
    float dist    = length(lightPos - fragPos);
    float attenuation = 1.0 / (light.constant + light.linear * dist + 
  			            light.quadratic * (dist * dist));    


    vec4 diffuse  = light.lightColor * diff;
    vec4 specular = light.lightColor * spec * vec4(specularStrength, 0.0);
    diffuse  *= attenuation;
    specular *= attenuation;
    return (diffuse + specular);
} 