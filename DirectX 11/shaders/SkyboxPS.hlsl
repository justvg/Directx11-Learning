struct vs_out
{
	float4 Pos : SV_POSITION;
	float3 PosL : POSITION;
};

TextureCube Cubemap;
SamplerState Sampler;

float4 PS(vs_out Input) : SV_TARGET
{
	float4 FragColor = Cubemap.Sample(Sampler, Input.PosL);
	return(FragColor);
}