struct vs_out
{
	float4 Pos : SV_POSITION;
	float3 Normal : NORMAL;
};

struct gs_out
{
	float4 Pos : SV_POSITION;
};

[maxvertexcount(6)]
void GS(triangle vs_out Input[3],
		inout LineStream<gs_out> Output)
{
	gs_out TempOut[2];

	TempOut[0].Pos = Input[0].Pos;
	TempOut[1].Pos = Input[0].Pos + float4(Input[0].Normal, 0.0) * 0.4;
	Output.Append(TempOut[0]);
	Output.Append(TempOut[1]);
	Output.RestartStrip();

	TempOut[0].Pos = Input[1].Pos;
	TempOut[1].Pos = Input[1].Pos + float4(Input[1].Normal, 0.0) * 0.4;
	Output.Append(TempOut[0]);
	Output.Append(TempOut[1]);
	Output.RestartStrip();

	TempOut[0].Pos = Input[2].Pos;
	TempOut[1].Pos = Input[2].Pos + float4(Input[2].Normal, 0.0) * 0.4;
	Output.Append(TempOut[0]);
	Output.Append(TempOut[1]);
}