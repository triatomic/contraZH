// Terrain passes that also receive the sun's cast shadow, or bump the sun's light, or both.
//
// Each build matches one legacy terrain shader exactly. terrain.nvp blends two
// textures by vertex alpha and applies vertex lighting, and terrainnoise.nvp and
// terrainnoise2.nvp then modulate by one or two cloud or light maps. The shadow
// then scales the colour the way the legacy stencil shadow did, by a factor taken
// from the shadow colour.
//
// NOISE_COUNT (0-2) picks the legacy shader and PACKED picks the depth format.
// SHADOWED (default 1) picks whether the shadow map is read at all. The shadow map
// sits on the first stage after the noise maps. Fixed-function vertex processing
// hands out texcoord sets in stage order, so a gap in the stages would move the
// shadow coordinates into a different register.
//
// BUMP adds the terrain normal maps, laid out like the colour atlas so the base and
// blend UVs index them too. The world position comes on the next stage and the
// normal atlas on the one after. The vertex lighting stays, and only the sun's share
// is redone per pixel, so the normal maps need ps_2_a for the derivatives that build
// their frame.
//
// LIGHTS adds the dynamic point lights the vertex lighting leaves out. They need the
// world position, on the same stage as for BUMP, and take ps_2_a for their length.

#ifndef SHADOWED
#define SHADOWED 1
#endif

#ifndef BUMP
#define BUMP 0
#endif

#ifndef LIGHTS
#define LIGHTS 0
#endif

#define CONCAT_(a, b) a##b
#define CONCAT(a, b) CONCAT_(a, b)

#define SHADOW_STAGE (2 + NOISE_COUNT)

sampler2D BaseTexture  : register(s0);
sampler2D BlendTexture : register(s1);

#if NOISE_COUNT >= 1
sampler2D Noise1Texture : register(s2);
#endif
#if NOISE_COUNT >= 2
sampler2D Noise2Texture : register(s3);
#endif

#if SHADOWED

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

#endif

#if BUMP || LIGHTS

// Stage numbers, spelled out because register names need a literal digit.
#if NOISE_COUNT + SHADOWED == 0
#define POSITION_INDEX 2
#define NORMAL_INDEX 3
#elif NOISE_COUNT + SHADOWED == 1
#define POSITION_INDEX 3
#define NORMAL_INDEX 4
#elif NOISE_COUNT + SHADOWED == 2
#define POSITION_INDEX 4
#define NORMAL_INDEX 5
#else
#define POSITION_INDEX 5
#define NORMAL_INDEX 6
#endif

#endif

#if LIGHTS
// Nine fill c5 to c25, and fxc needs the rest for literals, so W3DShaderManager::MAX_PIXEL_LIGHTS must match.
#define POINT_LIGHT_REGISTER c5
#define POINT_LIGHT_COUNT 9
#include "pointlights.hlsli"
#endif

#if BUMP

sampler2D NormalAtlas : register(CONCAT(s, NORMAL_INDEX));

float4 ToSun      : register(c1);   // world space
float4 SunColor   : register(c2);   // the sun's diffuse colour in the vertex lighting
float4 BumpParams : register(c3);   // x = normal map strength, y = 1 for the debug view

// The atlas holds x in luminance and y in alpha, and z comes back from unit length.
float3 AtlasNormal(float2 uv)
{
    float4 texel = tex2D(NormalAtlas, uv);
    float2 xy = float2(texel.r, texel.a) * 2.0f - 1.0f;
    return float3(xy * BumpParams.x, sqrt(saturate(1.0f - dot(xy, xy))));
}

// Schuler's cotangent frame from the UV derivatives, so cliff cells with their own
// UV layout bump the right way too. The determinant's sign keeps it the right way round.
float3 BumpNormal(float3 normal, float3 dpdyPerp, float3 dpdxPerp, float side, float2 uv)
{
    float2 duvdx = ddx(uv);
    float2 duvdy = ddy(uv);
    float3 tangent = dpdyPerp * duvdx.x + dpdxPerp * duvdy.x;
    float3 bitangent = dpdyPerp * duvdx.y + dpdxPerp * duvdy.y;
    float scale = side * rsqrt(max(max(dot(tangent, tangent), dot(bitangent, bitangent)), 1e-30f));

    float3 texel = AtlasNormal(uv);
    return (tangent * texel.x + bitangent * texel.y) * scale + normal * texel.z;
}

#endif

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
#if SHADOWED
    float4 ShadowPos : SHADOW_TEXCOORD;
#endif
#if BUMP || LIGHTS
    float3 WorldPos  : CONCAT(TEXCOORD, POSITION_INDEX);
#endif
};

float4 main(PsIn input) : COLOR
{
    float4 color = lerp(tex2D(BaseTexture, input.BaseUV), tex2D(BlendTexture, input.BlendUV), input.Diffuse.a);

#if SHADOWED
    float lit = ShadowLit(input.ShadowPos);
#else
    float lit = 1.0f;
#endif

#if BUMP || LIGHTS
    // The facet's own normal, turned up, since terrain never faces down.
    float3 dpdx = ddx(input.WorldPos);
    float3 dpdy = ddy(input.WorldPos);
    float3 facet = cross(dpdx, dpdy);
    facet *= (facet.z < 0.0f) ? -1.0f : 1.0f;
    float3 normal = facet * rsqrt(max(dot(facet, facet), 1e-30f));
#endif

#if BUMP
    float3 dpdyPerp = cross(dpdy, normal);
    float3 dpdxPerp = cross(normal, dpdx);
    float side = (dot(dpdx, dpdyPerp) < 0.0f) ? -1.0f : 1.0f;

    float3 bumped = lerp(BumpNormal(normal, dpdyPerp, dpdxPerp, side, input.BaseUV),
                         BumpNormal(normal, dpdyPerp, dpdxPerp, side, input.BlendUV), input.Diffuse.a);
    bumped = (dot(bumped, bumped) > 1e-20f) ? normalize(bumped) : normal;

    // The vertex lighting holds the sun on the smooth surface. The bump only changes the sun's share.
    float change = saturate(dot(bumped, ToSun.xyz)) - saturate(dot(normal, ToSun.xyz));
    float3 light = input.Diffuse.rgb + SunColor.rgb * change * lit;
#elif LIGHTS
    float3 bumped = normal;
    float3 light = input.Diffuse.rgb;
#endif

#if LIGHTS
    light += PointLighting(input.WorldPos, bumped);
#endif

#if BUMP || LIGHTS
    color.rgb *= saturate(light);
    color.a *= input.Diffuse.a;
#else
    color *= input.Diffuse;
#endif

#if NOISE_COUNT >= 1
    color *= tex2D(Noise1Texture, input.Noise1UV);
#endif
#if NOISE_COUNT >= 2
    color *= tex2D(Noise2Texture, input.Noise2UV);
#endif

#if SHADOWED
    color.rgb *= lerp(ShadowColor.rgb, float3(1.0f, 1.0f, 1.0f), lit);
#endif

#if BUMP
    // The debug view shows only the bump's shading, 4x, on grey.
    color.rgb = lerp(color.rgb, saturate(0.5f + change * 4.0f).xxx, BumpParams.y);
#endif
    return color;
}
