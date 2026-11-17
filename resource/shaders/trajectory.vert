#version 330 core

layout (location = 0) in vec3 position;
layout (location = 1) in vec3 color;
layout (location = 2) in float speed;

out vec3 fragColor;
out float fragSpeed;

uniform mat4 projection;
uniform mat4 view;
uniform mat4 model;  // 新增模型矩阵

void main()
{
    // 应用模型矩阵使轨迹线随场景旋转
    gl_Position = projection * view * model * vec4(position, 1.0);
    fragColor = color;
    fragSpeed = speed;
}
