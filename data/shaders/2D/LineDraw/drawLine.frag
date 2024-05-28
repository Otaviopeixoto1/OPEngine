#version 330 core

out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D sdf;

void main()
{   
    //FragColor = vec4(TexCoords, 0.0, 0.0);
    vec4 sdf = texture(sdf, TexCoords).rgba;
    FragColor = (sdf.r < 0) ? vec4(sdf.gba, 1.0f) : vec4(0.0);
}