#include "Structs.hlsli"

VS_OUTPUT main(float4 pos : POSITION, float2 texc : TEXCOORD)
{
	VS_OUTPUT output;
	output.Pos = mul(pos, WVP);
	output.texCoords = texc;
	return output;
}