// Draws a vehicle's headlight in place of its HEADLIGHT mesh, as two draws.
//
// Without POOL the beam draws: a quad along the light that faces the camera. The pixel shader shapes
// it into a cone that dims along its length and towards its edge, and fades it out where it meets
// the scene's INTZ depth.
//
// POOL draws the light the lamp throws. A quad covers the light's place on screen, and each pixel
// finds its world position from the scene depth and lights it by its place inside the lamp's cone.
// The draw blends as dest * (1 + light), which lights the scene's own colour a second time.
//
// POOL=2 draws every pool in one quad over all of them and takes the brightest at each pixel, so pools
// that overlap light the ground no more than the brightest one does. The pools come in a float texture,
// since ps_3_0 cannot index constants by a loop counter.

#ifndef POOL
#define POOL 0
#endif

sampler2D DepthTexture : register(s0);   // the scene's INTZ depth

float Smooth(float x)
{
    return x * x * (3.0f - 2.0f * x);
}

// Turns stored depth back into clip-space w. linearize holds the projection's terms, facing the sign of depth along the view.
float SceneDepth(float2 uv, float4 linearize, float facing)
{
    float stored = tex2Dlod(DepthTexture, float4(uv, 0.0f, 0.0f)).r;
    return (linearize.x - stored * linearize.y) / (stored * linearize.z - linearize.w) * facing;
}

#if POOL

float4 Eye       : register(c0);
float4 Linearize : register(c1);
float4 Params    : register(c2);   // x = sign of depth along the view, y = pools with POOL=2, z = 1 / the pool texture's width

#if POOL == 2
sampler2D PoolTexture : register(s1);   // three texels a pool: the origin, aim and colour below
#else
float4 Origin    : register(c3);   // xyz = the lamp, w = 1 / range
float4 Aim       : register(c4);   // xyz = unit direction of the light, w = tangent of its half angle
float4 Color     : register(c5);   // rgb = light over the scene's light, a = falloff power
#endif

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
};

// How much of one lamp's light reaches a point, 0 to 1.
float PoolLight(float3 world, float4 origin, float4 aim, float falloff)
{
    float3 fromLamp = world - origin.xyz;
    float along = dot(fromLamp, aim.xyz);
    float aside = length(fromLamp - aim.xyz * along);

    // 0 on the light's middle line, 1 on the cone's edge.
    float off = aside / max(along * aim.w, 0.001f);
    float cone = Smooth(saturate((1.0f - off) * 2.5f));
    float reach = pow(saturate(1.0f - along * origin.w), falloff);
    return cone * reach * step(0.0f, along);
}

float4 mainPS(PsIn input) : COLOR
{
    float3 world = Eye.xyz + input.Ray * SceneDepth(input.DepthUV, Linearize, Params.x);

#if POOL == 2
    float3 light = 0.0f;
    [loop] for (float i = 0.0f; i < Params.y; i += 1.0f)
    {
        float u = (i * 3.0f + 0.5f) * Params.z;
        float4 origin = tex2Dlod(PoolTexture, float4(u, 0.5f, 0.0f, 0.0f));
        float4 aim = tex2Dlod(PoolTexture, float4(u + Params.z, 0.5f, 0.0f, 0.0f));
        float4 color = tex2Dlod(PoolTexture, float4(u + Params.z * 2.0f, 0.5f, 0.0f, 0.0f));
        light = max(light, color.rgb * PoolLight(world, origin, aim, color.a));
    }
    return float4(light, 0.0f);
#else
    return float4(Color.rgb * PoolLight(world, Origin, Aim, Color.a), 0.0f);
#endif
}

#else

float4 ClipX     : register(c0);   // world to clip space, one output component each
float4 ClipY     : register(c1);
float4 ClipZ     : register(c2);
float4 ClipW     : register(c3);

float4 Linearize : register(c5);
float4 Params    : register(c6);   // x = sign of depth along the view, y = 1 with scene depth bound
float4 DepthMap  : register(c7);   // clip space to depth texcoords: xy scale, zw offset

// The beam's half width at the lamp, as a share of its half width at the far end.
static const float LampWidth = 0.15f;

struct VsIn
{
    float3 Position : POSITION;    // world space
    float3 Color    : NORMAL;      // beam colour times intensity
    float4 Level    : COLOR0;      // dims a beam seen end on
    float2 Along    : TEXCOORD0;   // x = 0 at the lamp to 1 at the far end, y = -1 to 1 across
    float2 Shape    : TEXCOORD1;   // x = falloff power, y = 1 / softness
};

struct VsOut
{
    float4 Position : POSITION;
    float4 Level    : COLOR0;
    float4 Along    : TEXCOORD0;   // xy as the vertex has them, zw = its shape
    float3 Clip     : TEXCOORD1;   // clip-space x, y and w
    float3 Color    : TEXCOORD2;
};

VsOut mainVS(VsIn input)
{
    VsOut output;
    float4 position = float4(input.Position, 1.0f);
    output.Position = float4(dot(position, ClipX), dot(position, ClipY), dot(position, ClipZ), dot(position, ClipW));
    output.Level = input.Level;
    output.Along = float4(input.Along, input.Shape);
    output.Clip = output.Position.xyw;
    output.Color = input.Color;
    return output;
}

struct PsIn
{
    float4 Level : COLOR0;
    float4 Along : TEXCOORD0;
    float3 Clip  : TEXCOORD1;
    float3 Color : TEXCOORD2;
};

float4 mainPS(PsIn input) : COLOR
{
    // The quad is as wide as the far end all along, and the cone is cut out of it here.
    float across = abs(input.Along.y) / lerp(LampWidth, 1.0f, input.Along.x);
    float edge = saturate(1.0f - across * across);
    float reach = pow(saturate(1.0f - input.Along.x), input.Along.z);

    float2 uv = input.Clip.xy / input.Clip.z * DepthMap.xy + DepthMap.zw;
    float gap = SceneDepth(uv, Linearize, Params.x) - input.Clip.z;
    float soft = lerp(1.0f, saturate(gap * input.Along.w), Params.y);

    return float4(input.Color * (edge * edge * reach * soft * input.Level.r), 0.0f);
}

#endif
