#version 330 core

// The specific picking color of this object
uniform vec3 uPickingColor;

void main()
{
	// Just a plain color in every part of our mesh
	gl_FragColor = vec4(uPickingColor.rgb, 1.0);
}