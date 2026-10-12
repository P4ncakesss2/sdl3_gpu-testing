struct VertexInput
{
    float3 position : TEXCOORD0;
    float2 uv : TEXCOORD1;
};

cbuffer TransformData : register(b0, space1)
{
    column_major float4x4 mvp;
};

struct VertexOutput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

VertexOutput main(VertexInput input)
{
    VertexOutput output;
    output.position = mul(mvp, float4(input.position, 1.0));
    output.uv = input.uv;
    return output;
}
