#version 440

//in vec3 outNormal;
in vec4 outColor;
//in vec4 outPosition;

out vec4 finalColor;


void main()
{	
	finalColor = outColor;
	//finalColor = vec4(1.0f,0.0f,0.0f,1.0f);
}