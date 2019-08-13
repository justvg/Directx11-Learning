struct vertex_out
{
	float4 Pos : SV_POSITION;
	float3 PosW : POSITION;
	float3 NormalW : NORMAL;
	float2 TexCoords : TEXCOORD;
};

Texture2D DiffuseMap;
SamplerState DefaultSampler;

struct dir_light
{
	float3 Diffuse;
	float3 Specular;
	float3 Dir;
};

struct point_light
{
	float3 Diffuse;
	float3 Specular;
	float3 PosW;
};

struct spot_light
{
	float3 Diffuse;
	float3 Specular;
	float3 PosW;
	float3 Dir;
};

cbuffer light_info
{
	dir_light DirLight;
	point_light PointLight;
	spot_light SpotLight;
	float3 ViewPosW;	
};

float3 CalcDirLight(dir_light DirLight, float3 MaterialColor,  
					float3 Normal, float3 ViewDir)
{
	float3 LightDir = normalize(-DirLight.Dir);

	float3 Ambient = 0.2f * DirLight.Diffuse * MaterialColor;

	float3 Diffuse = DirLight.Diffuse * MaterialColor * max(dot(Normal, LightDir), 0.0f);

	float3 ReflectV = reflect(-LightDir, Normal);
	float3 Specular = DirLight.Specular * MaterialColor * pow(max(dot(ViewDir, ReflectV), 0.0), 4);

	float3 Result = Ambient + Diffuse + Specular;
	return(Result);
}

float AttenuationFactor(float3 LightSourcePos, float3 PointPos)
{
	float F = 1.0f / pow(length(LightSourcePos - PointPos), 2);
	return(F);
}

float3 CalcPointLight(point_light PointLight, float3 PosW, 
					  float3 MaterialColor, float3 Normal, float3 ViewDir)
{
	float3 LightDir = normalize(PointLight.PosW - PosW);
	
	float3 Ambient = 0.2f * PointLight.Diffuse * MaterialColor;
	
	float3 Diffuse = PointLight.Diffuse * MaterialColor * max(dot(Normal, LightDir), 0.0);
	
	float3 ReflectV = reflect(-LightDir, Normal);
	float3 Specular = PointLight.Specular * MaterialColor * pow(max(dot(ViewDir, ReflectV), 0.0), 4);

	float Attenuation = AttenuationFactor(PointLight.PosW, PosW);
	float3 Result = Ambient + Diffuse*Attenuation + Specular*Attenuation;
	return(Result);
}

float3 CalcSpotLight(spot_light SpotLight, float3 PosW,
					 float3 MaterialColor, float3 Normal, float3 ViewDir)
{
	float3 LightDir = normalize(SpotLight.PosW - PosW);

	float3 Ambient = 0.5f * SpotLight.Diffuse * MaterialColor;

	float3 Diffuse = SpotLight.Diffuse * MaterialColor * max(dot(Normal, LightDir), 0.0f);

	float3 ReflectV = reflect(-LightDir, Normal);
	float3 Specular = SpotLight.Specular * MaterialColor * pow(max(dot(ViewDir, ReflectV), 0.0f), 256);

	float Attenuation = AttenuationFactor(SpotLight.PosW, PosW);
	float Spot = pow(max(dot(-LightDir, normalize(SpotLight.Dir)), 0.0f), 96);
	float3 Result = Ambient*Spot + Diffuse*Attenuation + Specular*Attenuation;
	return(Result);
}

float4 PS(vertex_out Input) : SV_TARGET
{
	float3 Normal = normalize(Input.NormalW);
	float3 ViewDir = normalize(ViewPosW - Input.PosW);
	float3 TexColor = (DiffuseMap.Sample(DefaultSampler, Input.TexCoords)).xyz;

	float3 Color = 0.2f * CalcDirLight(DirLight, TexColor, Normal, ViewDir);

	Color += CalcPointLight(PointLight, Input.PosW, TexColor, Normal, ViewDir);

	Color += CalcSpotLight(SpotLight, Input.PosW, TexColor, Normal, ViewDir);

	float4 FragColor = float4(Color, 1.0f);
	return (sqrt(FragColor));
}