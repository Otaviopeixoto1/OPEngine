#version 330 core

out vec4 FragColor;
  
in vec2 TexCoords;

uniform sampler2D gAlbedoSpec;
uniform sampler2D gNormal;
uniform sampler2D gPosition;


void main()
{             
    vec3 ViewFragPos = texture(gPosition, TexCoords).rgb;
    vec3 ViewNormal = texture(gNormal, TexCoords).rgb;
    vec3 Albedo = texture(gAlbedoSpec, TexCoords).rgb;
    float Specular = texture(gAlbedoSpec, TexCoords).a;
    

    FragColor = vec4(ViewNormal,1.0);
}  