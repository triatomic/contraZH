// Ripples the scene behind a shape and pulls its colours apart, like a jammed signal.
//
// The shape draws over a copy of the scene on stage 3 and writes that copy back bent, alpha blended
// by the shape's own brightness. Rings travel through the shape, a noise field on stage 2 wobbles
// it, and bands across the screen jump sideways. Red bends further than green and blue less. The
// shape's vertices are in camera space, and stage 1 hands that position over as TEXCOORD1.
//
// SHAPE says where the rings run: 0 out from the middle of a sprite's texture, 1 out from the
// draw's world origin, 2 along a beam, whose coordinates are the second uv set on stage 2.

#ifndef SHAPE
#define SHAPE 0
#endif

sampler2D ShapeTexture : register(s0);
sampler2D NoiseTexture : register(s2);
sampler2D SceneTexture : register(s3);

float4 ClipX     : register(c0);   // camera space to clip x, y and w, one row each
float4 ClipY     : register(c1);
float4 ClipW     : register(c2);
float4 ScreenMap : register(c3);   // xy scale and zw offset from clip space to scene uv
float4 Rings     : register(c4);   // x = radians per world unit, y = phase so far, z = push and w = colour spread in world units
float4 Params    : register(c5);   // x = how much of its mask the draw keeps, y = 1 when the shape adds its colour, which ignores alpha
float4 Wobble    : register(c6);   // x = world to noise scale, y = noise crossed so far, z = push in world units, w = mask gain
float4 WorldX    : register(c7);   // camera space to world x, y and z, one row each
float4 WorldY    : register(c8);
float4 WorldZ    : register(c9);
float4 Glitch    : register(c10);  // x = bands per unit of clip y, yz = this jump's spot in the noise, w = push in world units
float4 Bend      : register(c11);  // xy = scene uv per world unit at unit depth, z = red's share of the bend, w = blue's
float4 Center    : register(c12);  // xyz = where the rings start, in camera space

struct PsIn
{
    float4 Diffuse  : COLOR0;
    float2 TexCoord : TEXCOORD0;
    float3 Position : TEXCOORD1;
#if SHAPE == 2
    float2 Beam     : TEXCOORD2;
#endif
};

float4 main(PsIn input) : COLOR
{
    float4 position = float4(input.Position, 1.0f);
    float w = dot(position, ClipW);
    float2 ndc = float2(dot(position, ClipX), dot(position, ClipY)) / w;
    float3 world = float3(dot(position, WorldX), dot(position, WorldY), dot(position, WorldZ));

    // reach is the distance the rings have to travel to this pixel, and slope how it grows across the screen.
#if SHAPE == 1
    float reach = length(input.Position - Center.xyz);
    float2 slope = float2(ddx(reach), ddy(reach));
#elif SHAPE == 2
    float reach = input.Beam.y;
    float2 slope = float2(ddx(input.Beam.x), ddy(input.Beam.x));
#else
    float spread = length(input.TexCoord - 0.5f);
    float2 slope = float2(ddx(spread), ddy(spread));
    // A texture width in world units, from how fast each crosses the screen.
    float reach = spread * length(ddx(input.Position)) / max(length(ddx(input.TexCoord)), 1e-6f);
#endif
    float2 outward = slope * rsqrt(max(dot(slope, slope), 1e-12f));

    float2 push = outward * (sin(reach * Rings.x - Rings.y) * Rings.z);

    // Two layers of the field cross each other, so the wobble boils instead of sliding.
    float2 field = world.xy + world.z * float2(0.6f, 0.8f);
    float2 drift = tex2D(NoiseTexture, field * Wobble.x + float2(0.0f, Wobble.y)).rg
                 + tex2D(NoiseTexture, field * (Wobble.x * 1.9f) - float2(Wobble.y * 1.3f, 0.0f)).gr - 1.0f;
    push += drift * Wobble.z;

    // Each band reads its own texel, eleven apart so neighbours are unrelated. Cubing leaves most bands still.
    float band = ndc.y * Glitch.x;
    band -= frac(band);
    float jolt = tex2D(NoiseTexture, float2(band * 0.171875f + Glitch.y, Glitch.z)).b * 2.0f - 1.0f;
    push.x += clamp(jolt * jolt * jolt * 4.0f, -1.0f, 1.0f) * Glitch.w;

    float4 shape = tex2D(ShapeTexture, input.TexCoord) * input.Diffuse;
    float mask = saturate(max(shape.r, max(shape.g, shape.b)) * lerp(shape.a, 1.0f, Params.y) * Wobble.w) * Params.x;

    float2 unit = Bend.xy * (mask / w);
    float2 bend = push * unit;
    float2 split = outward * Rings.w * unit;
    float2 uv = ndc * ScreenMap.xy + ScreenMap.zw;

    float3 scene;
    scene.r = tex2D(SceneTexture, uv + bend * Bend.z + split).r;
    scene.g = tex2D(SceneTexture, uv + bend).g;
    scene.b = tex2D(SceneTexture, uv + bend * Bend.w - split).b;
    return float4(scene, mask);
}
