#version 440 core

out vec4 FragColor; 
in vec2 TexCoords;

uniform sampler2D image;


uniform vec2 direction;

uniform float offset5[2] = float[](0.0, 1.3333333333333333);
uniform float weight5[2] = float[](0.29411764705882354, 0.35294117647058826);

uniform float offset9[3] = float[](0.0, 1.3846153846, 3.2307692308);
uniform float weight9[3] = float[](0.2270270270, 0.3162162162, 0.0702702703);

uniform float offset13[4] = float[](0.0, 1.411764705882353, 3.2941176470588234, 5.176470588235294);
uniform float weight13[4] = float[](0.1964825501511404, 0.2969069646728344, 0.09447039785044732, 0.010381362401148057);




subroutine vec4 GetBlur();

layout(index = 0) subroutine(GetBlur)
vec4 Blur5() 
{
    vec2 tex_offset = 1.0 / textureSize(image, 0); // gets size of single texel

    vec3 result = texture(image, TexCoords).rgb * weight5[0]; // current fragment's contribution
    result += texture2D(image, TexCoords + tex_offset * offset5[1] * direction).rgb * weight5[1];
    result += texture2D(image, TexCoords - tex_offset * offset5[1] * direction).rgb * weight5[1];

    

    return vec4(result, 1.0);
}

layout(index = 1) subroutine(GetBlur)
vec4 Blur9() 
{
    vec2 tex_offset = 1.0 / textureSize(image, 0); // gets size of single texel
    vec3 result = texture(image, TexCoords).rgb * weight9[0]; // current fragment's contribution
    
    for(int i = 1; i < 3; ++i)
    {
        result += texture2D(image, TexCoords + tex_offset * offset9[i] * direction).rgb * weight9[i];
        result += texture2D(image, TexCoords - tex_offset * offset9[i] * direction).rgb * weight9[i];
    }
    

    return vec4(result, 1.0);
}

layout(index = 2) subroutine(GetBlur)
vec4 Blur13() 
{
    vec2 tex_offset = 1.0 / textureSize(image, 0); // gets size of single texel
    vec3 result = texture(image, TexCoords).rgb * weight13[0]; // current fragment's contribution
    
    for(int i = 1; i < 4; ++i)
    {
        result += texture2D(image, TexCoords + tex_offset * offset13[i] * direction).rgb * weight13[i];
        result += texture2D(image, TexCoords - tex_offset * offset13[i] * direction).rgb * weight13[i];
    }
    

    return vec4(result, 1.0);
}

layout(location = 0) subroutine uniform GetBlur Blur;




void main()
{             
    FragColor = Blur();
}