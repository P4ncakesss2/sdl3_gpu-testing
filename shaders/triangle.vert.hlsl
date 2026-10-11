cbuffer TransformData : register(b0, space1)
{
    column_major float4x4 mvp;
};

struct VertexOutput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

VertexOutput main(uint vertex_id : SV_VertexID)
{
    static const float2 positions[3] = {
        float2( 0.0,  0.65),
        float2( 0.65, -0.55),
        float2(-0.65, -0.55)
    };
    static const float2 uvs[3] = {
        float2(0.5, 0.0),
        float2(1.0, 1.0),
        float2(0.0, 1.0)
    };

    VertexOutput output;
    output.position = mul(mvp, float4(positions[vertex_id], 0.0, 1.0));
    output.uv = uvs[vertex_id];
    return output;
}
