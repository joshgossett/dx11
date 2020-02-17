#include "Structs.hlsli"

Texture2D ObjTexture;
SamplerState ObjSamplerState;

float4 main(VS_OUTPUT input) : SV_TARGET
{
	return ObjTexture.Sample(ObjSamplerState, input.texCoords);
}