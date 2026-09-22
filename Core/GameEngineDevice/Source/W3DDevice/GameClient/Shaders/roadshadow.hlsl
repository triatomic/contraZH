// Road passes that also receive the sun's cast shadow.
//
// Every legacy road mode is the same product, the road texture times each cloud or
// light map times the vertex diffuse, on colour and alpha alike. roadnoise2.nvp
// does it for both maps and the fixed-function two-stage path does it for fewer.
// Roads blend onto terrain by their alpha, so the shadow scales colour only.
//
// NOISE_COUNT (0-2) is how many maps apply and PACKED picks the depth format. The
// shadow map sits on the first stage after the maps, because fixed-function vertex
// processing hands out texcoord sets in stage order.

#define SHADOW_STAGE (1 + NOISE_COUNT)

sampler2D RoadTexture : register(s0);

#if NOISE_COUNT >= 1
sampler2D Noise1Texture : register(s1);
#endif
#if NOISE_COUNT >= 2
sampler2D Noise2Texture : register(s2);
#endif

#if SHADOW_STAGE == 1
sampler2D ShadowMap : register(s1);
#define SHADOW_TEXCOORD TEXCOORD1
#elif SHADOW_STAGE == 2
sampler2D ShadowMap : register(s2);
#define SHADOW_TEXCOORD TEXCOORD2
#else
sampler2D ShadowMap : register(s3);
#define SHADOW_TEXCOORD TEXCOORD3
#endif

#include "shadowreceive.hlsli"

struct PsIn
{
    float4 Diffuse   : COLOR0;
    float2 RoadUV    : TEXCOORD0;
#if NOISE_COUNT >= 1
    float2 Noise1UV  : TEXCOORD1;
#endif
#if NOISE_COUNT >= 2
    float2 Noise2UV  : TEXCOORD2;
#endif
    float4 ShadowPos : SHADOW_TEXCOORD;
};

float4 main(PsIn input) : COLOR
{
    float4 color = tex2D(RoadTexture, input.RoadUV) * input.Diffuse;
#if NOISE_COUNT >= 1
    color *= tex2D(Noise1Texture, input.Noise1UV);
#endif
#if NOISE_COUNT >= 2
    color *= tex2D(Noise2Texture, input.Noise2UV);
#endif

    color.rgb *= ShadowFactor(input.ShadowPos);
    return color;
}
