Texture2D triangle_texture : register(t0, space2);
SamplerState triangle_sampler : register(s0, space2);

struct FragmentInput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

float4 main(FragmentInput input) : SV_Target0
{
    return triangle_texture.Sample(triangle_sampler, input.uv);
}
