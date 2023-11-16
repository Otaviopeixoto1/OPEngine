#version 330 core

#include "lights.glsl"

out vec4 FragColor;
  
in vec2 TexCoords;

uniform sampler2D gAlbedoSpec;
uniform sampler2D gNormal;
uniform sampler2D gPosition;


void main()
{             
    vec3 ViewFragPos = texture(gPosition, TexCoords).rgb;
    vec3 ViewNormal = texture(gNormal, TexCoords).rgb;
    vec4 AlbedoSpec = texture(gAlbedoSpec, TexCoords);
    


    vec4 outFrag = vec4(AlbedoSpec.rgb,1.0) * vec4(ambientLight.xyz * ambientLight.w, 1.0);

    vec3 norm = normalize(ViewNormal);
    for(int i = 0; i < numDirLights; i++)
    {
        vec3 viewDir = -normalize(ViewFragPos);
        outFrag += vec4(AlbedoSpec.rgb,1.0) * CalcDirLight(dirLights[i], norm, viewDir, vec3(1,1,1), AlbedoSpec.a);
    }
    
    #ifndef LIGHT_VOLUMES
        for(int i = 0; i < numPointLights; i++)
        {
            outFrag += vec4(AlbedoSpec.rgb,1.0) * CalcPointLight(pointLights[i], norm, ViewFragPos, vec3(1,1,1), AlbedoSpec.a);
        }
    #endif
     
    // exposure tonemapping
    //float exposure = 0.5f;
    //float gamma = 2.2f;
    //vec3 mapped = vec3(1.0) - exp(-outFrag.xyz * exposure);
    
    // gamma correction 
    //mapped = pow(mapped, vec3(1.0 / gamma));

    //FragColor = vec4(mapped, 1.0f);
    FragColor = outFrag;
}  