#version 330 core

// Input vertex data, different for all executions of this shader.
layout(location = 0) in vec3 aVertexPosition;

// Values that stay constant for the whole mesh.
uniform mat4 uMVMatrix;
uniform mat4 uPMatrix;

void main()
{	
	// Set the complete (Perspective*model*view*objcoords) position of the vertex, 1.0 is totally opaque
	gl_Position =  uPMatrix * uMVMatrix * vec4(aVertexPosition, 1.0);
}