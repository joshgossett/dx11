struct VS_OUTPUT {
	float4 Pos : SV_POSITION;
	float2 texCoords : TEXCOORD;
};

cbuffer cbPerObject
{
	float4x4 WVP;
};