// Adds point lights to the finished opaque scene from its INTZ depth, one instanced quad a light.
//
// A pixel's camera space position comes back out of the stored depth, and its normal out of that position's
// slope across the screen. The light multiplies what the pixel already shows, which stands in for its surface
// colour, so the colours arrive divided by the scene's own light. The falloff matches pointlights.hlsli.

#if VERTEX

float4 ProjectionColumns[4] : register(c0);   // camera space to clip space, one column each
float4 ClipToDepth          : register(c4);   // xy scale and zw offset from clip space to the depth texture

struct VsIn
{
    float3 Corner  : POSITION;
    float2 UV      : TEXCOORD0;
    float4 Place   : TEXCOORD4;   // camera space position, and the outer radius
    float4 Color   : TEXCOORD5;   // colour over the scene's light, and 1 / (outer - inner radius)
    float3 Ambient : TEXCOORD6;   // ambient as a fraction of the colour, then the scale and offset from world height to the light's share
};

struct VsOut
{
    float4 Position : POSITION;
    float2 UV       : TEXCOORD0;
    float4 Place    : TEXCOORD1;
    float4 Color    : TEXCOORD2;
    float3 Ambient  : TEXCOORD3;
};

VsOut mainVS(VsIn input)
{
    float3 center = input.Place.xyz;
    float radius = input.Place.w;
    float distanceSquared = dot(center, center);
    float depth = abs(center.z);

    // The quad stands at the light's depth. Off the view axis by angle a, a sphere of angular radius b reaches
    // radius / cos(a + b) across it, and this is that cosine.
    float cosine = (depth * sqrt(max(distanceSquared - radius * radius, 0.0f)) -
        radius * sqrt(max(distanceSquared - depth * depth, 0.0f))) / max(distanceSquared, 1e-4f);
    float4 view = float4(center.xy + input.Corner.xy * (2.0f * radius / max(cosine, 0.25f)), center.z, 1.0f);
    float4 clip = float4(dot(view, ProjectionColumns[0]), dot(view, ProjectionColumns[1]),
        dot(view, ProjectionColumns[2]), dot(view, ProjectionColumns[3]));

    // A light the camera stands in or beside covers the screen.
    if (cosine < 0.25f)
    {
        clip = float4(input.Corner.xy * 2.0f, 0.5f, 1.0f);
    }

    VsOut output;
    output.Position = clip;
    output.UV = clip.xy / clip.w * ClipToDepth.xy + ClipToDepth.zw;
    output.Place = input.Place;
    output.Color = input.Color;
    output.Ambient = input.Ambient;
    return output;
}

#else

sampler2D SceneDepth : register(s0);

float4 ToView0 : register(c0);   // depth texture uv and stored depth to homogeneous camera space, one row each
float4 ToView1 : register(c1);
float4 ToView2 : register(c2);
float4 ToView3 : register(c3);
float4 WorldHeight : register(c4);   // camera space position to world height

struct PsIn
{
    float2 UV      : TEXCOORD0;
    float4 Place   : TEXCOORD1;
    float4 Color   : TEXCOORD2;
    float3 Ambient : TEXCOORD3;
};

float4 mainPS(PsIn input) : COLOR
{
    float stored = tex2D(SceneDepth, input.UV).r;
    float4 homogeneous = input.UV.x * ToView0 + input.UV.y * ToView1 + stored * ToView2 + ToView3;
    float3 position = homogeneous.xyz / homogeneous.w;

    // The camera sits at the origin, so a normal facing it points against the pixel's position.
    float3 normal = normalize(cross(ddy(position), ddx(position)));
    normal = (dot(normal, position) > 0.0f) ? -normal : normal;

    float3 toLight = input.Place.xyz - position;
    float distance = length(toLight);
    float ramp = saturate((input.Place.w - distance) * input.Color.w);
    float factor = ramp * ramp * (3.0f - 2.0f * ramp);
    float shade = saturate(dot(normal, toLight) / max(distance, 1e-3f));

    // A light split at the ground reaches only the pixels on its side of it.
    float share = saturate((dot(position, WorldHeight.xyz) + WorldHeight.w) * input.Ambient.y + input.Ambient.z);

    // Alpha stays 0, so the blend leaves the target's alpha as it was.
    return float4(input.Color.rgb * (factor * share * (shade + input.Ambient.x)), 0.0f);
}

#endif
