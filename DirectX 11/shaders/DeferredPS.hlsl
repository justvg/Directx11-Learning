Texture2D NormalsTexture : register(t0);
Texture2D RSMIndirectIllumTexture : register(t1);
Texture2D ColorTexture : register(t2);

SamplerState DefaultSampler : register(s0);

struct vs_output
{
    float4 Pos : SV_POSITION;
    float2 TexCoords : TEXCOORD;
};

float4 PS(vs_output Input, float4 ScreenSpacePos : SV_Position) : SV_TARGET
{
    float4 FragColor = float4(0.0, 0.0, 0.0, 0.0);

    float3 SunDir = normalize(float3(-1.0, -1.0, 1.0));
    float3 Normal = NormalsTexture.Sample(DefaultSampler, Input.TexCoords).xyz;
    
    float4 RSMIndirectIllum = RSMIndirectIllumTexture.Sample(DefaultSampler, Input.TexCoords);
    float ShadowFactor = RSMIndirectIllum.w;

    float3 Ambient = RSMIndirectIllum.xyz;
    float3 DiffuseColor = ShadowFactor * max(dot(Normal, -SunDir), 0.0) * ColorTexture.Sample(DefaultSampler, Input.TexCoords).xyz;

    float3 FinalColor = Ambient + DiffuseColor;
    float3 ColorAfterToneMapping = FinalColor / (FinalColor + 1.0);
    FragColor = float4(ColorAfterToneMapping, 1.0);
    return(FragColor);
}