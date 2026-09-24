// Draws a particle sprite fading out where it nears the surface behind it, so smoke and fire
// meet the ground and buildings without a hard line.
//
// The sprite keeps its fixed-function colour, texture times vertex colour. Its vertices are
// already in camera space, and stage 1 hands that position over as TEXCOORD1.
//
// DEPTH picks the surface. 1 reads the scene's INTZ depth from stage 1, which covers everything
// drawn so far. 0 reads the terrain's height texture from stage 1, which covers only the ground.

sampler2D ParticleTexture : register(s0);
sampler2D SurfaceTexture  : register(s1);

#if DEPTH
float4 ClipX       : register(c0);   // camera space to clip x, y and w, one row each
float4 ClipY       : register(c1);
float4 ClipW       : register(c2);
float4 ScreenMap   : register(c3);   // xy scale and zw offset from clip space to depth texture
float4 Linearize   : register(c4);   // projection terms that turn stored depth back into camera z
#else
float4 WorldX      : register(c0);   // camera space to world x, y and z, one row each
float4 WorldY      : register(c1);
float4 WorldZ      : register(c2);
float4 HeightMap   : register(c3);   // xy scale and zw offset from world xy to height texture
float4 HeightDecode : register(c4);  // weights of the high and low height bytes
#endif
float4 Params      : register(c5);   // x = 1 / fade distance, y = 1 to fade colour too, z = 1 to fade towards white, w = sign of depth along the view

struct PsIn
{
    float4 Diffuse   : COLOR0;
    float2 TexCoord  : TEXCOORD0;
    float3 Position  : TEXCOORD1;
};

float4 main(PsIn input) : COLOR
{
    float4 position = float4(input.Position, 1.0f);

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

    float4 color = tex2D(ParticleTexture, input.TexCoord) * input.Diffuse;
    color.a *= fade;
    color.rgb *= lerp(1.0f, fade, Params.y);
    color.rgb = lerp(color.rgb, lerp(float3(1.0f, 1.0f, 1.0f), color.rgb, fade), Params.z);
    return color;
}
