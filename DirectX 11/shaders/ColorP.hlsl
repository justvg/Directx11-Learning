struct vertex_out
{
	float4 Pos : SV_POSITION;
	float4 Color : COLOR;
};

float4 PS(vertex_out Input) : SV_TARGET
{
	return (Input.Color);
}