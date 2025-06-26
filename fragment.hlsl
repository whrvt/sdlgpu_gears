struct PixelInput {
    float3 color : TEXCOORD0;
};

struct PixelOutput {
    float4 color : SV_Target0;
};

PixelOutput main(PixelInput input) {
    PixelOutput output;
    output.color = float4(input.color, 1.0);
    return output;
}
