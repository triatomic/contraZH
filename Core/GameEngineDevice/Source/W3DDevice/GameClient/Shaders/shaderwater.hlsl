// Standing water and rivers, shaded per pixel over a copy of the scene behind them.
//
// The water draws with blending off and composites itself. It refracts the scene copy
// through the waves, tints it by the water's depth over the terrain, and adds a Fresnel
// reflection, a sun glint and foam. The reflection is the scene mirrored in the water plane
// where W3DWater rendered one, and the map's skybox wherever that mirror is empty or absent.
// The shadow map darkens the glint, the reflection and the water colour where units and
// cliffs shade the water.
//
// Fixed-function vertex processing hands out texcoord sets in stage order, so stages 0 to
// 2 each hold a texture. Stage 2 carries the world position, and every later sampler reads
// coordinates computed from it.
//
// PACKED picks the shadow map's depth format. SWELL picks the ps_3_0 build that follows
// shaderwaterswell.hlsl, whose vertex waves tilt the normal and raise foam on their crests.
// RADIAL adds the water mask for its camera-centred grid, which spans every lake at one level.
// RIVER picks the river build. It fades to the untouched scene by the river texture's
// alpha and its edge texture, and one wave layer follows the flow.

sampler2D WaterTexture  : register(s0);
sampler2D EdgeTexture   : register(s1);
sampler2D NormalMap     : register(s2);
sampler2D ShroudTexture : register(s3);
sampler2D HeightTexture : register(s4);
sampler2D SceneTexture  : register(s5);
sampler2D ShadowMap     : register(s6);
sampler2D FoamTexture   : register(s7);
sampler2D SkyNorth      : register(s8);
sampler2D SkyEast       : register(s9);
sampler2D SkySouth      : register(s10);
sampler2D SkyWest       : register(s11);
sampler2D SkyTop        : register(s12);
sampler2D Reflection    : register(s13);   // mirrored scene at half size, alpha 1 where anything drew
#if RADIAL
sampler2D WaterMask     : register(s14);   // standing water's coverage in alpha, its level at 1/16 unit in red and green times coverage
#endif

// c0 and c4 belong to shadowreceive.hlsli.
float4 ScreenU       : register(c1);   // world to scene-copy texcoords, before the divide by ScreenW
float4 ScreenV       : register(c2);
float4 ScreenW       : register(c3);
float4 Camera        : register(c5);   // world position, w = time
float4 ToSun         : register(c6);   // world space, w = specular power
float4 SunColor      : register(c7);   // sun colour times specular strength
float4 SkyTint       : register(c8);   // the map's light on the skybox, w = reflection strength
float4 HeightMapping : register(c9);   // world xy to height texcoords: xy scale, zw offset
float4 HeightDecode  : register(c10);  // x,y = high and low byte weights, z = wave texcoord scale, w = wave strength
float4 WaterParams   : register(c11);  // x = clarity, y = deep opacity, z = 1 / foam depth, w = refraction strength
float4 Absorption    : register(c12);  // per-channel absorption, red fastest, w = 1 when the shadow map is bound
float4 ShroudMapping : register(c13);  // world xy to shroud texcoords: xy scale, zw offset
float4 ShadowU       : register(c14);  // world to shadow map, one output component each
float4 ShadowV       : register(c15);
float4 ShadowZ       : register(c16);
float4 ShadowW       : register(c17);
float4 Planar        : register(c18);  // x = mirror plane height, y = 1 / fade distance, z = distortion, w = 1 when mirrored
float4 PlanarMap     : register(c19);  // xy = texel centre shift from the scene copy, z = height tolerance, w = added reflection
#if RADIAL
float4 RadialPlane   : register(c20);  // x = water level of this draw
#endif
float4 Shore         : register(c21);  // x = 1 / the depth over which the water's surface light fades in from the shore

#include "shadowreceive.hlsli"

struct PsIn
{
    float4 Diffuse  : COLOR0;
    float2 BaseUV   : TEXCOORD0;
    float2 EdgeUV   : TEXCOORD1;
    float3 WorldPos : TEXCOORD2;
#if SWELL
    float3 Swell    : TEXCOORD3;   // xy = slope, z = height from -1 to 1
#endif
};

float2 WaveSlope(float2 uv)
{
    return tex2D(NormalMap, uv).rg * 2.0f - 1.0f;
}

// The skybox is a box around the camera with five faces, no bottom, laid out as new_skybox.w3d
// maps them. Every face is sampled and the one the ray leaves through is kept.
float3 SkyboxColor(float3 dir)
{
    dir.z = max(dir.z, 0.02f);
    float3 a = abs(dir);
    float2 pX = dir.yz / a.x;
    float2 pY = dir.xz / a.y;
    float2 pZ = dir.xy / a.z;

    float3 north = tex2D(SkyNorth, float2(1.0f - pX.x, 1.0f - pX.y) * 0.5f).rgb;
    float3 south = tex2D(SkySouth, float2(1.0f + pX.x, 1.0f - pX.y) * 0.5f).rgb;
    float3 east  = tex2D(SkyEast,  float2(1.0f + pY.x, 1.0f - pY.y) * 0.5f).rgb;
    float3 west  = tex2D(SkyWest,  float2(1.0f - pY.x, 1.0f - pY.y) * 0.5f).rgb;
    float3 top   = tex2D(SkyTop,   float2(1.0f - pZ.x, 1.0f + pZ.y) * 0.5f).rgb;

    float3 side = (a.x >= a.y) ? ((dir.x < 0.0f) ? north : south) : ((dir.y < 0.0f) ? east : west);
    return (a.z >= max(a.x, a.y)) ? top : side;
}

float4 main(PsIn input) : COLOR
{
    float3 world = input.WorldPos;
    float4 worldPoint = float4(world, 1.0f);
    float time = Camera.w;
    float2 mapUV = world.xy * HeightMapping.xy + HeightMapping.zw;

#if RADIAL
    // Dividing by coverage undoes the premultiply, so filtering blends only covered levels.
    float4 mask = tex2D(WaterMask, mapUV);
    float maskLevel = dot(mask.rg, float2(255.0f * 256.0f / 16.0f, 255.0f / 16.0f)) / max(mask.a, 0.001f);
    clip(mask.a - 0.5f);
    clip(0.5f - abs(maskLevel - RadialPlane.x));
#endif

    // The height texture holds each terrain height as a high and a low byte.
    float2 heightBytes = tex2D(HeightTexture, mapUV).rg;
    float depth = max(world.z - dot(heightBytes, HeightDecode.xy), 0.0f);

    float2 waveUV = world.xy * HeightDecode.z;
    float2 slope = WaveSlope(waveUV + time * float2(0.031f, 0.017f));
    slope += WaveSlope(waveUV * 2.7f + time * float2(-0.023f, 0.037f));
#if RIVER
    slope += WaveSlope(input.BaseUV * float2(1.0f, 2.0f));
    slope *= 0.33f;
#else
    slope *= 0.5f;
#endif
#if SWELL
    float3 normal = normalize(float3(slope * HeightDecode.w + input.Swell.xy, 1.0f));
#else
    float3 normal = normalize(float3(slope * HeightDecode.w, 1.0f));
#endif

    // A bent sample far from the straight one likely landed on something standing in or
    // over the water, which must not smear into it, so the straight sample is kept there.
    float invW = 1.0f / dot(worldPoint, ScreenW);
    float2 screen = float2(dot(worldPoint, ScreenU), dot(worldPoint, ScreenV)) * invW;
    float2 bend = slope * WaterParams.w * saturate(depth * 0.125f);
    float3 straight = tex2D(SceneTexture, screen).rgb;
    float3 bent = tex2D(SceneTexture, screen + bend).rgb;
    float3 delta = bent - straight;
    float3 scene = lerp(straight, bent, saturate((0.15f - dot(delta, delta)) * 20.0f));

    float4 shadowPos = float4(dot(worldPoint, ShadowU), dot(worldPoint, ShadowV), dot(worldPoint, ShadowZ), dot(worldPoint, ShadowW));
    float lit = lerp(1.0f, ShadowLit(shadowPos), Absorption.w);
    float3 shade = lerp(ShadowColor.rgb, float3(1.0f, 1.0f, 1.0f), lit);

    float4 water = tex2D(WaterTexture, input.BaseUV);
    float3 body = water.rgb * input.Diffuse.rgb * shade;
    float3 transmission = exp(-depth * WaterParams.x * Absorption.rgb);
    float3 opacity = WaterParams.y * (1.0f - transmission);

    float3 toEye = normalize(Camera.xyz - world);
    float facing = saturate(dot(normal, toEye));
    float fresnel = 0.02f + 0.98f * pow(1.0f - facing, 5.0f);
    float3 sky = SkyboxColor(reflect(-toEye, normal)) * SkyTint.rgb * lerp(0.6f, 1.0f, lit);

    // Water off the mirror plane, as on other lakes or sloping rivers, keeps the skybox.
    float4 mirror = tex2D(Reflection, screen + PlanarMap.xy + normal.xy * Planar.z);
    float onPlane = saturate(1.0f - max(abs(world.z - Planar.x) - PlanarMap.z, 0.0f) * Planar.y);
    float mirrored = mirror.a * onPlane * Planar.w;
    sky = lerp(sky, mirror.rgb * lerp(0.6f, 1.0f, lit), mirrored);

    // Some water colour always shows through.
    float reflection = min(fresnel * SkyTint.w + mirrored * PlanarMap.w, 0.8f);

    float3 halfway = normalize(ToSun.xyz + toEye);
    float highlight = dot(normal, halfway);
    float glint = (pow(saturate(highlight), ToSun.w) + 0.08f * pow(saturate(highlight), ToSun.w * 0.06f)) * lit;

    float foamMask = saturate(1.0f - depth * WaterParams.z);
    foamMask *= foamMask;
#if SWELL
    foamMask = max(foamMask, saturate((input.Swell.z - 0.35f) * 2.5f));
#endif
    float2 foamUV = world.xy * 0.02f + slope * 0.04f;
    float foam = tex2D(FoamTexture, foamUV + time * float2(0.011f, -0.007f)).r;
    foam *= tex2D(FoamTexture, foamUV * 0.8f - time * float2(0.006f, 0.009f)).r * 2.0f;
    foam *= foamMask;

    // As the legacy soft water edge did, the surface fades out at the waterline instead of ending in a line.
    float edge = saturate(depth * Shore.x);
    reflection *= edge;
    glint *= edge;
    foam *= edge;

    // The scene copy is already shrouded, so the shroud only darkens the water's own light.
    float3 shroud = tex2D(ShroudTexture, world.xy * ShroudMapping.xy + ShroudMapping.zw).rgb;
    float3 under = scene * (1.0f - opacity) + body * opacity * shroud;
    float3 color = lerp(under, sky * shroud, reflection);
    color += (SunColor.rgb * glint + input.Diffuse.rgb * shade * foam) * shroud;

#if RIVER
    float coverage = water.a * tex2D(EdgeTexture, input.EdgeUV).a;
    color = lerp(straight, color, coverage);
#endif
    return float4(color, 1.0f);
}
