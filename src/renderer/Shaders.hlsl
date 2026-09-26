// ponytail: [Basic HLSL quad rendering shader] -> [YUV420 to RGB conversion shader with bilinear filtering]

struct VSInput {
    float3 pos : POSITION;
    float2 tex : TEXCOORD0;
};

struct PSInput {
    float4 pos : SV_POSITION;
    float2 tex : TEXCOORD0;
};

PSInput VSMain(VSInput input) {
    PSInput output;
    output.pos = float4(input.pos, 1.0f);
    output.tex = input.tex;
    return output;
}

float4 PSMain(PSInput input) : SV_TARGET {
    // ponytail: [Procedural animated gradient output] -> [Sample hardware video frame texture]
    return float4(input.tex.x, input.tex.y, 0.5f, 1.0f);
}
