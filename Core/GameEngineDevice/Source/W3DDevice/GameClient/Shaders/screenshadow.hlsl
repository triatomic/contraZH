// One full-screen pass that multiplies the sun's shadow into the opaque scene.
//
// Each pixel's position comes from the scene's INTZ depth, so no receiver redraws its
// geometry. The caller blends the output as destination times source.

sampler2D SceneDepth : register(s0);
sampler2D ShadowMap  : register(s1);

#include "shadowreceive.hlsli"

float4 ToView0 : register(c5);    // depth texture uv and stored depth to homogeneous camera space, one row each
float4 ToView1 : register(c6);
float4 ToView2 : register(c7);
float4 ToView3 : register(c8);
float4 ToMap0  : register(c9);    // camera space to the shadow map's uv and depth, one row each
float4 ToMap1  : register(c10);
float4 ToMap2  : register(c11);
float4 ToMap3  : register(c12);

float4 main(float2 uv : TEXCOORD0) : COLOR
{
    float stored = tex2D(SceneDepth, uv).r;
    float4 position = uv.x * ToView0 + uv.y * ToView1 + stored * ToView2 + ToView3;
    float3 view = position.xyz / position.w;

    float4 shadowPos = view.x * ToMap0 + view.y * ToMap1 + view.z * ToMap2 + ToMap3;
    return float4(ShadowFactor(shadowPos), 1.0f);
}
