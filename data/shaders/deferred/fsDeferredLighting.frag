#version 330 core

#include "lights.glsl"

out vec4 FragColor;
  
in vec2 TexCoords;

uniform sampler2D gAlbedoSpec;
uniform sampler2D gNormal;
uniform sampler2D gPosition;

layout (std140) uniform GlobalMatrices
{
    mat4 projectionMatrix;
    mat4 viewMatrix;
    mat4 inverseViewMatrix;
};


void main()
{             
    vec4 ViewFragPos = texture(gPosition, TexCoords);
    vec3 ViewNormal = texture(gNormal, TexCoords).rgb;
    vec4 AlbedoSpec = texture(gAlbedoSpec, TexCoords);
    


    vec4 outFrag = vec4(AlbedoSpec.rgb,1.0) * vec4(ambientLight.xyz * ambientLight.w, 1.0);

    vec3 norm = normalize(ViewNormal);
    vec3 worldNorm = (inverseViewMatrix * vec4(norm,0.0)).xyz;
    vec4 worldPos = inverseViewMatrix * ViewFragPos;

    for(int i = 0; i < numDirLights; i++)
    {
        vec3 viewDir = -normalize(ViewFragPos.xyz);
        vec4 lighting = vec4(AlbedoSpec.rgb, 1.0) * CalcDirLight(i, norm, viewDir, vec3(1,1,1), AlbedoSpec.a);
        outFrag += lighting * GetDirLightShadow(i, worldPos, worldNorm);
    }
    
    #ifndef LIGHT_VOLUMES
        for(int i = 0; i < numPointLights; i++)
        {
            outFrag += vec4(AlbedoSpec.rgb,1.0) * CalcPointLight(i, norm, ViewFragPos.xyz, vec3(1,1,1), AlbedoSpec.a);
        }
    #endif
     
    FragColor = outFrag;
}  