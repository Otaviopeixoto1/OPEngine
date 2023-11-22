
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


uniform sampler2D shadowMap0;

float GetDirLightShadow(int lightIndex, vec4 worldPos, vec3 worldNormal)
{
    #ifndef DIR_LIGHT_SHADOWS
        return 1;
    #endif

    //float bias = max(0.05 * (1.0 - dot(worldNormal, dirLights[lightIndex].direction.xyz)), 0.005);
    float bias = 0.001;
    vec2 texelSize = 1.0 / textureSize(shadowMap0, 0);  

    vec3 normalBias = worldNormal * max(texelSize.x,texelSize.y) * 1.4142136f * 2;




    vec4 posClipSpace = dirLights[lightIndex].lightMatrix * vec4(worldPos.xyz + normalBias, 1);

    // Perspective division is only necessary on perspective projection, but doesnt affect ortographic projection:
    vec3 ndcPos = posClipSpace.xyz / posClipSpace.w;
    ndcPos = ndcPos * 0.5 + 0.5; 

    // Depth samples to compare   
    float currentDepth = ndcPos.z; 

    

    
    // One sample:
    //float closestDepth = texture(shadowMap0, ndcPos.xy).r;
    //float shadow = currentDepth - bias > closestDepth  ? 0.0 : 1.0; 

    
  
    
    // Multiple samples:
    float shadow = 0.0;
    for(int x = -1; x <= 1; ++x)
    {
        for(int y = -1; y <= 1; ++y)
        {
            float pcfDepth = texture(shadowMap0, ndcPos.xy + vec2(x, y) * texelSize).r; 
            shadow += currentDepth - bias > pcfDepth ? 0.0 : 1.0;        
        }    
    }
    shadow /= 9.0;

    

    return  shadow;
}



vec4 CalcDirLight(int lightIndex, vec3 normal, vec3 viewDir, vec3 specularStrength, float specularPower)
{
    DirLight light = dirLights[lightIndex];
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


vec4 CalcPointLight(int lightIndex, vec3 normal, vec3 fragPos, vec3 specularStrength, float specularPower)
{
    PointLight light = pointLights[lightIndex];
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
    return (diffuse + specular) * attenuation;
} 



