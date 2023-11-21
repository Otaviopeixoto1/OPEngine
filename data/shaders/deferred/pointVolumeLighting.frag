#version 330 core

//force use of light volumes
#define LIGHT_VOLUMES

#include "lights.glsl"


out vec4 FragColor;


uniform sampler2D gAlbedoSpec;
uniform sampler2D gNormal;
uniform sampler2D gPosition;
uniform int instanceID;
uniform vec2 gScreenSize;

void main()
{    
    vec2 uv = gl_FragCoord.xy / gScreenSize;
    vec3 ViewFragPos = texture(gPosition, uv).rgb;
    vec3 ViewNormal = texture(gNormal, uv).rgb;
    vec4 AlbedoSpec = texture(gAlbedoSpec, uv);

    vec3 norm = normalize(ViewNormal);

    FragColor = vec4(AlbedoSpec.rgb,1.0) * CalcPointLight(instanceID, norm, ViewFragPos, vec3(1,1,1), AlbedoSpec.a);
}  