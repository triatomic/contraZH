// Road passes that also receive the sun's cast shadow, or add dynamic point lights, or both.
//
// Every legacy road mode is the same product, the road texture times each cloud or
// light map times the vertex diffuse, on colour and alpha alike. roadnoise2.nvp
// does it for both maps and the fixed-function two-stage path does it for fewer.
// Roads blend onto terrain by their alpha, so the shadow scales colour only.
//
// NOISE_COUNT (0-2) is how many maps apply and PACKED picks the depth format. With
// two, the second is W3DGroundNoise's texture, read through groundnoise.hlsli, and the
// first is the cloud map or white.
// SHADOWED (default 1) picks whether the shadow map is read at all. The shadow map
// sits on the first stage after the maps, because fixed-function vertex processing
// hands out texcoord sets in stage order.
//
// Blend tiles of three textures draw through these too, laid over the terrain by their alpha,
// which heightblend.hlsli shapes by the top texture's height against a middling one below.
// Roads set its constants to leave their alpha alone.
//
// LIGHTS adds the point lights to the vertex lighting. They need the world position,
// on the stage after the shadow map, and take ps_2_a for their length.
//
// GLINT adds the sun's glint as the terrain has it. The lit and bumped builds have it, and GLINT
// alone gives the world position to roads without lights.
//
// BUMP 1 adds a normal map on the stage after the world position, read with the road UVs. Roads
// read their texture's _nrm.dds and blend tiles the terrain's normal atlas. BUMP 2 takes the
// road texture's brightness as height instead, for roads without a normal map, through the slope
// map W3DSlopeMap builds from it on the normal map's stage. As on the terrain,
// only the sun's share of the vertex lighting is redone, and the frame comes from derivatives.

#ifndef SHADOWED
#define SHADOWED 1
#endif

#ifndef LIGHTS
#define LIGHTS 0
#endif

#ifndef BUMP
#define BUMP 0
#endif

#ifndef GLINT
#define GLINT (LIGHTS || BUMP)
#endif

#define CONCAT_(a, b) a##b
#define CONCAT(a, b) CONCAT_(a, b)

#define SHADOW_STAGE (1 + NOISE_COUNT)

#define HEIGHT_BLEND_REGISTER c1
#include "heightblend.hlsli"

sampler2D RoadTexture : register(s0);

#if NOISE_COUNT >= 1
sampler2D Noise1Texture : register(s1);
#endif
#if NOISE_COUNT >= 2
sampler2D Noise2Texture : register(s2);
#endif

#if NOISE_COUNT >= 2
#include "groundnoise.hlsli"
#endif

#if SHADOWED

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

#endif

#if LIGHTS || GLINT

// Stage numbers, spelled out because register names need a literal digit.
#if NOISE_COUNT + SHADOWED == 0
#define POSITION_INDEX 1
#define NORMAL_INDEX 2
#elif NOISE_COUNT + SHADOWED == 1
#define POSITION_INDEX 2
#define NORMAL_INDEX 3
#elif NOISE_COUNT + SHADOWED == 2
#define POSITION_INDEX 3
#define NORMAL_INDEX 4
#else
#define POSITION_INDEX 4
#define NORMAL_INDEX 5
#endif

#endif

#if GLINT
float4 ToSun    : register(c2);   // world space, w = normal map strength
float4 SunColor : register(c3);   // the sun's diffuse colour in the vertex lighting, w = 1 for the debug view
#include "terrainglint.hlsli"
#endif

#if BUMP == 1

sampler2D NormalMap : register(CONCAT(s, NORMAL_INDEX));

// Schuler's cotangent frame from the UV derivatives, since roads bend and turn their UVs with them.
// z comes back from unit length, as the atlas has none. HeightBlend.w is 1 for the terrain atlas,
// which holds y in alpha, and 0 for a _nrm.dds, which holds it in green.
float3 BumpNormal(float3 normal, float3 position, float2 uv)
{
    float3 dpdx = ddx(position);
    float3 dpdy = ddy(position);
    float3 dpdyPerp = cross(dpdy, normal);
    float3 dpdxPerp = cross(normal, dpdx);
    float2 duvdx = ddx(uv);
    float2 duvdy = ddy(uv);
    float3 tangent = dpdyPerp * duvdx.x + dpdxPerp * duvdy.x;
    float3 bitangent = dpdyPerp * duvdx.y + dpdxPerp * duvdy.y;
    float side = (dot(dpdx, dpdyPerp) < 0.0f) ? -1.0f : 1.0f;
    float scale = side * ToSun.w * rsqrt(max(max(dot(tangent, tangent), dot(bitangent, bitangent)), 1e-30f));

    float4 texel = tex2D(NormalMap, uv);
    float2 xy = float2(texel.r, lerp(texel.g, texel.a, HeightBlend.w)) * 2.0f - 1.0f;
    float3 bumped = (tangent * xy.x + bitangent * xy.y) * scale + normal * sqrt(saturate(1.0f - dot(xy, xy)));
    return (dot(bumped, bumped) > 1e-20f) ? normalize(bumped) : normal;
}

#elif BUMP == 2

// Only roads take derived bumps, and their height blend leaves alpha alone, so its register holds
// x = rise of full brightness in world units and yz = the slope map's stored texel to height per uv.

sampler2D SlopeMap : register(CONCAT(s, NORMAL_INDEX));

// Mikkelsen's surface gradient, with the height change per pixel from the slope map's change per uv.
float3 BumpNormal(float3 normal, float3 position, float2 uv)
{
    float3 dpdx = ddx(position);
    float3 dpdy = ddy(position);
    float3 r1 = cross(dpdy, normal);
    float3 r2 = cross(normal, dpdx);
    float det = dot(dpdx, r1);

    float2 slope = (tex2D(SlopeMap, uv).rg - 128.0f / 255.0f) * HeightBlend.yz;
    float3 gradient = sign(det) * (dot(slope, ddx(uv)) * r1 + dot(slope, ddy(uv)) * r2) * HeightBlend.x;
    float3 bumped = abs(det) * normal - gradient;
    return (dot(bumped, bumped) > 1e-20f) ? normalize(bumped) : normal;
}

#endif

#if LIGHTS

// Eight fill c5 to c22 before the glint's c23 to c25, and fxc needs the rest for literals, so W3DShaderManager::MAX_PIXEL_LIGHTS must match.
#define POINT_LIGHT_REGISTER c5
#define POINT_LIGHT_COUNT 8
#include "pointlights.hlsli"

#endif

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
#if SHADOWED
    float4 ShadowPos : SHADOW_TEXCOORD;
#endif
#if LIGHTS || GLINT
    float3 WorldPos  : CONCAT(TEXCOORD, POSITION_INDEX);
#endif
};

float4 main(PsIn input) : COLOR
{
    float4 color = tex2D(RoadTexture, input.RoadUV);
#if BUMP == 2
    float weight = input.Diffuse.a;
#else
    float weight = HeightBlendWeight(input.Diffuse.a, 0.5f, tex2D(HeightAtlas, input.RoadUV).r);
#endif
#if GLINT
    float glintStrength = dot(color.rgb, GlintAlbedo.xyz) + GlintAlbedo.w;
#endif

#if SHADOWED
    float lit = ShadowLit(input.ShadowPos);
#else
    float lit = 1.0f;
#endif

#if LIGHTS || BUMP
    // Roads lie on the terrain, so their facet's normal is turned up.
    float3 facet = cross(ddx(input.WorldPos), ddy(input.WorldPos));
    facet *= (facet.z < 0.0f) ? -1.0f : 1.0f;
    float3 normal = facet * rsqrt(max(dot(facet, facet), 1e-30f));
    float3 light = input.Diffuse.rgb;
#endif

#if BUMP
    // The vertex lighting holds the sun on the smooth surface. The bump only changes the sun's share.
    float3 bumped = BumpNormal(normal, input.WorldPos, input.RoadUV);
    float change = saturate(dot(bumped, ToSun.xyz)) - saturate(dot(normal, ToSun.xyz));
    light += SunColor.rgb * change * lit;
#elif LIGHTS
    float3 bumped = normal;
#endif

#if LIGHTS && LIGHTS_DEBUG
    return float4(PointLighting(input.WorldPos, bumped), color.a * weight);
#elif LIGHTS
    light += PointLighting(input.WorldPos, bumped);
#endif

#if LIGHTS || BUMP
    color.rgb *= saturate(light);
#else
    color.rgb *= input.Diffuse.rgb;
#endif
    color.a *= weight;

#if NOISE_COUNT >= 1
    float4 cloud = tex2D(Noise1Texture, input.Noise1UV);
#if !SHADOWED
    color *= cloud;
#endif
#endif
#if NOISE_COUNT >= 2
    color.rgb *= GroundNoise(Noise2Texture, input.Noise2UV);
#endif

#if SHADOWED && NOISE_COUNT >= 1
    color.rgb *= CloudShadowFactor(lit, cloud.rgb);
#elif SHADOWED
    color.rgb *= lerp(ShadowColor.rgb, float3(1.0f, 1.0f, 1.0f), lit);
#endif

#if GLINT
    // The bump tilts the smooth normal as far as it tilts the facet's.
#if BUMP
    float3 glintNormal = normalize(GlintNormal(input.WorldPos) + bumped - normal);
#else
    float3 glintNormal = GlintNormal(input.WorldPos);
#endif
    float glint = Glint(input.WorldPos, glintNormal, glintStrength * lit, GlintEye.w);
#if NOISE_COUNT >= 1
    color.rgb += SunColor.rgb * cloud.rgb * glint;
#else
    color.rgb += SunColor.rgb * glint;
#endif
#endif

#if BUMP
    // The debug view shows only the bump's shading, 4x, on grey.
    color.rgb = lerp(color.rgb, saturate(0.5f + change * 4.0f).xxx, SunColor.w);
#endif
    return color;
}
