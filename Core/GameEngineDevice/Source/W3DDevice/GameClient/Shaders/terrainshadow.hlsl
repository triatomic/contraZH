// Terrain passes that also receive the sun's cast shadow.
//
// Each build matches one legacy terrain shader exactly. terrain.nvp blends two
// textures by vertex alpha and applies vertex lighting, and terrainnoise.nvp and
// terrainnoise2.nvp then modulate by one or two cloud or light maps. The shadow
// then scales the colour the way the legacy stencil shadow did, by a factor taken
// from the shadow colour.
//
// NOISE_COUNT (0-2) picks the legacy shader and PACKED picks the depth format.
// The shadow map sits on the first stage after the noise maps. Fixed-function
// vertex processing hands out texcoord sets in stage order, so a gap in the stages
// would move the shadow coordinates into a different register.

#define SHADOW_STAGE (2 + NOISE_COUNT)

sampler2D BaseTexture  : register(s0);
sampler2D BlendTexture : register(s1);

#if NOISE_COUNT >= 1
sampler2D Noise1Texture : register(s2);
#endif
#if NOISE_COUNT >= 2
sampler2D Noise2Texture : register(s3);
#endif

#if SHADOW_STAGE == 2
sampler2D ShadowMap : register(s2);
#define SHADOW_TEXCOORD TEXCOORD2
#elif SHADOW_STAGE == 3
sampler2D ShadowMap : register(s3);
#define SHADOW_TEXCOORD TEXCOORD3
#else
sampler2D ShadowMap : register(s4);
#define SHADOW_TEXCOORD TEXCOORD4
#endif

#include "shadowreceive.hlsli"

struct PsIn
{
    float4 Diffuse   : COLOR0;
    float2 BaseUV    : TEXCOORD0;
    float2 BlendUV   : TEXCOORD1;
#if NOISE_COUNT >= 1
    float2 Noise1UV  : TEXCOORD2;
#endif
#if NOISE_COUNT >= 2
    float2 Noise2UV  : TEXCOORD3;
#endif
    float4 ShadowPos : SHADOW_TEXCOORD;
};

float4 main(PsIn input) : COLOR
{
    float4 color = lerp(tex2D(BaseTexture, input.BaseUV), tex2D(BlendTexture, input.BlendUV), input.Diffuse.a);
    color *= input.Diffuse;
#if NOISE_COUNT >= 1
    color *= tex2D(Noise1Texture, input.Noise1UV);
#endif
#if NOISE_COUNT >= 2
    color *= tex2D(Noise2Texture, input.Noise2UV);
#endif

    color.rgb *= ShadowFactor(input.ShadowPos);
    return color;
}
