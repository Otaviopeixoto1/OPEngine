
// Light structs and lighting calculations (using view space data)

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




vec4 CalcDirLight(DirLight light, vec3 normal, vec3 viewDir)
{
    vec4 lightDir = vec4(normalize(light.direction.xyz), 0.0);
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
    vec3 lightPos = light.position.xyz;
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