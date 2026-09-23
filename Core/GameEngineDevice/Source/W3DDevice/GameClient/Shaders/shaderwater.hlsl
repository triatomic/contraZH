// Standing water and rivers, shaded per pixel over a copy of the scene behind them.
//
// The water draws with blending off and composites itself. It refracts the scene copy
// through the waves, tints it by the water's depth over the terrain, and adds a Fresnel
// sky reflection, a sun glint and shore foam.
//
// Fixed-function vertex processing hands out texcoord sets in stage order, so stages 0 to
// 2 each hold a texture. Stage 2 carries the world position, and every later sampler reads
// coordinates computed from it.
//
// RIVER picks the river build. It fades to the untouched scene by the river texture's
// alpha and its edge texture, and one wave layer follows the flow.

sampler2D WaterTexture  : register(s0);
sampler2D EdgeTexture   : register(s1);
sampler2D NormalMap     : register(s2);
sampler2D ShroudTexture : register(s3);
sampler2D HeightTexture : register(s4);
sampler2D SceneTexture  : register(s5);
sampler2D SkyTexture    : register(s6);
sampler2D FoamTexture   : register(s7);

float4 ScreenU       : register(c0);   // world to scene-copy texcoords, before the divide by ScreenW
float4 ScreenV       : register(c1);
float4 ScreenW       : register(c2);
float4 Camera        : register(c3);   // world position, w = time
float4 ToSun         : register(c4);   // world space, w = specular power
float4 SunColor      : register(c5);   // sun colour times specular strength
float4 SkyColor      : register(c6);   // w = reflection strength
float4 HeightMapping : register(c7);   // world xy to height texcoords: xy scale, zw offset
float4 HeightDecode  : register(c8);   // x,y = high and low byte weights, z = wave texcoord scale, w = wave strength
float4 WaterParams   : register(c9);   // x = clarity, y = deep opacity, z = 1 / foam depth, w = refraction strength
float4 Absorption    : register(c10);  // per-channel absorption, red fastest
float4 ShroudMapping : register(c11);  // world xy to shroud texcoords: xy scale, zw offset
float4 SkyMapping    : register(c12);  // x = sky plane height, y = sky texcoord scale, zw = scroll

struct PsIn
{
    float4 Diffuse  : COLOR0;
    float2 BaseUV   : TEXCOORD0;
    float2 EdgeUV   : TEXCOORD1;
    float3 WorldPos : TEXCOORD2;
};

float2 WaveSlope(float2 uv)
{
    return tex2D(NormalMap, uv).rg * 2.0f - 1.0f;
}

float4 main(PsIn input) : COLOR
{
    float3 world = input.WorldPos;
    float time = Camera.w;

    // The height texture holds each terrain height as a high and a low byte.
    float2 heightBytes = tex2D(HeightTexture, world.xy * HeightMapping.xy + HeightMapping.zw).rg;
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
    float3 normal = normalize(float3(slope * HeightDecode.w, 1.0f));

    // Shallow water bends the scene less, so shores and hulls don't smear.
    float invW = 1.0f / dot(float4(world, 1.0f), ScreenW);
    float2 screen = float2(dot(float4(world, 1.0f), ScreenU), dot(float4(world, 1.0f), ScreenV)) * invW;
    float2 bend = slope * WaterParams.w * saturate(depth * 0.125f);
    float3 scene = tex2D(SceneTexture, screen + bend).rgb;

    float4 water = tex2D(WaterTexture, input.BaseUV);
    float3 body = water.rgb * input.Diffuse.rgb;
    float3 transmission = exp(-depth * WaterParams.x * Absorption.rgb);
    float3 opacity = WaterParams.y * (1.0f - transmission);

    float3 toEye = normalize(Camera.xyz - world);
    float facing = saturate(dot(normal, toEye));
    float fresnel = 0.02f + 0.98f * pow(1.0f - facing, 5.0f);
    float reflection = saturate(fresnel * SkyColor.w);

    // The sky is a plane above the water, met along the reflected view ray.
    float3 reflected = reflect(-toEye, normal);
    float2 skyUV = (world.xy + reflected.xy * (SkyMapping.x / max(reflected.z, 0.05f))) * SkyMapping.y + SkyMapping.zw;
    float3 sky = tex2D(SkyTexture, skyUV).rgb * SkyColor.rgb;

    float3 halfway = normalize(ToSun.xyz + toEye);
    float highlight = dot(normal, halfway);
    float glint = pow(saturate(highlight), ToSun.w) + 0.08f * pow(saturate(highlight), ToSun.w * 0.06f);

    float shore = saturate(1.0f - depth * WaterParams.z);
    float foam = tex2D(FoamTexture, world.xy * 0.02f + slope * 0.04f + time * float2(0.011f, -0.007f)).r;
    foam *= shore * shore;

    // The scene copy is already shrouded, so the shroud only darkens the water's own light.
    float3 shroud = tex2D(ShroudTexture, world.xy * ShroudMapping.xy + ShroudMapping.zw).rgb;
    float3 under = scene * (1.0f - opacity) + body * opacity * shroud;
    float3 color = lerp(under, sky * shroud, reflection);
    color += (SunColor.rgb * glint + input.Diffuse.rgb * foam) * shroud;

#if RIVER
    float coverage = water.a * tex2D(EdgeTexture, input.EdgeUV).a;
    color = lerp(tex2D(SceneTexture, screen).rgb, color, coverage);
#endif
    return float4(color, 1.0f);
}
