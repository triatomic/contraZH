// Shifts the scene behind a heat sprite, like air over a hot engine.
//
// The sprite draws after the particles, over a copy of the scene on stage 3. The shift and the
// sprite's alpha are full at its middle and gone at its edges. The red and green of its colour
// carry the shift, as a share of the sprite's size. Its vertices are in camera space, and stage 1
// hands that position over as TEXCOORD1.

sampler2D SceneTexture : register(s3);

float4 ClipX     : register(c0);   // camera space to clip x, y and w, one row each
float4 ClipY     : register(c1);
float4 ClipW     : register(c2);
float4 ScreenMap : register(c3);   // xy scale and zw offset from clip space to scene uv
float4 Bend      : register(c4);   // xy = scene uv per world unit at unit depth
float4 Bounds    : register(c5);   // the scene uv the copy holds, xy the least and zw the most

// The furthest shift, as a share of the sprite's size.
static const float Reach = 0.06f;

static const float3 Tint = float3(1.0f, 0.933f, 0.867f);

struct PsIn
{
    float4 Diffuse  : COLOR0;
    float2 TexCoord : TEXCOORD0;
    float3 Position : TEXCOORD1;
};

float4 main(PsIn input) : COLOR
{
    float4 position = float4(input.Position, 1.0f);
    float w = dot(position, ClipW);
    float2 ndc = float2(dot(position, ClipX), dot(position, ClipY)) / w;

    float2 fromMiddle = abs(input.TexCoord - 0.5f);
    float weight = saturate(1.0f - 2.0f * max(fromMiddle.x, fromMiddle.y));

    // A texture width in world units, from how fast each crosses the screen.
    float size = length(ddx(input.Position)) / max(length(ddx(input.TexCoord)), 1e-6f);

    float2 shift = (input.Diffuse.rg * 2.0f - 1.0f) * (Reach * size * weight);
    float2 uv = ndc * ScreenMap.xy + ScreenMap.zw + shift * Bend.xy / w;
    uv = clamp(uv, Bounds.xy, Bounds.zw);

    return float4(tex2D(SceneTexture, uv).rgb * Tint, weight * input.Diffuse.a);
}
