#version 450

//@ VERTEX

layout(set = 0, binding = 0, row_major) uniform CameraBuffer
{
    mat3 view_projection;
} camera;

layout(set = 1, binding = 0, row_major) uniform ObjectBuffer
{
    mat3 transform;
} object;

layout(location = 0) in vec2 v_position;
layout(location = 1) in vec2 v_uv0;
layout(location = 2) in vec3 v_normal;

layout(location = 0) out vec2 f_uv;
layout(location = 1) out vec3 f_normal;

void main()
{
    // Position
    mat3 mvp = object.transform * camera.view_projection;
    vec3 screen_pos = vec3(v_position, 1.0) * mvp;
    gl_Position = vec4(screen_pos.xy, 0.0, 1.0);

    // Uv
    f_uv = v_uv0;

    // Normal
    vec2 transform_right = normalize(object.transform[0].xy);
    vec2 transform_up = normalize(object.transform[1].xy);
    vec3 world_normal = vec3(
        dot(v_normal.xy, vec2(transform_right.x, transform_up.x)),
        dot(v_normal.xy, vec2(transform_right.y, transform_up.y)),
        v_normal.z
    );

    f_normal = normalize(world_normal);
}

//@ END

//@ FRAGMENT

layout(location = 0) in vec2 f_uv;
layout(location = 1) in vec3 f_normal;
layout(location = 0) out vec4 outColor;
layout(set = 2, binding = 0) uniform ColorBuffer
{
    vec4 color;
} colorData;

layout(set = 3, binding = 0) uniform LightBuffer
{
    vec3 direction;
    float padding;
    vec4 diffuse_color;
    vec4 shadow_color;
} light;

layout(set = 4, binding = 0) uniform sampler2D mainTexture;

float get_value(vec3 c) {
    vec4 K = vec4(0.0, -1.0 / 3.0, 2.0 / 3.0, -1.0);
    vec4 p = mix(vec4(c.bg, K.wz), vec4(c.gb, K.xy), step(c.b, c.g));
    vec4 q = mix(vec4(p.xyw, c.r), vec4(c.r, p.yzx), step(p.x, c.r));
    return q.x;
}

void main()
{
    float diffuse = (dot(f_normal, light.direction) + 1) * 0.5;
    float lighting = 0.3 + 0.7 * diffuse;
    vec4 texColor = texture(mainTexture, f_uv);
    vec3 finalColor = texColor.rgb * lighting * colorData.color.rgb;
    float gray = max(0.02, get_value(finalColor));

    outColor = vec4(vec3(gray), texColor.a * colorData.color.a);
}

//@ END
