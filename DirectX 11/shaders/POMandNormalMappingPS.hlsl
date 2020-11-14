struct vs_output
{
    float4 Pos : SV_POSITION;
    float3 WorldPos : POSITION;
    float3 Normal : NORMAL;
    float2 TexCoords : TEXCOORD;
    float3 Tangent : TANGENT;
    float3 Bitangent : BITANGENT;
};

cbuffer camera_info : register(b1)
{
    float3 CameraPos;
};

Texture2D DiffuseMap : register(t0);
Texture2D NormalMap : register(t1);
Texture2D DisplacementMap : register(t2);
SamplerState DefaultSampler : register(s0); 

float2 ParallaxMapping(float2 TexCoords, float3 ViewDir, float3x3 TBN)
{
    float3 ViewDirTangent;
    ViewDirTangent.x = dot(ViewDir, TBN[0]);
    ViewDirTangent.y = dot(ViewDir, TBN[1]);
    ViewDirTangent.z = dot(ViewDir, TBN[2]);
    ViewDirTangent = normalize(ViewDirTangent);

    // Parallax mapping
    // float Height = DisplacementMap.Sample(DefaultSampler, TexCoords).x;
    // float2 P = 0.1*Height * ViewDirTangent.xy;
    // return TexCoords - P;

    // Parallax occlusion mapping
    const float MinLayersCount = 10.0;
    const float MaxLayersCount = 32.0;
    const float LayersCount = lerp(MaxLayersCount, MinLayersCount, max(dot(float3(0.0, 0.0, 1.0), ViewDirTangent), 0.0));
    float LayerDepth = 1.0 / LayersCount;

    float2 DeltaTexCoords = 0.15*LayerDepth*ViewDirTangent.xy;

    float CurrentLayerDepth = 0.0;
    float2 CurrentTexCoords = TexCoords;
    float CurrentSampledDepth = DisplacementMap.SampleLevel(DefaultSampler, CurrentTexCoords, 0).x;

    while(CurrentSampledDepth > CurrentLayerDepth)
    {
        CurrentLayerDepth += LayerDepth;
        CurrentTexCoords -= DeltaTexCoords;
        CurrentSampledDepth = DisplacementMap.SampleLevel(DefaultSampler, CurrentTexCoords, 0).x;
    }

    float2 PrevTexCoords = CurrentTexCoords + DeltaTexCoords;

    float AfterCollisionDepthDelta = CurrentSampledDepth - CurrentLayerDepth;
    float BeforeCollisionDepthDelta = DisplacementMap.SampleLevel(DefaultSampler, PrevTexCoords, 0).x - (CurrentLayerDepth - LayerDepth);
    float t = AfterCollisionDepthDelta / (AfterCollisionDepthDelta - BeforeCollisionDepthDelta);
    float2 FinalTexCoords = lerp(CurrentTexCoords, PrevTexCoords, t);

    return FinalTexCoords;
}

float4 PS(vs_output Input) : SV_TARGET
{
    float4 FragColor = float4(0.0, 0.0, 0.0, 0.0);

    float3x3 TBN = float3x3(normalize(Input.Tangent), normalize(Input.Bitangent), normalize(Input.Normal));

    float3 ViewDir = normalize(CameraPos - Input.WorldPos);
    float2 TexCoords = ParallaxMapping(Input.TexCoords, ViewDir, TBN);
    if((TexCoords.x > 1.0) || (TexCoords.y > 1.0) || (TexCoords.x < 0.0) || (TexCoords.y < 0.0))
        discard;

    // float3 Normal = normalize(Input.Normal);
    float3 SampledNormal = NormalMap.SampleLevel(DefaultSampler, TexCoords, 0).xyz;
    float3 Normal = normalize(2.0*SampledNormal - float3(1.0, 1.0, 1.0));
    Normal = normalize(mul(Normal, TBN));

    float3 SunDir = normalize(float3(-0.2, -0.2, 1.0));
    float3 DiffuseColor = DiffuseMap.SampleLevel(DefaultSampler, TexCoords, 0).xyz;

    float3 Ambient = 0.1 * DiffuseColor;
    float3 Diffuse = max(dot(Normal, -SunDir), 0.0) * DiffuseColor;

    FragColor = float4(Ambient + Diffuse, 1.0);
    return(FragColor);
}