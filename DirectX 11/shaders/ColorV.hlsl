cbuffer matrix_buffer
{
	float4x4 Model;
	float4x4 View;
	float4x4 Projection;
};

struct vertex_input
{
	float3 Pos : POSITION;
	float3 Normal : NORMAL;
	float2 TexCoords : TEXCOORD;
};

struct vertex_out
{
	float4 Pos : SV_POSITION;
	float3 PosW : POSITION;
	float3 NormalW : NORMAL;
	float2 TexCoords : TEXCOORD;
};

vertex_out VS(vertex_input Input)
{
	vertex_out Output;

	Output.Pos = mul(mul(mul(float4(Input.Pos, 1.0f), Model), View), Projection);
	Output.PosW = mul(float4(Input.Pos, 1.0f), Model).xyz;
	Output.NormalW = normalize(mul(Input.Normal, (float3x3)Model));
	Output.TexCoords = Input.TexCoords;

	return (Output);
}

