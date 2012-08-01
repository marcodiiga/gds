#version 330 core

// u,v values
in vec2 vTextureCoord;
in vec3 vTransformedNormal;
in vec4 vPosition;

// Values that stay constant for the whole mesh.
uniform sampler2D myTextureSampler;

uniform vec3 uAmbientColor;
uniform vec3 uPointLightingLocation;
uniform vec3 uPointLightingColor;

void main()
{
	vec3 lightWeighting;
	// Get the light direction vector
	vec3 lightDirection = normalize(uPointLightingLocation - vPosition.xyz);
	// Simple dot product between normal and light direction (cannot be lower than zero - no light)
	float directionalLightWeighting = max(dot(normalize(vTransformedNormal), lightDirection), 0.0);
	// Use the phong model
    lightWeighting = uAmbientColor + uPointLightingColor * directionalLightWeighting;
	
	// Weight the texture color with the light weight
	vec4 fragmentColor;
	fragmentColor = texture2D(myTextureSampler, vec2(vTextureCoord.s, vTextureCoord.t));
	
	gl_FragColor = vec4(fragmentColor.rgb * lightWeighting, fragmentColor.a);
}