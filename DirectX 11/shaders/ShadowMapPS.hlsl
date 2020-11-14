struct vs_output
{
    float4 Pos : SV_POSITION;
    float3 WorldPos : POSITION;
    float3 WorldNormal : NORMAL;
};

struct ps_output
{
    float3 WorldPos : SV_TARGET0;
    float3 WorldNormal : SV_TARGET1;
    float3 Flux : SV_TARGET2;
};

cbuffer color_info : register(b1)
{
    float3 Color;
};

ps_output PS(vs_output Input)
{
    ps_output Output;

    Output.WorldPos = Input.WorldPos;
    Output.WorldNormal = normalize(Input.WorldNormal);
    Output.Flux = Color;

    return(Output);
}