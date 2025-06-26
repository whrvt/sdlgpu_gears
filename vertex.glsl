#version 450

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_normal;

layout(set = 1, binding = 0) uniform UniformBuffer {
    mat4 mvp_matrix;
    mat4 model_matrix;
    mat3 normal_matrix;
    vec3 light_position;
    vec3 light_color;
    vec3 object_color;
} ubo;

layout(location = 0) out vec3 frag_color;

void main() {
    gl_Position = ubo.mvp_matrix * vec4(in_position, 1.0);

    // transform normal to view space for lighting calculation
    vec3 view_normal = normalize(ubo.normal_matrix * in_normal);

    // light direction in view space (i.e. glLightfv(GL_LIGHT0, GL_POSITION, pos))
    vec3 light_dir = normalize(ubo.light_position);

    float diff = max(dot(view_normal, light_dir), 0.0);
    vec3 ambient = 0.2 * ubo.object_color;
    vec3 diffuse = diff * ubo.light_color * ubo.object_color;

    frag_color = ambient + diffuse;
}
