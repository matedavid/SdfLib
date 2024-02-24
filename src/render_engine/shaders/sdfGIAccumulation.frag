#version 430

uniform sampler2D colorTexture;
uniform sampler2D accumulationTexture;

uniform int accumulationFrame;

out vec4 fragColor;

in vec2 uv;

void main() 
{
    vec3 color = texture(colorTexture, uv).rgb;
    if (accumulationFrame == 1)
    {
        fragColor = vec4(color, 1.0f);
        return;
    }

    vec3 accum = texture(accumulationTexture, uv).rgb;

    vec3 finalColor = (accum * (float(accumulationFrame) - 1) + color) / float(accumulationFrame);
    fragColor = vec4(finalColor, 1.0f);
}