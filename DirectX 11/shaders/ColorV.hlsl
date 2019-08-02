cbuffer matrix_buffer
{
	float4x4 Model;
	float4x4 View;
	float4x4 Projection;
};

struct vertex_input
{
	float3 Pos : POSITION;
	float4 Color : COLOR;
};

struct vertex_out
{
	float4 Pos : SV_POSITION;
	float4 Color : COLOR;
};

vertex_out VS(vertex_input Input)
{
	vertex_out Output;

	Output.Pos = mul(mul(mul(float4(Input.Pos, 1.0f), Model), View), Projection);
	Output.Color = Input.Color;

	return (Output);
}

