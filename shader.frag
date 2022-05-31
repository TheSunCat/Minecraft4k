#version 450

out vec4 fragColor;

void main()
{
    // Normalized pixel coordinates (from -1 to 1)
    vec2 iResolution = vec2(1920.0, 1080.0);
    vec2 uv = (gl_FragCoord.xy/iResolution.xy)*2.0 - vec2(1.0,1.0);
    uv.y *= iResolution.y/iResolution.x;

    fragColor = abs(uv.yxyx);
}