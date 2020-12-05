Texture2D NormalsTexture : register(t0);
Texture2D RSMIndirectIllumTexture : register(t1);
Texture2D ColorTexture : register(t2);
Texture2D LinearDepthTexture : register(t3);
Texture2D WorldPosTexture : register(t4);

SamplerState DefaultSampler : register(s0);

cbuffer camera_info : register(b0)
{
    float4 WorldVectorsToFarCorners[4]; // NOTE(georgy): W of these vectors contain FarPlane distance
    float4 CameraWorldPos;
};

cbuffer matrix_buffer : register(b1)
{
    float4x4 Projection;
    float4x4 View;
    float4x4 Model;
};

struct vs_output
{
    float4 Pos : SV_POSITION;
    float2 TexCoords : TEXCOORD0;
    float4 CameraVec : TEXCOORD1;
};

float CalculateScreenSpaceShadows(float3 ViewPos, float3 ToLightDir)
{
    float3 ToLightDirView = normalize(mul(float4(ToLightDir, 0.0), View).xyz);

    const uint StepCount = 12;
    const float RayMaxDistance = 0.05f;
    const float StepLength = RayMaxDistance / StepCount;

    float3 RayPos = ViewPos;
    float3 RayStep = StepLength*ToLightDirView;
    float SSShadowsFactor = 0.0f;
    for(uint I = 0; I < StepCount; I++)
    {
        RayPos += RayStep;

        float4 RayUV = mul(float4(RayPos, 1.0), Projection);
        RayUV.xyz /= RayUV.w;
        RayUV.xy = float2(0.5f, -0.5f)*RayUV.xy + float2(0.5, 0.5);

        if((RayUV.x < 0.0) || (RayUV.x > 1.0) || (RayUV.y < 0.0) || (RayUV.y > 1.0))
            continue;

        float RayDepth = RayPos.z;
        RayDepth -= 0.002f*RayDepth;
        float SceneDepth = LinearDepthTexture.SampleLevel(DefaultSampler, RayUV.xy, 0).r * WorldVectorsToFarCorners[0].w;

        float DistForFullShadow = 0.005f;
        float SurfaceThickness = 0.1f + 0.03f*RayDepth;
        float Factor = saturate((RayDepth - SceneDepth) / DistForFullShadow) * saturate(((SceneDepth + SurfaceThickness) - RayDepth) / DistForFullShadow);
        SSShadowsFactor = max(SSShadowsFactor, Factor);
    }

    return(1.0f - SSShadowsFactor);
}

float4 PS(vs_output Input) : SV_TARGET
{
    float4 FragColor = float4(0.0, 0.0, 0.0, 0.0);

    float3 SunDir = normalize(float3(-1.0, -1.0, 1.0));
    float3 Normal = NormalsTexture.Sample(DefaultSampler, Input.TexCoords).xyz;
    
    float4 RSMIndirectIllum = RSMIndirectIllumTexture.Sample(DefaultSampler, Input.TexCoords);
    float ShadowFactor = RSMIndirectIllum.w;

    float LinearDepth = LinearDepthTexture.Sample(DefaultSampler, Input.TexCoords).r;
    float3 ViewPos = LinearDepth*Input.CameraVec.xyz;
    float SSShadowsFactor = CalculateScreenSpaceShadows(ViewPos, -SunDir);

    float3 Ambient = RSMIndirectIllum.xyz;
    float3 DiffuseColor = SSShadowsFactor * ShadowFactor * max(dot(Normal, -SunDir), 0.0) * ColorTexture.Sample(DefaultSampler, Input.TexCoords).xyz;

    float3 FinalColor = Ambient + DiffuseColor;
    float3 ColorAfterToneMapping = FinalColor / (FinalColor + 1.0);
    FragColor = float4(ColorAfterToneMapping, 1.0);

    return(FragColor);
}