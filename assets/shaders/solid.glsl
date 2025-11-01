#version 450

//@ VERTEX

layout(set = 0, binding = 0, row_major) uniform CameraBuffer {
    mat3 view_projection;
} camera;

layout(set = 1, binding = 0, row_major) uniform ObjectBuffer {
    mat3 transform;
} object;

layout(location = 0) in vec2 v_position;
layout(location = 1) in float v_depth;
layout(location = 2) in vec2 v_uv;

layout(location = 0) out vec2 f_uv;
layout(location = 1) out vec3 f_normal;

void main() {
    mat3 mvp = object.transform * camera.view_projection;
    vec3 screen_pos = vec3(v_position, 1.0) * mvp;
    gl_Position = vec4(screen_pos.xy, 0.0, 1.0);
    f_uv = v_uv;
}

//@ END

//@ FRAGMENT

layout(location = 0) in vec2 f_uv;
layout(location = 0) out vec4 outColor;

layout(set = 2, binding = 0) uniform ColorBuffer {
    vec4 color;
    vec2 uv_offset;
    vec2 padding;
} color_buffer;

layout(set = 4, binding = 0) uniform sampler2D mainTexture;

void main() {
    vec4 color = texture(mainTexture, f_uv + color_buffer.uv_offset) * color_buffer.color;
    outColor = color;
}

//@ END
