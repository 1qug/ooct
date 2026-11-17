#version 330 core

layout(lines) in;
layout(triangle_strip, max_vertices = 4) out;

uniform float u_thickness = 3.0;  // 线宽
uniform mat4 projection;
uniform mat4 view;

void main() {
    vec4 p0 = gl_in[0].gl_Position;
    vec4 p1 = gl_in[1].gl_Position;
    
    // 计算屏幕空间方向
    vec2 screen_p0 = (projection * view * p0).xy;
    vec2 screen_p1 = (projection * view * p1).xy;
    vec2 dir = normalize(screen_p1 - screen_p0);
    vec2 normal = vec2(-dir.y, dir.x) * u_thickness / 1000.0;
    
    // 生成四边形
    gl_Position = p0 + vec4(normal, 0.0, 0.0);
    EmitVertex();
    
    gl_Position = p0 - vec4(normal, 0.0, 0.0);
    EmitVertex();
    
    gl_Position = p1 + vec4(normal, 0.0, 0.0);
    EmitVertex();
    
    gl_Position = p1 - vec4(normal, 0.0, 0.0);
    EmitVertex();
    
    EndPrimitive();
}
