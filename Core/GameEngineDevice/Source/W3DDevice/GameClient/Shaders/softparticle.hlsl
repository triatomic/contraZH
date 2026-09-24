// Draws a particle sprite fading out where it nears the surface behind it, so smoke and fire
// meet the ground and buildings without a hard line.
//
// The sprite keeps its fixed-function colour, texture times vertex colour. Its vertices are
// already in camera space, and stage 1 hands that position over as TEXCOORD1.
//
// SOFT turns the fade on. DEPTH picks the surface. 1 reads the scene's INTZ depth from stage 1,
// which covers everything drawn so far. 0 reads the terrain's height texture from stage 1, which
// covers only the ground.
//
// FLAME shades the sprite as fire. A noise field on stage 2, fixed in the world and rising with
// time, warps the texture lookup, breaks up dim edges and makes the brightness flicker. Bright
// texels run to white at their own brightness and dim ones deepen toward their hue, so blue and
// green flames keep their colour.

#ifndef SOFT
#define SOFT 1
#endif
#ifndef FLAME
#define FLAME 0
#endif

sampler2D ParticleTexture : register(s0);
sampler2D SurfaceTexture  : register(s1);
sampler2D NoiseTexture    : register(s2);

#if SOFT && DEPTH
float4 ClipX       : register(c0);   // camera space to clip x, y and w, one row each
float4 ClipY       : register(c1);
float4 ClipW       : register(c2);
float4 ScreenMap   : register(c3);   // xy scale and zw offset from clip space to depth texture
float4 Linearize   : register(c4);   // projection terms that turn stored depth back into camera z
#elif SOFT
float4 WorldX      : register(c0);   // camera space to world x, y and z, one row each
float4 WorldY      : register(c1);
float4 WorldZ      : register(c2);
float4 HeightMap   : register(c3);   // xy scale and zw offset from world xy to height texture
float4 HeightDecode : register(c4);  // weights of the high and low height bytes
#endif
float4 Params      : register(c5);   // x = 1 / fade distance, y = 1 to fade colour too, z = 1 to fade towards white, w = sign of depth along the view
#if FLAME
float4 Flame       : register(c6);   // x = noise rise so far, y = texture warp, z = world to noise scale, w = heat gain
float4 FlameWorldX : register(c7);   // camera space to world x, y and z, one row each
float4 FlameWorldY : register(c8);
float4 FlameWorldZ : register(c9);
#endif

struct PsIn
{
    float4 Diffuse   : COLOR0;
    float2 TexCoord  : TEXCOORD0;
    float3 Position  : TEXCOORD1;
};

float4 main(PsIn input) : COLOR
{
    float4 position = float4(input.Position, 1.0f);

#if FLAME
    float3 flameWorld = float3(dot(position, FlameWorldX), dot(position, FlameWorldY), dot(position, FlameWorldZ));

    // Two upright slices through the field, so the pattern changes whichever way the camera faces.
    float4 noiseA = tex2D(NoiseTexture, float2(flameWorld.x, flameWorld.z) * Flame.z - float2(0.0f, Flame.x));
    float4 noiseB = tex2D(NoiseTexture, float2(flameWorld.y, flameWorld.z) * (Flame.z * 1.7f) - float2(0.0f, Flame.x * 1.3f));

    float2 uv = input.TexCoord + (noiseA.rg + noiseB.gr - 1.0f) * Flame.y;
    float4 texel = tex2D(ParticleTexture, uv);
    float4 color = texel * input.Diffuse;

    // Adding ignores alpha, and additive systems often leave it at zero.
    float coverage = lerp(texel.a, 1.0f, Params.y);

    // Only the texture's faintest fringe breaks up, so particles dimming with age keep their size.
    float shape = max(texel.r, max(texel.g, texel.b)) * coverage;
    float keep = saturate(shape * 4.0f + 1.0f - noiseA.b - noiseB.b);

    float peak = max(color.r, max(color.g, color.b));
    float heat = saturate((peak * lerp(input.Diffuse.a, 1.0f, Params.y) * coverage - 0.5f) * Flame.w);

    // Halfway to the squared colour, so dim parts redden without losing much light.
    float3 deep = color.rgb * (color.rgb + peak) / max(2.0f * peak, 0.001f);
    color.rgb = lerp(deep, peak.xxx, heat * heat) * (keep * (0.85f + 0.3f * noiseB.b));
    color.a *= keep;
#else
    float4 color = tex2D(ParticleTexture, input.TexCoord) * input.Diffuse;
#endif

#if SOFT
#if DEPTH
    float w = dot(position, ClipW);
    float2 ndc = float2(dot(position, ClipX), dot(position, ClipY)) / w;
    float stored = tex2D(SurfaceTexture, ndc * ScreenMap.xy + ScreenMap.zw).r;

    // Depth along the view for both, with the sign that makes further away larger.
    float sceneZ = (Linearize.x - stored * Linearize.y) / (stored * Linearize.z - Linearize.w);
    float gap = (sceneZ - input.Position.z) * Params.w;
#else
    float3 world = float3(dot(position, WorldX), dot(position, WorldY), dot(position, WorldZ));
    float2 heightBytes = tex2D(SurfaceTexture, world.xy * HeightMap.xy + HeightMap.zw).rg;
    float gap = world.z - dot(heightBytes, HeightDecode.xy);
#endif

    float fade = saturate(gap * Params.x);
    color.a *= fade;
    color.rgb *= lerp(1.0f, fade, Params.y);
    color.rgb = lerp(color.rgb, lerp(float3(1.0f, 1.0f, 1.0f), color.rgb, fade), Params.z);
#endif
    return color;
}
