// Lifts standing water's vertices by a tiling height texture, ahead of the ps_3_0 water shader.
//
// The water draws in world space, so the fixed-function vertex format comes straight in. Two
// layers of the height texture, at different scales and drifting apart, keep the swell from
// visibly repeating. Its slope and height go out beside the world position, to tilt the pixel
// normal and raise foam on the crests.

sampler2D SwellMap : register(s0);   // vertex texture sampler 0

float4 ClipX        : register(c0);   // world to clip space, one output component each
float4 ClipY        : register(c1);
float4 ClipZ        : register(c2);
float4 ClipW        : register(c3);
float4 Swell        : register(c4);   // x = world to texcoord scale, y = height, zw = drift
float4 SwellSample  : register(c5);   // x = world step for the slope, y = mip level
float4 SwellChannel : register(c6);   // picks the channel holding height

struct VsIn
{
    float3 Position : POSITION;
    float4 Diffuse  : COLOR0;
    float2 BaseUV   : TEXCOORD0;
    float2 EdgeUV   : TEXCOORD1;
};

struct VsOut
{
    float4 Position   : POSITION;
    float4 Diffuse    : COLOR0;
    float2 BaseUV     : TEXCOORD0;
    float2 EdgeUV     : TEXCOORD1;
    float3 WorldPos   : TEXCOORD2;
    float3 Swell      : TEXCOORD3;   // xy = slope, z = height from -1 to 1
};

float Layer(float2 uv)
{
    return dot(tex2Dlod(SwellMap, float4(uv, 0.0f, SwellSample.y)), SwellChannel) * 2.0f - 1.0f;
}

float Height(float2 world)
{
    float2 uv = world * Swell.x;
    return (0.65f * Layer(uv + Swell.zw) + 0.35f * Layer(uv * 1.7f - Swell.wz * 1.3f)) * Swell.y;
}

VsOut main(VsIn input)
{
    float step = SwellSample.x;
    float h = Height(input.Position.xy);
    float hx = Height(input.Position.xy + float2(step, 0.0f));
    float hy = Height(input.Position.xy + float2(0.0f, step));
    float4 world = float4(input.Position.xy, input.Position.z + h, 1.0f);

    VsOut output;
    output.Position = float4(dot(world, ClipX), dot(world, ClipY), dot(world, ClipZ), dot(world, ClipW));
    output.Diffuse = input.Diffuse;
    output.BaseUV = input.BaseUV;
    output.EdgeUV = input.EdgeUV;
    output.WorldPos = world.xyz;
    output.Swell = float3(float2(h - hx, h - hy) / step, h / max(Swell.y, 0.001f));
    return output;
}
