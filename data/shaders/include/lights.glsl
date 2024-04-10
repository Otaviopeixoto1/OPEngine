
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

layout(std140) uniform LightData
{
    vec4 ambientLight; 
    int numDirLights;
    int numPointLights;
    int lpad2;
    int lpad3;
    DirLight dirLights[MAX_DIR_LIGHTS]; 
    PointLight pointLights[MAX_POINT_LIGHTS];
}; 



#ifndef SHADOW_CASCADE_COUNT
    #define SHADOW_CASCADE_COUNT 3
#endif

#ifndef SHADOW_MAP_COUNT
    #define SHADOW_MAP_COUNT 1
#endif

layout(std140) uniform ShadowData
{
    float shadowBias;
    float shadowSamples;
    float numLights;
    float spad3;
    mat4 lightSpaceMatrices0[4];
    vec4 frustrumDistances0; //this only allows for 4 cascades. To add more, a second vec4 of frustrum distances 
                             //must be added into this buffer
};

#if defined(PCF_SHADOWS)
    uniform sampler2DArrayShadow shadowMap0;
#elif defined(VSM_SHADOWS)
    uniform sampler2D shadowMap0; //sampler2DArray
#endif



//this output should depend on the shadow renderer:
float GetDirLightShadow(int lightIndex, vec3 viewPos, vec3 worldPos, vec3 worldNormal)
{
    #if !defined(DIR_LIGHT_SHADOWS)
        return 1;

    #elif defined(PCF_SHADOWS)

        float fragDepth = abs(viewPos.z);

        vec4 res = step(frustrumDistances0, vec4(fragDepth,fragDepth,fragDepth,fragDepth));
        int index = int(res.x + res.y + res.z); //returns a value between 0 and 3



        //use view normal instead of world normal
        //float bias = max(0.005 * (1.0 - dot(worldNormal, dirLights[lightIndex].direction.xyz)), 0.0005);
        float bias = 0.00050;
        vec2 texelSize = 1.0 / textureSize(shadowMap0, 0).xy;  

        vec3 normalBias = worldNormal * max(texelSize.x, texelSize.y) * 1.4142136f * 100;


        /*
        // calculate bias (based on depth map resolution and slope)
        vec3 normal = normalize(fs_in.Normal);
        float bias = max(0.05 * (1.0 - dot(normal, lightDir)), 0.005);
        if (layer == cascadeCount)
        {
            bias *= 1 / (farPlane * 0.5f);
        }
        else
        {
            bias *= 1 / (cascadePlaneDistances[layer] * 0.5f);
        }
        */




        vec4 posClipSpace = lightSpaceMatrices0[index] * vec4(worldPos.xyz + normalBias, 1);

        // Perspective division is only realy necessary with perspective projection, 
        // it wont affect ortographic projection be we do it anyway:
        vec3 ndcPos = posClipSpace.xyz / posClipSpace.w;
        ndcPos = ndcPos * 0.5 + 0.5; 

        // Depth samples to compare   
        float currentDepth = ndcPos.z; 

        
        // One sample:
        //float closestDepth = texture(shadowMap0, vec3(ndcPos.xy, index)).r;
        //float shadow = currentDepth - bias > closestDepth  ? 0.0 : 1.0;


        // Multiple samples (4x4 kernel):
        float shadow = 0.0f;
        for(float x = -1.5; x <= 1.5; ++x)
        {
            for(float y = -1.5; y <= 1.5; ++y)
            {
                //float pcfDepth = texture(shadowMap0, vec3(ndcPos.xy + vec2(x, y) * texelSize, index) ).r; 
                //shadow += currentDepth - bias > pcfDepth ? 0.0 : 1.0; 
                
                //for hardware pcf       
                shadow += texture(shadowMap0, vec4(ndcPos.xy + vec2(x, y) * texelSize, index, currentDepth - bias));
            }    
        }
        shadow /= 16.0f;

        return shadow;

    #elif defined(VSM_SHADOWS)

        vec4 posClipSpace = lightSpaceMatrices0[0] * vec4(worldPos.xyz, 1);

        // Perspective division is only realy necessary with perspective projection, 
        // it wont affect ortographic projection be we do it anyway:
        vec3 ndcPos = posClipSpace.xyz / posClipSpace.w;
        ndcPos = ndcPos * 0.5 + 0.5; 

        // Depth samples to compare   
        float currentDepth = ndcPos.z; 

        vec2 moments = texture2D(shadowMap0, ndcPos.xy).xy;

        // One-tailed inequality valid if currentDepth > moments.x    
        float p = float(currentDepth <= moments.x);   
        // Compute variance:

        //add as shader parameter
        float g_MinVariance = 0.01f;


        float variance = moments.y - (moments.x*moments.x);   
        variance = max(variance, g_MinVariance);   
        // Compute probabilistic upper bound.    
        float d = currentDepth - moments.x;   
        float p_max = variance / (variance + d*d);   
        return max(p, p_max);


    #else
        return 1.0f;
    #endif

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
    vec4 specular = (spec * light.lightColor) * vec4(specularStrength, 0.0) * smoothstep(-0.1,0.1, NdotL);  


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



