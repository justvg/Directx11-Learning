struct vs_output
{
    float4 Pos : SV_POSITION;
    float3 WorldPos : POSITION;
    float3 WorldNormal : NORMAL;
};

cbuffer color_info : register(b1)
{
    float3 Color;
};

cbuffer light_matrix : register(b2)
{
    float4x4 LightProjection;
    float4x4 LightView;
};

cbuffer rsm_samples : register(b3)
{
    float2 RSMSamples[256];
};

cbuffer rsm_noise : register(b4)
{
    float2 RSMNoise[16];
};

Texture2D ShadowMap : register(t0);
Texture2D WorldPosTexture : register(t1);
Texture2D WorldNormalsTexture : register(t2);
Texture2D FluxTexture : register(t3);

SamplerState DefaultSampler : register(s0);
SamplerState ShadowMapSampler : register(s1);

float CalculateShadow(float3 FragWorldPos)
{
    float4 LightClipSpace = mul(mul(float4(FragWorldPos, 1.0), LightView), LightProjection);
    float3 LightNDC = LightClipSpace.xyz / LightClipSpace.w;
    float2 UV = float2(0.5, -0.5)*LightNDC.xy + float2(0.5, 0.5);

    float ShadowMapDepth = ShadowMap.Sample(ShadowMapSampler, UV).x;

    float Bias = 0.075;
    float ShadowFactor = ((LightNDC.z - Bias) > ShadowMapDepth) ? 0.0 : 1.0;

    return(ShadowFactor);
}

float3 CalculateRSM(float3 FragWorldPos, float3 FragWorldNormal, float2 ScreenSpaceP)
{
    float4 LightClipSpace = mul(mul(float4(FragWorldPos, 1.0), LightView), LightProjection);
    float3 LightNDC = LightClipSpace.xyz / LightClipSpace.w;
    float2 UV = float2(0.5, -0.5)*LightNDC.xy + float2(0.5, 0.5);

    uint2 PixelIndex = (uint2)ScreenSpaceP;
    uint XI = PixelIndex.x % 4;
    uint YI = PixelIndex.y % 4;
    float2 RandomVec = RSMNoise[YI*4 + XI];
    float2x2 NoiseMatrix = float2x2(RandomVec, float2(RandomVec.y, -RandomVec.x));

    const float MaxRadius = 0.3;
    float3 IndirectIllumination = float3(0.0, 0.0, 0.0);
    for(int I = 0; I < 256; I++)
    {
        float2 SampleUV = UV + MaxRadius*mul(RSMSamples[I], NoiseMatrix);

        float3 WorldPos = WorldPosTexture.Sample(DefaultSampler, SampleUV).xyz;
        float3 WorldNormal = WorldNormalsTexture.Sample(DefaultSampler, SampleUV).xyz;
        float3 Flux = FluxTexture.Sample(DefaultSampler, SampleUV).xyz;

        float3 IndirectBounce = Flux * max(dot(WorldNormal, normalize(FragWorldPos - WorldPos)), 0.0) * 
                                       max(dot(FragWorldNormal, normalize(WorldPos - FragWorldPos)), 0.0) /
                                       pow(length(WorldPos - FragWorldPos), 4);
        IndirectBounce *= dot(RSMSamples[I].xy, RSMSamples[I].xy);

        IndirectIllumination += IndirectBounce;
    }

    IndirectIllumination = IndirectIllumination / 4.0;
    return(IndirectIllumination);
}

struct gbuffer_output
{
    float4 Normal : SV_TARGET0;
    float4 RSMIndirectIllum : SV_TARGET1;
    float4 Color : SV_TARGET2;
};

gbuffer_output PS(vs_output Input, float4 ScreenSpacePos : SV_Position)
{
    gbuffer_output Output;

    Output.Normal = float4(normalize(Input.WorldNormal), 0.0);
    
    Output.RSMIndirectIllum.xyz = CalculateRSM(Input.WorldPos, Input.WorldNormal, ScreenSpacePos.xy);
    Output.RSMIndirectIllum.w = CalculateShadow(Input.WorldPos);

    Output.Color = float4(Color, 1.0);

    return(Output);
}