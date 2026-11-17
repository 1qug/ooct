#version 330 core
in vec3 FragPos;
in vec3 Normal;
in vec4 FragPosLightSpace;

out vec4 FragColor;

// 主光源参数
uniform vec3 lightPos;
uniform vec3 lightColor;
uniform float lightIntensity;

// 相机光源参数
uniform vec3 cameraLightPos;
uniform vec3 cameraLightColor;

// 材质参数
uniform vec3 objectColor;
uniform vec3 viewPos;

// 环境光参数
uniform vec3 ambientUp;
uniform vec3 ambientDown;

// 阴影贴图
uniform sampler2D shadowMap;

float ShadowCalculation(vec4 fragPosLightSpace) {
    // 执行透视除法
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    // 变换到[0,1]的范围
    projCoords = projCoords * 0.5 + 0.5;
    
    // 取得最近点的深度
    float closestDepth = texture(shadowMap, projCoords.xy).r; 
    // 取得当前片元在光源视角下的深度
    float currentDepth = projCoords.z;
    
    // 计算阴影偏移（根据表面角度动态调整）
    vec3 normal = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos);
    float bias = max(0.005 * (1.0 - dot(normal, lightDir)), 0.001);
    
    // 使用PCF软阴影
    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(shadowMap, 0);
    for(int x = -1; x <= 1; ++x) {
        for(int y = -1; y <= 1; ++y) {
            float pcfDepth = texture(shadowMap, projCoords.xy + vec2(x, y) * texelSize).r; 
            shadow += currentDepth - bias > pcfDepth ? 1.0 : 0.0;        
        }    
    }
    shadow /= 9.0;
    
    // 距离衰减（指数衰减）
    float dist = length(FragPos - lightPos);
    float attenuation = 1.0 / (1.0 + 0.1 * dist + 0.01 * dist * dist);
    shadow *= attenuation;
    
    return shadow;
}

void main() {
    // 环境光（渐变天空盒效果）
    vec3 norm = normalize(Normal);
    vec3 ambient = 0.25 * (ambientUp * (norm.y * 0.5 + 0.5) + 
                          ambientDown * (-norm.y * 0.5 + 0.5));
    
    // 主光源计算
    vec3 lightDir = normalize(lightPos - FragPos);
    
    // 漫反射
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * lightColor * lightIntensity;
    
    // 镜面反射（增强高光）
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 128); // 提高高光锐度
    vec3 specular = 0.8 * spec * lightColor;
    
    // 阴影计算
    float shadow = ShadowCalculation(FragPosLightSpace);
    
    // 相机光源（棚灯效果）
    vec3 cameraLightDir = normalize(cameraLightPos - FragPos);
    float cameraDiff = max(dot(norm, cameraLightDir), 0.0);
    vec3 cameraDiffuse = cameraDiff * cameraLightColor;
    
    // 相机光源镜面反射
    vec3 cameraReflectDir = reflect(-cameraLightDir, norm);
    float cameraSpec = pow(max(dot(viewDir, cameraReflectDir), 0.0), 64);
    vec3 cameraSpecular = 0.3 * cameraSpec * vec3(1.0);
    
    // 组合所有光源
    vec3 lighting = ambient +
                   (1.0 - shadow) * (diffuse + specular) + // 主光源
                   cameraDiffuse + cameraSpecular;         // 相机光源
    
    // 应用物体颜色
    vec3 result = lighting * objectColor;
    
    // 伽马校正
    result = pow(result, vec3(1.0/2.2));
    
    FragColor = vec4(result, 1.0);
}
