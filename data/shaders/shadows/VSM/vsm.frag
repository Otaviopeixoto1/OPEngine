#version 440

in vec4 lightSpacePos;
out vec4 FragColor;

void main() 
{
    float FragmentDepth = lightSpacePos.z/lightSpacePos.w;
    FragmentDepth = FragmentDepth * 0.5 + 0.5;			//Don't forget to move away from unit cube ([-1,1]) to [0,1] coordinate system

    vec2 mu = vec2(FragmentDepth,FragmentDepth*FragmentDepth);

    // Adding bias to solve shadow acne:
    float dx = dFdx(FragmentDepth);
    float dy = dFdy(FragmentDepth);
    mu.y += 0.25*(dx*dx+dy*dy);

    //OPTIMIZED MOMENT QUANTIZATION
    //mu.y = 4.0*(mu.x-mu.y);

    FragColor = vec4(mu,0.0,0.0);
}