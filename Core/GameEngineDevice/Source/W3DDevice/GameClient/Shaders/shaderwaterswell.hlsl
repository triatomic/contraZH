// Lifts standing water's vertices by a tiling height texture, ahead of the ps_3_0 water shader.
//
// The water draws in world space, so the fixed-function vertex format comes straight in. Two
// layers of the height texture, at different scales and drifting apart, keep the swell from
// visibly repeating. Its slope and height go out beside the world position, to tilt the pixel
// normal and raise foam on the crests.
//
// RADIAL draws a polar grid centred under the camera instead, whose cells widen with distance.
// Each vertex carries its offset from the centre and its cell size, which sets the slope step
// and the mip level, and the water texture coordinates and colour are made here.

sampler2D SwellMap : register(s0);   // vertex texture sampler 0

float4 ClipX        : register(c0);   // world to clip space, one output component each
float4 ClipY        : register(c1);
float4 ClipZ        : register(c2);
float4 ClipW        : register(c3);
float4 Swell        : register(c4);   // x = world to texcoord scale, y = height, zw = drift
float4 SwellSample  : register(c5);   // x = world step for the slope, y = mip level; RADIAL: x = texels per world unit
float4 SwellChannel : register(c6);   // picks the channel holding height

#if RADIAL
float4 Radial       : register(c7);   // xy = grid centre, z = water level
float4 Wobble       : register(c8);   // x = texcoords per world unit, yz = wobble size, w = wobble phase
float4 WobbleRate   : register(c9);   // x = wobble cycles per world unit
float4 WaterColor   : register(c10);

struct VsIn
{
    float3 Position : POSITION;   // xy = offset from the centre, z = cell size
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
    float3 Swell      : TEXCOORD3;   // xy = slope, z = height from -1 to 1
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
    float2 at = Radial.xy + input.Position.xy;
    float step = input.Position.z;
    float mip = log2(max(step * SwellSample.x, 1.0f));
    float level = Radial.z;
#else
    float2 at = input.Position.xy;
    float step = SwellSample.x;
    float mip = SwellSample.y;
    float level = input.Position.z;
#endif
    float h = Height(at, mip);
    float hx = Height(at + float2(step, 0.0f), mip);
    float hy = Height(at + float2(0.0f, step), mip);
    float4 world = float4(at, level + h, 1.0f);

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
    output.Swell = float3(float2(h - hx, h - hy) / step, h / max(Swell.y, 0.001f));
    return output;
}
