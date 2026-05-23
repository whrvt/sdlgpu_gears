#include <metal_stdlib>
using namespace metal;

struct FragmentInput {
    float4 position [[position]];
    float3 color;
};

fragment float4 main0(FragmentInput in [[stage_in]]) {
    return float4(in.color, 1.0);
}
