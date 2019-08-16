struct gs_out
{
	float4 Pos : SV_POSITION;
};

float4 PS(gs_out Input) : SV_TARGET
{
	float4 FragColor = float4(1.0, 1.0, 0.0, 1.0);

	return(FragColor);
}