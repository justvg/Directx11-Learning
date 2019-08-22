struct vs_out
{
	float4 Pos : SV_POSITION;
	float2 TexCoords : TEXCOORD;
};

Texture2D Texture;
SamplerState Sampler;

float4 PS(vs_out Input) : SV_TARGET
{
	float4 FragColor = Texture.Sample(Sampler, Input.TexCoords);

	return(FragColor);
}