// Lifts standing water's vertices by a tiling height texture, ahead of the ps_3_0 water shader.
//
// The water draws in world space, so the fixed-function vertex format comes straight in. Two
// layers of the height texture, at different scales and drifting apart, keep the swell from
// visibly repeating. The pixel shader reads the same layers for the slope and the crests.
//
// RADIAL draws square levels centred under the camera instead, each with twice the cells of the
// one inside it and snapped to its own world lattice, so vertices never slide across the swell.
// Near a level's edge the odd vertices fold onto the coarser lattice, and the water texture
// coordinates and colour are made here.

sampler2D SwellMap : register(s0);   // vertex texture sampler 0

float4 ClipX        : register(c0);   // world to clip space, one output component each
float4 ClipY        : register(c1);
float4 ClipZ        : register(c2);
float4 ClipW        : register(c3);
float4 Swell        : register(c4);   // x = world to texcoord scale, y = height, zw = drift
float4 SwellSample  : register(c5);   // y = mip level
float4 SwellChannel : register(c6);   // picks the channel holding height

#if RADIAL
float4 Level        : register(c7);   // xy = this level's lattice origin, z = water level, w = cell size
float4 Wobble       : register(c8);   // x = texcoords per world unit, yz = wobble size, w = wobble phase
float4 WobbleRate   : register(c9);   // x = wobble cycles per world unit
float4 WaterColor   : register(c10);
float4 Fold         : register(c11);  // x = distance from the eye where folding starts, y = 1 / its width, z = log2 of texels per cell
float4 Eye          : register(c12);  // xy = camera position

struct VsIn
{
    float3 Position : POSITION;   // xy = cell index within the level
};
#else
struct VsIn
{
    float3 Position : POSITION;
    float4 Diffuse  : COLOR0;
    float2 BaseUV   : TEXCOORD0;
    float2 EdgeUV   : TEXCOORD1;
};
#endif

struct VsOut
{
    float4 Position   : POSITION;
    float4 Diffuse    : COLOR0;
    float2 BaseUV     : TEXCOORD0;
    float2 EdgeUV     : TEXCOORD1;
    float3 WorldPos   : TEXCOORD2;
};

float Layer(float2 uv, float mip)
{
    return dot(tex2Dlod(SwellMap, float4(uv, 0.0f, mip)), SwellChannel) * 2.0f - 1.0f;
}

float Height(float2 world, float mip)
{
    float2 uv = world * Swell.x;
    return (0.65f * Layer(uv + Swell.zw, mip) + 0.35f * Layer(uv * 1.7f - Swell.wz * 1.3f, mip + 0.77f)) * Swell.y;
}

VsOut main(VsIn input)
{
#if RADIAL
    float2 grid = input.Position.xy;
    float2 at = Level.xy + grid * Level.w;
    float2 away = abs(at - Eye.xy);
    float fold = saturate((max(away.x, away.y) - Fold.x) * Fold.y);
    at -= frac(grid * 0.5f) * 2.0f * Level.w * fold;
    float mip = max(Fold.z + fold, 0.0f);
    float level = Level.z;
#else
    float2 at = input.Position.xy;
    float mip = SwellSample.y;
    float level = input.Position.z;
#endif
    float4 world = float4(at, level + Height(at, mip), 1.0f);

    VsOut output;
    output.Position = float4(dot(world, ClipX), dot(world, ClipY), dot(world, ClipZ), dot(world, ClipW));
#if RADIAL
    // The same texture coordinates drawTrapezoidWater gives each vertex.
    output.Diffuse = WaterColor;
    output.BaseUV = at * Wobble.x + Wobble.yz * sin(Wobble.w + at * WobbleRate.x);
    output.EdgeUV = float2(0.0f, 0.0f);
#else
    output.Diffuse = input.Diffuse;
    output.BaseUV = input.BaseUV;
    output.EdgeUV = input.EdgeUV;
#endif
    output.WorldPos = world.xyz;
    return output;
}
