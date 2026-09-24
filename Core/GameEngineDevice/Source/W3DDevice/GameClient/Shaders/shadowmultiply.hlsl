// A second pass that multiplies the sun's shadow into geometry already drawn.
//
// For surfaces that render fixed function, such as bridges, where rebuilding the
// whole lighting model in a shader would only reproduce it. The caller blends the
// output as destination times source, so a lit pixel returns white.
//
// PACKED picks the depth format. The shadow map is on stage 0.
// CLOUD multiplies in the terrain's cloud map from stage 1, so objects darken with the ground.

sampler2D ShadowMap : register(s0);

#if CLOUD
sampler2D CloudTexture : register(s1);
#endif

#include "shadowreceive.hlsli"

struct PS_INPUT
{
    float4 ShadowPos : TEXCOORD0;
#if CLOUD
    float2 CloudUV   : TEXCOORD1;
#endif
};

float4 main(PS_INPUT input) : COLOR
{
    float3 color = ShadowFactor(input.ShadowPos);
#if CLOUD
    color *= tex2D(CloudTexture, input.CloudUV).rgb;
#endif
    return float4(color, 1.0f);
}
