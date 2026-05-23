#include <metal_stdlib>
using namespace metal;

struct VertexInput {
    float3 position [[attribute(0)]];
    float3 normal   [[attribute(1)]];
};

struct VertexOutput {
    float4 position [[position]];
    float3 color;
};

// matches std140 layout in vertex.glsl / vertex.hlsl: normal_matrix is 3 vec4s
struct Uniforms {
    float4x4 mvp_matrix;
    float4x4 model_matrix;
    float4 normal_matrix_col0;
    float4 normal_matrix_col1;
    float4 normal_matrix_col2;
    float4 light_position;
    float4 light_color;
    float4 object_color;
};

vertex VertexOutput main0(VertexInput in [[stage_in]],
                          constant Uniforms& ubo [[buffer(0)]]) {
    VertexOutput out;

    out.position = ubo.mvp_matrix * float4(in.position, 1.0);

    // reconstruct 3x3 normal matrix from the three column vectors
    float3x3 normal_matrix = float3x3(ubo.normal_matrix_col0.xyz,
                                      ubo.normal_matrix_col1.xyz,
                                      ubo.normal_matrix_col2.xyz);

    // transform normal to view space for lighting calculation
    float3 view_normal = normalize(normal_matrix * in.normal);

    // light direction in view space (i.e. glLightfv(GL_LIGHT0, GL_POSITION, pos))
    float3 light_dir = normalize(ubo.light_position.xyz);

    float diff = max(dot(view_normal, light_dir), 0.0);
    float3 ambient = 0.2 * ubo.object_color.xyz;
    float3 diffuse = diff * ubo.light_color.xyz * ubo.object_color.xyz;

    out.color = ambient + diffuse;

    return out;
}
