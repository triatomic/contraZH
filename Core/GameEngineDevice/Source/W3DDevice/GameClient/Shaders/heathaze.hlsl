// Wobbles the scene behind a flame sprite, like air shimmering over a fire.
//
// The sprite draws before the particles, over a copy of the scene on stage 3, and writes back that
// copy bent by a noise field on stage 2, alpha blended whatever the flame's own blend. The field is fixed in the world and rises with time. The
// sprite's own brightness masks the bend, so the haze follows the shape of the flame. Its vertices
// are in camera space, and stage 1 hands that position over as TEXCOORD1.

sampler2D ParticleTexture : register(s0);
sampler2D NoiseTexture    : register(s2);
sampler2D SceneTexture    : register(s3);

float4 ClipX     : register(c0);   // camera space to clip x, y and w, one row each
float4 ClipY     : register(c1);
float4 ClipW     : register(c2);
float4 ScreenMap : register(c3);   // xy scale and zw offset from clip space to scene uv
float4 Haze      : register(c4);   // x = noise rise so far, y = bend in scene uv at unit depth, z = world to noise scale, w = mask gain
float4 Params    : register(c5);   // y = 1 when the flame adds its colour, which ignores alpha
float4 WorldX    : register(c7);   // camera space to world x, y and z, one row each
float4 WorldY    : register(c8);
float4 WorldZ    : register(c9);

struct PsIn
{
    float4 Diffuse   : COLOR0;
    float2 TexCoord  : TEXCOORD0;
    float3 Position  : TEXCOORD1;
};

float4 main(PsIn input) : COLOR
{
    float4 position = float4(input.Position, 1.0f);
    float w = dot(position, ClipW);
    float2 ndc = float2(dot(position, ClipX), dot(position, ClipY)) / w;
    float3 world = float3(dot(position, WorldX), dot(position, WorldY), dot(position, WorldZ));

    float2 bend = tex2D(NoiseTexture, float2(world.x, world.z) * Haze.z - float2(0.0f, Haze.x)).rg
                + tex2D(NoiseTexture, float2(world.y, world.z) * (Haze.z * 1.9f) - float2(0.0f, Haze.x * 1.4f)).gr - 1.0f;

    float4 sprite = tex2D(ParticleTexture, input.TexCoord) * input.Diffuse;
    float mask = saturate(max(sprite.r, max(sprite.g, sprite.b)) * lerp(sprite.a, 1.0f, Params.y) * Haze.w);

    float2 uv = ndc * ScreenMap.xy + ScreenMap.zw + bend * (Haze.y * mask / w);
    return float4(tex2D(SceneTexture, uv).rgb, mask);
}
