struct vs_input
{
    float3 Pos : POSITION;
};

struct vs_output
{
    float4 Pos : SV_POSITION;
    float2 TexCoords : TEXCOORD;
};

vs_output VS(vs_input Input)
{
    vs_output Output;
    
    Output.Pos = float4(Input.Pos, 1.0);
    Output.TexCoords = float2(0.5, -0.5)*Input.Pos.xy + float2(0.5, 0.5);

    return(Output);
}