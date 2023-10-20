#version 330 core
out vec4 FragColor;

in vec2 TexCoords;

uniform vec3 albedo;
uniform vec3 lightColor;

void main()
{    
    FragColor = vec4(albedo*lightColor,1.0);
}