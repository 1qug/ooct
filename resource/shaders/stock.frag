#version 330 core
in vec3 FragPos;
in vec3 Normal;

uniform vec3 objectColor;
uniform float alpha;

out vec4 FragColor;

void main()
{
    // 简单的固定颜色，不受光照影响
    FragColor = vec4(objectColor, alpha);
}
