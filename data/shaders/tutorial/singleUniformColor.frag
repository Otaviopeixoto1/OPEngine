#version 330 core

out vec4 FragColor;
in vec3 Normal;
in vec2 TexCoord;

uniform vec4 uColor;
uniform sampler2D main_texture;

void main()
{
    //using a uniform color provided by the openGL api:
    //FragColor = uColor;
    
    //using aditional inputs comming from the vertex shader:
    //FragColor = vec4(Normal, 1.0);

    //using the uvs:
    //FragColor = vec4(TexCoord,0.0,1.0);
    
    //using/sampling a texture provided by the openGL api:
    FragColor = texture(main_texture, TexCoord) * uColor;
}