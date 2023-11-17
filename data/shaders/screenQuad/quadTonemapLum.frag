#version 330 core

out vec4 FragColor;

in vec2 TexCoords;

uniform float exposure;
uniform sampler2D screenTexture;

float LinearRgbToLuminance(vec3 linearRgb) 
{
	return dot(linearRgb, vec3(0.2126729f,  0.7151522f, 0.0721750f));
}

void main()
{    
    const float gamma = 2.2;
    vec3 hdrColor = texture(screenTexture, TexCoords).rgb;
  
    // exposure tonemapping
    vec3 mapped = vec3(1.0) - exp(-hdrColor * exposure);
    
    // gamma correction 
    mapped = pow(mapped, vec3(1.0 / gamma));
    
    //FragColor = vec4(mapped, 1.0);
    float lum  = LinearRgbToLuminance(mapped);
    FragColor = vec4(mapped, lum);
}