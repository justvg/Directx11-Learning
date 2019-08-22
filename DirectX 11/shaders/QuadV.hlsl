struct vs_in
{
	float3 Pos : POSITION;
	float2 TexCoords : TEXCOORD;
};

struct vs_out
{
	float4 Pos : SV_POSITION;
	float2 TexCoords : TEXCOORD;
};

vs_out VS(vs_in Input)
{
	vs_out Output;

	Output.Pos = float4(Input.Pos, 1.0);
	Output.TexCoords = Input.TexCoords;

	return(Output);
}