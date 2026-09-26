// ponytail: [Basic HLSL quad rendering shader] -> [YUV420 to RGB conversion shader with bilinear filtering]

Texture2D g_texture : register(t0);
SamplerState g_sampler : register(s0);

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
    return g_texture.Sample(g_sampler, input.tex);
}
