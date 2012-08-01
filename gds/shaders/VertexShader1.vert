#version 330 core

// Input vertex data, different for all executions of this shader.
layout(location = 0) in vec3 aVertexPosition;
layout(location = 1) in vec2 aTextureCoord;
layout(location = 2) in vec3 aVertexNormal;

// Values that stay constant for the whole mesh.
uniform mat4 uMVMatrix;
uniform mat4 uPMatrix;
uniform mat3 uNMatrix;

out vec2 vTextureCoord;
out vec3 vTransformedNormal;
out vec4 vPosition;

void main()
{	
	// Pass along the position of the vertex (used to calculate point-to-vertex light direction),
	// no perspective here since we need absolute position (we used absolute position for the light point too)
	vPosition = uMVMatrix * vec4(aVertexPosition, 1.0);
	// Set the complete (Perspective*model*view) position of the vertex
	gl_Position =  uPMatrix * vPosition;
	
	// Save the uv attributes
	vTextureCoord = aTextureCoord;
	
	// Pass along the modified normal matrix * vertex normal (the uNMatrix is
	// necessary otherwise normals would point in a wrong direction and
	// they would not be modulo 1 vectors), this matrix ensures direction and
	// modulo 1 preservation while converting their coords to absolute coordinates
	vTransformedNormal = uNMatrix * aVertexNormal;
}