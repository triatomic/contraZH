// Draws a storm of sand or snow that stands on the ground inside an upright cylinder.
//
// The storm is two draws. Without GRAIN the haze draws as a quad over the storm's place on screen.
// Each pixel walks its view ray through the cylinder, from the camera to the scene's INTZ depth,
// and adds up the haze it crosses. The haze thins towards the cylinder's edge and towards its top,
// which follows the terrain's height texture, and noise drifting with the wind bunches it into gusts.
//
// GRAIN draws the grains or flakes. Every grain is a quad whose place the vertex shader works out
// from a seed and the time alone. Grains fill a square tile that wraps around the middle of the
// view, so a storm of any size takes the same grain count. They ride the wind at three paces, sink,
// swirl, stand on the terrain's height and smear along their own travel.
//
// Both draws need shader model 3, the haze for its loop of lookups and the grains to read textures
// per vertex.

#ifndef GRAIN
#define GRAIN 0
#endif

float Smooth(float x)
{
    return x * x * (3.0f - 2.0f * x);
}

#if GRAIN

sampler2D GroundMap : register(s0);   // terrain heights as high and low bytes, vertex texture sampler 0
sampler2D GustMap   : register(s1);   // the haze's noise, vertex texture sampler 1

float4 ClipX         : register(c0);   // world to clip space, one output component each
float4 ClipY         : register(c1);
float4 ClipZ         : register(c2);
float4 ClipW         : register(c3);
float4 Center        : register(c4);   // xyz = the storm's middle on the ground, w = radius
float4 Box           : register(c5);   // x = tile size, y = storm height, z = 1 / edge fade share, w = opacity times the storm's level
float4 Focus         : register(c6);   // xy = middle of the tile, z = swirl time, w = how strongly grains keep to the ground
float4 Travel        : register(c7);   // xy = wind travel so far at a third of its speed, z = the same for the fall in storm heights
float4 Motion        : register(c8);   // xy = wind velocity, z = fall speed, w = seconds of travel a grain smears along
float4 Grain         : register(c9);   // x = half size, y = swirl size, z = smallest half size at unit distance, w = gusts
float4 Eye           : register(c10);
float4 GroundMapping : register(c11);  // world xy to ground texcoords: xy scale, zw offset
float4 GroundDecode  : register(c12);  // xy = high and low byte weights, z = 1 when the ground is bound, w = ground height otherwise
float4 Color         : register(c13);
float4 SeedShift     : register(c14);  // moves every seed, so a second draw adds new grains
float4 GustNoise     : register(c15);  // x = world to noise scale, yz = noise scroll so far

struct VsIn
{
    float3 Seed   : POSITION;    // 0 to 1 each
    float2 Corner : TEXCOORD0;   // -1 to 1 across the quad
    float2 Extra  : TEXCOORD1;   // 0 to 1 each
};

struct VsOut
{
    float4 Position : POSITION;
    float4 Color    : COLOR0;
    float2 Corner   : TEXCOORD0;
};

VsOut mainVS(VsIn input)
{
    float3 seed = frac(input.Seed + SeedShift.xyz);
    float tile = Box.x;

    // Whole multiples of the travel wrap with it, so the three paces never jump.
    float windPace = 2.0f + floor(input.Extra.x * 2.999f);
    float fallPace = 2.0f + floor(input.Extra.y * 2.999f);

    float2 drift = seed.xy * tile + Travel.xy * windPace;
    float2 at = Focus.xy + (frac((drift - Focus.xy) / tile + 0.5f) - 0.5f) * tile;
    float sink = frac(seed.z - Travel.z * fallPace);

    float3 phase = Focus.z * float3(1.3f, 1.7f, 2.1f) + seed * 40.0f;
    float3 swirl = Grain.y * sin(phase) * float3(1.0f, 1.0f, 0.5f);
    float3 velocity = float3(Motion.xy * (windPace / 3.0f), -Motion.z * (fallPace / 3.0f)) +
        Grain.y * cos(phase) * float3(1.3f, 1.7f, 1.05f);

    // The tile's edge fades before the swirl moves the grain, so the fade stays in step with the wrap.
    float2 fromFocus = abs(at - Focus.xy) / (0.5f * tile);
    float tileFade = saturate((1.0f - max(fromFocus.x, fromFocus.y)) * 8.0f);
    at += swirl.xy;

    float2 groundBytes = tex2Dlod(GroundMap, float4(at * GroundMapping.xy + GroundMapping.zw, 0.0f, 0.0f)).rg;
    float ground = lerp(GroundDecode.w, dot(groundBytes, GroundDecode.xy), GroundDecode.z);
    float3 world = float3(at, ground + pow(sink, Focus.w) * Box.y + swirl.z);

    float fromCenter = length(at - Center.xy) / Center.w;
    float edgeFade = Smooth(saturate((1.0f - fromCenter) * Box.z));
    float wrapFade = saturate(sink * 8.0f) * saturate((1.0f - sink) * 4.0f);
    float gust = tex2Dlod(GustMap, float4(at * GustNoise.x - GustNoise.yz, 0.0f, 0.0f)).r;
    float gustFade = lerp(1.0f, saturate(gust * 2.4f - 0.5f), Grain.w);

    float3 toEye = Eye.xyz - world;
    float distance = length(toEye);
    toEye /= max(distance, 0.001f);
    float nearFade = saturate((distance - 20.0f) / 40.0f);

    // A grain smaller than a pixel would flicker, so it keeps a pixel's size and dims to match.
    float halfSize = max(Grain.x, distance * Grain.z);
    float alpha = Box.w * edgeFade * tileFade * wrapFade * gustFade * nearFade * (Grain.x / halfSize) * (0.6f + 0.4f * input.Extra.x);

    float3 smear = velocity * Motion.w;
    float smearLength = length(smear);
    float3 along = (smearLength > 0.0001f) ? smear / smearLength : float3(0.0f, 0.0f, 1.0f);
    float3 across = cross(along, toEye);
    across /= max(length(across), 0.0001f);

    world += along * (input.Corner.y * (0.5f * smearLength + halfSize)) + across * (input.Corner.x * halfSize);

    VsOut output;
    float4 position = float4(world, 1.0f);
    output.Position = float4(dot(position, ClipX), dot(position, ClipY), dot(position, ClipZ), dot(position, ClipW));
    // A faded grain folds into one point and draws nothing.
    if (alpha < 0.004f)
    {
        output.Position = float4(0.0f, 0.0f, 0.0f, 1.0f);
    }
    output.Color = float4(Color.rgb, alpha);
    output.Corner = input.Corner;
    return output;
}

struct PsIn
{
    float4 Color  : COLOR0;
    float2 Corner : TEXCOORD0;
};

float4 mainPS(PsIn input) : COLOR
{
    float inside = saturate(1.0f - dot(input.Corner, input.Corner));
    return float4(input.Color.rgb, input.Color.a * inside * inside);
}

#else

#define STEPS 8

sampler2D DepthTexture  : register(s0);   // the scene's INTZ depth
sampler2D HeightTexture : register(s1);   // terrain heights as high and low bytes
sampler2D NoiseTexture  : register(s2);

float4 Eye          : register(c0);
float4 Center       : register(c1);   // xyz = the storm's middle on the ground, w = radius
float4 Shape        : register(c2);   // x = 1 / storm height, y = 1 / edge fade share, z = highest and w = lowest the haze reaches
float4 Linearize    : register(c3);   // projection terms that turn stored depth back into camera z
float4 Params       : register(c4);   // x = sign of depth along the view, y = 1 with scene depth bound, z = reach without it, w = gusts
float4 Noise        : register(c5);   // x = world to noise scale, yz = noise scroll so far, w = optical depth per world unit at the storm's level
float4 HeightMap    : register(c6);   // xy scale and zw offset from world xy to height texture
float4 HeightDecode : register(c7);   // xy = high and low byte weights, z = 1 when the ground is bound, w = ground height otherwise
float4 Color        : register(c8);   // rgb = haze colour, a = the most the haze may hide

struct VsIn
{
    float3 Position : POSITION;    // already in clip space
    float3 Ray      : NORMAL;      // world travel per unit of view depth
    float2 DepthUV  : TEXCOORD0;
};

struct VsOut
{
    float4 Position : POSITION;
    float3 Ray      : TEXCOORD0;
    float2 DepthUV  : TEXCOORD1;
};

VsOut mainVS(VsIn input)
{
    VsOut output;
    output.Position = float4(input.Position.xy, 0.5f, 1.0f);
    output.Ray = input.Ray;
    output.DepthUV = input.DepthUV;
    return output;
}

struct PsIn
{
    float3 Ray     : TEXCOORD0;
    float2 DepthUV : TEXCOORD1;
    float2 Pixel   : VPOS;
};

float4 mainPS(PsIn input) : COLOR
{
    float stored = tex2Dlod(DepthTexture, float4(input.DepthUV, 0.0f, 0.0f)).r;
    float sceneDepth = (Linearize.x - stored * Linearize.y) / (stored * Linearize.z - Linearize.w) * Params.x;
    float reach = lerp(Params.z, sceneDepth, Params.y);

    // Where the ray crosses the cylinder's wall.
    float3 ray = input.Ray;
    float2 fromCenter = Eye.xy - Center.xy;
    float a = max(dot(ray.xy, ray.xy), 1.0e-12f);
    float b = dot(fromCenter, ray.xy);
    float c = dot(fromCenter, fromCenter) - Center.w * Center.w;
    float discriminant = b * b - a * c;
    clip(discriminant);
    float root = sqrt(max(discriminant, 0.0f));

    // Where it crosses the highest and lowest the haze reaches.
    float climb = (abs(ray.z) > 1.0e-6f) ? ray.z : 1.0e-6f;
    float top = (Shape.z - Eye.z) / climb;
    float bottom = (Shape.w - Eye.z) / climb;

    float enter = max(max((-b - root) / a, min(top, bottom)), 0.0f);
    float leave = min(min((-b + root) / a, max(top, bottom)), reach);
    float span = leave - enter;
    clip(span);

    // Each pixel starts its steps at its own offset, which trades banding for fine grain.
    float jitter = frac(52.9829189f * frac(dot(input.Pixel, float2(0.06711056f, 0.00583715f))));
    float stride = span / STEPS;

    float crossed = 0.0f;
    float bright = 0.0f;
    for (int i = 0; i < STEPS; i++)
    {
        float3 at = Eye.xyz + ray * (enter + (i + jitter) * stride);

        float2 heightBytes = tex2Dlod(HeightTexture, float4(at.xy * HeightMap.xy + HeightMap.zw, 0.0f, 0.0f)).rg;
        float ground = lerp(HeightDecode.w, dot(heightBytes, HeightDecode.xy), HeightDecode.z);
        float rise = (at.z - ground) * Shape.x;
        float heightFade = Smooth(saturate(1.0f - rise)) * saturate(1.0f + rise * 4.0f);
        float edgeFade = Smooth(saturate((1.0f - length(at.xy - Center.xy) / Center.w) * Shape.y));

        // Height shears the lookup, so the gusts lean and change up the storm. The second layer scrolls a whole multiple, which keeps its wrap seamless.
        float2 field = (at.xy + at.z * float2(0.5f, 0.3f)) * Noise.x;
        float coarse = tex2Dlod(NoiseTexture, float4(field - Noise.yz, 0.0f, 0.0f)).r;
        float fine = tex2Dlod(NoiseTexture, float4(field * 2.7f - Noise.yz * 3.0f + 0.37f, 0.0f, 0.0f)).g;
        float noise = 0.6f * coarse + 0.4f * fine;
        float billow = saturate((noise - 0.5f) * 2.5f + 0.5f) * 2.0f;

        float density = heightFade * edgeFade * lerp(1.0f, billow, Params.w);
        crossed += density;
        bright += density * noise;
    }

    float opticalDepth = crossed * stride * length(ray) * Noise.w;
    float alpha = min(1.0f - exp(-opticalDepth), Color.a);
    float shade = lerp(0.8f, 1.15f, saturate(bright / max(crossed, 0.0001f)));
    return float4(Color.rgb * shade, alpha);
}

#endif
