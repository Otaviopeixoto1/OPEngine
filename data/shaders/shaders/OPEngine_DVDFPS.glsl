#version 330 core
out vec4 FragColor;

in vec2 TexCoords;

uniform vec3 albedo;
//uniform vec4 lightColor;

void main()
{    
    //FragColor = vec4(albedo,1.0)*lightColor;
    FragColor = vec4(albedo,1.0);
}