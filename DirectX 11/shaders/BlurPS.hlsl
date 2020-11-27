Texture2D Texture : register(t0);

SamplerState Sampler
{
    Filter = MIN_MAG_MIP_LINEAR;
    AddressU = Clamp;
    AddressV = Clamp;
};

struct vs_output
{
    float4 Pos : SV_POSITION;
    float2 TexCoords : TEXCOORD;
};

float4 PS(vs_output Input) : SV_TARGET
{
    float TextureWidth;
    float TextureHeight;
    Texture.GetDimensions(TextureWidth, TextureHeight);
    float2 TexelSize = 1.0 / float2(TextureWidth, TextureHeight);

    float4 FinalColor = float4(0.0, 0.0, 0.0, 0.0);
    for(int X = -2; X <= 1; X++)
    {
        for(int Y = -2; Y <= 1; Y++)
        {
            FinalColor.xyz += Texture.Sample(Sampler, Input.TexCoords + TexelSize*float2(X, Y)).xyz;
        }
    }

    FinalColor *= (1.0 / 16.0);
    FinalColor.w = Texture.Sample(Sampler, Input.TexCoords).w;
    return(FinalColor);
}