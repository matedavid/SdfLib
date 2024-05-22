#version 430

uniform sampler2D inputTexture;
uniform float tonemappingConstant;

out vec4 fragColor;

in vec2 uv;

void main() 
{
    vec3 color = texture(inputTexture, uv).rgb;
    color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0/tonemappingConstant));

	fragColor = vec4(color, 1.0);
}