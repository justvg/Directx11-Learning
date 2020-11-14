cbuffer matrix_buffer : register(b0)
{
    float4x4 Projection;
    float4x4 View;
    float4x4 Model;
};

struct vs_input
{
    float3 Pos : POSITION;
    float3 Normal : NORMAL;
    float2 TexCoords : TEXCOORD;
    float3 Tangent : TANGENT;
    float3 Bitangent : BITANGENT;
};

struct vs_output
{
    float4 Pos : SV_POSITION;
    float3 WorldPos : POSITION;
    float3 Normal : NORMAL;
    float2 TexCoords : TEXCOORD;
    float3 Tangent : TANGENT;
    float3 Bitangent : BITANGENT;
};

vs_output VS(vs_input Input)
{
    vs_output Output;

    Output.Pos = mul(mul(mul(float4(Input.Pos, 1.0), Model), View), Projection);
    Output.WorldPos = (float3)mul(float4(Input.Pos, 1.0), Model);
    Output.Normal = normalize(mul(Input.Normal, (float3x3)Model));
    Output.Tangent = normalize(mul(Input.Tangent, (float3x3)Model));
    Output.Bitangent = normalize(mul(Input.Bitangent, (float3x3)Model));
    Output.TexCoords = Input.TexCoords;

    return(Output);
}