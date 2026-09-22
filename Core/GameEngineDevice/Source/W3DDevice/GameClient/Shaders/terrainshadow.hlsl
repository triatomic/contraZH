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

// x = shadow map texel size, y = depth bias, z = shadow strength, w = unused
float4 ShadowParams : register(c0);

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

float4 TerrainColor(PsIn input)
{
    float4 color = lerp(tex2D(BaseTexture, input.BaseUV), tex2D(BlendTexture, input.BlendUV), input.Diffuse.a);
    color *= input.Diffuse;
#if NOISE_COUNT >= 1
    color *= tex2D(Noise1Texture, input.Noise1UV);
#endif
#if NOISE_COUNT >= 2
    color *= tex2D(Noise2Texture, input.Noise2UV);
#endif
    return color;
}

#if PACKED

// Four point-sampled compares, weighted by where the pixel sits inside its texel.
// That weighting, not the tap count, is what stops shadow edges stair-stepping,
// because a box of point compares snaps every tap to the texel grid.
float SampleShadow(float2 uv, float depth)
{
    const float4 unshift = float4(1.0f, 1.0f / 255.0f, 1.0f / (255.0f * 255.0f),
                                  1.0f / (255.0f * 255.0f * 255.0f));
    float texel = ShadowParams.x;
    float2 texelPos = uv / texel - 0.5f;
    float2 weight = frac(texelPos);
    float2 base = (texelPos - weight + 0.5f) * texel;

    float lit00 = (depth <= dot(tex2D(ShadowMap, base), unshift)) ? 1.0f : 0.0f;
    float lit10 = (depth <= dot(tex2D(ShadowMap, base + float2(texel, 0.0f)), unshift)) ? 1.0f : 0.0f;
    float lit01 = (depth <= dot(tex2D(ShadowMap, base + float2(0.0f, texel)), unshift)) ? 1.0f : 0.0f;
    float lit11 = (depth <= dot(tex2D(ShadowMap, base + float2(texel, texel)), unshift)) ? 1.0f : 0.0f;

    return lerp(lerp(lit00, lit10, weight.x), lerp(lit01, lit11, weight.x), weight.y);
}

#else

// The sampler does the compare on a depth texture, and its bilinear filter gives
// 2x2 PCF for one instruction.
float SampleShadow(float2 uv, float depth)
{
    return tex2Dproj(ShadowMap, float4(uv, depth, 1.0f)).r;
}

#endif

float4 main(PsIn input) : COLOR
{
    // Guarded because a zero w turns every later op into NaN, which draws black.
    float w = max(abs(input.ShadowPos.w), 1e-6f);
    float2 uv = input.ShadowPos.xy / w;
    float depth = input.ShadowPos.z / w - ShadowParams.y;

    // Outside the sun's box nothing is shadowed. Applied as a mask rather than an
    // early out so the texture fetches stay in uniform flow.
    float inside = (uv.x >= 0.0f && uv.x <= 1.0f && uv.y >= 0.0f && uv.y <= 1.0f) ? 1.0f : 0.0f;
    float lit = lerp(1.0f, SampleShadow(uv, depth), inside);

    float4 color = TerrainColor(input);
    color.rgb *= lerp(1.0f, lit, ShadowParams.z);
    return color;
}
