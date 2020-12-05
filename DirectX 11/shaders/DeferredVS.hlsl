struct vs_input
{
    float3 Pos : POSITION;
};

struct vs_output
{
    float4 Pos : SV_POSITION;
    float2 TexCoords : TEXCOORD0;
    float4 CameraVec : TEXCOORD1;
};

cbuffer camera_info : register(b0)
{
    float4 WorldVectorsToFarCorners[4]; // NOTE(georgy): W of these vectors contain FarPlane distance
    float4 CameraWorldPos;
};

vs_output VS(vs_input Input, uint ID: SV_VertexID)
{
    vs_output Output;
    
    Output.Pos = float4(Input.Pos, 1.0);
    Output.TexCoords = float2(0.5, -0.5)*Input.Pos.xy + float2(0.5, 0.5);
    Output.CameraVec = WorldVectorsToFarCorners[ID];

    return(Output);
}