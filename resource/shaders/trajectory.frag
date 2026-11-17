#version 330 core

in vec3 fragColor;
in float fragSpeed;
out vec4 outColor;

void main()
{
    // 根据速度动态调整颜色强度
    float intensity = 0.5 + fragSpeed * 0.5;
    outColor = vec4(fragColor * intensity, 1.0);
}
