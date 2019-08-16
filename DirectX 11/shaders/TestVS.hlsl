cbuffer matrix_buffer
{
	float4x4 Model;
	float4x4 View;
	float4x4 Projection;
};

struct vs_in
{
	float3 Pos : POSITION;
	float3 Normal : NORMAL;
	float3 TexCoords : TEXCOORD;
};

struct vs_out
{
	float4 Pos : SV_POSITION;
	float3 Normal : NORMAL;
};

vs_out VS(vs_in Input)
{
	vs_out Output;

	Output.Pos = mul(mul(mul(float4(Input.Pos, 1.0), Model), View), Projection);
	float3x3 NormalMatrix = (float3x3)mul(Model, View);
	Output.Normal = normalize((float3)mul(float4(mul(Input.Normal, NormalMatrix), 1.0), Projection));

	return(Output);
}