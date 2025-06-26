struct VertexInput {
    float3 position : TEXCOORD0;
    float3 normal : TEXCOORD1;
};

struct VertexOutput {
    float3 color : TEXCOORD0;
    float4 position : SV_POSITION;
};

cbuffer UniformBuffer : register(b0, space1) {
    float4x4 mvp_matrix;
    float4x4 model_matrix;
    // normal_matrix is laid out as 3 vec4s in std140 (each column padded to 16 bytes)
    float4 normal_matrix_col0;
    float4 normal_matrix_col1;
    float4 normal_matrix_col2;
    float4 light_position;  // vec3 padded to vec4
    float4 light_color;     // vec3 padded to vec4
    float4 object_color;    // vec3 padded to vec4
};

VertexOutput main(VertexInput input) {
    VertexOutput output;

    output.position = mul(mvp_matrix, float4(input.position, 1.0));

    // reconstruct 3x3 normal matrix from the three column vectors
    float3x3 normal_matrix = float3x3(
        normal_matrix_col0.xyz,
        normal_matrix_col1.xyz,
        normal_matrix_col2.xyz
    );

    // transform normal to view space for lighting calculation
    float3 view_normal = normalize(mul(input.normal, normal_matrix));

    // light direction in view space (i.e. glLightfv(GL_LIGHT0, GL_POSITION, pos))
    float3 light_dir = normalize(light_position.xyz);

    float diff = max(dot(view_normal, light_dir), 0.0);
    float3 ambient = 0.2 * object_color.xyz;
    float3 diffuse = diff * light_color.xyz * object_color.xyz;

    output.color = ambient + diffuse;

    return output;
}
