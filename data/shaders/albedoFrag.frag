#version 330 core
out vec4 FragColor;

in vec2 TexCoords;

uniform vec3 albedo;

void main()
{    
    FragColor = vec4(albedo,1.0);
}