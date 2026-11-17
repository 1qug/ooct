#version 330 core
in vec3 FragPos;
in vec3 Normal;
in vec3 Color;

uniform vec3 lightPos;
uniform vec3 viewPos;
uniform vec3 lightColor;
uniform float lightIntensity;
uniform vec3 ambientUp;
uniform vec3 ambientDown;

out vec4 FragColor;

void main()
{
    // 基础光照计算
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * lightColor * lightIntensity;
    
    // 环境光 (上下渐变)
    float upFactor = max(norm.y, 0.0);
    vec3 ambient = mix(ambientDown, ambientUp, upFactor);
    
    FragColor = vec4((ambient + diffuse) * Color, 1.0);
}
