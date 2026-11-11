// One full-screen pass that multiplies the sun's shadow into the scene's objects.
//
// Each pixel's position comes from the scene's INTZ depth, so no object redraws its
// geometry to receive. The caller blends the output as destination times source and
// limits the draw to object pixels by stencil.
//
// CLOUD multiplies in the terrain's cloud map from stage 2, so objects darken with the ground.

sampler2D SceneDepth : register(s0);
sampler2D ShadowMap  : register(s1);

#if CLOUD
sampler2D CloudTexture : register(s2);
#endif

#include "shadowreceive.hlsli"

float4 ToView0 : register(c5);    // depth texture uv and stored depth to homogeneous camera space, one row each
float4 ToView1 : register(c6);
float4 ToView2 : register(c7);
float4 ToView3 : register(c8);
float4 ToMap0  : register(c9);    // camera space to the shadow map's uv and depth, one row each
float4 ToMap1  : register(c10);
float4 ToMap2  : register(c11);
float4 ToMap3  : register(c12);

#if CLOUD
float4 ToCloudU : register(c13);  // camera space to the cloud map's uv, one column each
float4 ToCloudV : register(c14);
#endif

float4 main(float2 uv : TEXCOORD0) : COLOR
{
    float stored = tex2D(SceneDepth, uv).r;
    float4 position = uv.x * ToView0 + uv.y * ToView1 + stored * ToView2 + ToView3;
    float3 view = position.xyz / position.w;

    float4 shadowPos = view.x * ToMap0 + view.y * ToMap1 + view.z * ToMap2 + ToMap3;

#if CLOUD
    float2 cloudUV = float2(dot(float4(view, 1.0f), ToCloudU), dot(float4(view, 1.0f), ToCloudV));
    float3 color = CloudShadowFactor(ShadowLit(shadowPos), tex2D(CloudTexture, cloudUV).rgb);

    // The sky and anything else beyond the map takes no cloud, since the map covers the ground in view.
    float2 mapUV = shadowPos.xy / shadowPos.w;
    float inside = (mapUV.x >= 0.0f && mapUV.x <= 1.0f && mapUV.y >= 0.0f && mapUV.y <= 1.0f) ? 1.0f : 0.0f;
    color = lerp(float3(1.0f, 1.0f, 1.0f), color, inside);
#else
    float3 color = ShadowFactor(shadowPos);
#endif
    return float4(color, 1.0f);
}
