cbuffer matrix_buffer
{
	float4x4 Model;
	float4x4 View;
	float4x4 Projection;
};

struct vs_in
{
	float3 Pos : POSITION;
};

struct vs_out
{
	float4 Pos : SV_POSITION;
	float3 PosL : POSITION;
};

vs_out VS(vs_in Input)
{
	vs_out Output;

	Output.Pos = mul(mul(mul(float4(Input.Pos, 1.0), Model), View), Projection).xyww;
	Output.PosL = Input.Pos;

	return(Output);
}