// Planar reflection and refraction on the meshes a model's PlanarMirror keys pick out.
//
// W3DPlanarMirror renders the scene mirrored in up to two level planes, at half size, before
// the views draw. Each pixel takes the mirror whose plane lies nearest its own height, and the
// map's skybox where that mirror is empty or no plane lies near. A _nrm.dds normal map beside
// the mesh's texture bends what the mirror shows, and what shows through a translucent mesh.
//
// REFRACT 0 is drawn over an opaque mesh's finished pixels at depth EQUAL, blended by the
// reflection's share. REFRACT 1 draws a translucent mesh whole: the scene copy behind it, bent,
// under the mesh's own colour, with the reflection over both. GLASS leaves the mesh's colour out,
// for any mesh, and keeps its texture's alpha only as an outline where the mesh draws with alpha.
//
// Frost blurs the reflection in every variant, and in REFRACT and GLASS blurs what shows through
// and clouds it towards the map's light. The blur's taps turn by a fresh angle at every pixel, so
// neighbours interleave into a smooth blur, and a crystal grain laid over the world scatters what
// shows through and makes the clouding patchy.
//
// Fixed-function vertex processing hands over the mesh's uv, camera-space normal and
// camera-space position as TEXCOORD0 to TEXCOORD2, so no vertex shader is needed.

sampler2D MeshTexture : register(s0);
sampler2D NormalMap   : register(s3);
sampler2D MirrorA     : register(s4);   // mirrored scenes at half size, alpha 1 where anything drew
sampler2D MirrorB     : register(s5);
#if REFRACT
sampler2D SceneCopy   : register(s6);
#endif
sampler2D FrostMap    : register(s13);  // rg = crystal scatter, b = cloud patches, a = a fresh angle per texel

float4 ToWorldX  : register(c0);   // camera space to world, one row each
float4 ToWorldY  : register(c1);
float4 ToWorldZ  : register(c2);
float4 ClipX     : register(c3);   // camera space to clip x, y and w
float4 ClipY     : register(c4);
float4 ClipW     : register(c5);
float4 ScreenMap : register(c6);   // clip to scene uv, xy scale and zw offset
float4 Planes    : register(c7);   // xy = the two mirrors' heights, far off for a missing one, z = 1 / fade distance, w = height tolerance
float4 Mirror    : register(c8);   // x = reflectivity head on, y = 1 - x, z = distortion, 0 without a normal map, w = 1 for an adding mesh, or for GLASS a mesh drawn with alpha
float4 Tint      : register(c9);
float4 SkyTint   : register(c10);  // the map's light on the skybox
float4 Frost     : register(c11);  // xy = blur radius in scene uv, z = cloudiness, w = world units to frost map uv
float4 FrostStep : register(c12);  // xy = scene uv to frost map uv at one texel a pixel

struct PsIn
{
    float2 TexCoord : TEXCOORD0;
    float3 Normal   : TEXCOORD1;
    float3 Position : TEXCOORD2;
#if REFRACT
    float4 Diffuse  : COLOR0;
#endif
};

#include "skybox.hlsli"

// Vogel spirals, which spread their taps evenly over the unit disc, two taps an entry. They come as constants,
// since fxc spends a literal register on nearly every rotated tap.
float4 Spiral6[3]  : register(c13);
float4 Spiral12[6] : register(c16);

// A spiral tap turned by the pixel's angle, given as its cosine and sine, and scaled to the frost's radius.
float2 Tap(float2 spiral, float2 turn)
{
    return float2(spiral.x * turn.x - spiral.y * turn.y, spiral.x * turn.y + spiral.y * turn.x) * Frost.xy;
}

// The half-size mirrors take six taps.
float4 BlurMirror(sampler2D s, float2 uv, float2 turn)
{
    float4 sum = 0.0f;
    for (int i = 0; i < 3; i++)
    {
        sum += tex2D(s, uv + Tap(Spiral6[i].xy, turn));
        sum += tex2D(s, uv + Tap(Spiral6[i].zw, turn));
    }
    return sum * (1.0f / 6.0f);
}

#if REFRACT
float3 BlurScene(float2 uv, float2 turn)
{
    float3 sum = 0.0f;
    for (int i = 0; i < 6; i++)
    {
        sum += tex2D(SceneCopy, uv + Tap(Spiral12[i].xy, turn)).rgb;
        sum += tex2D(SceneCopy, uv + Tap(Spiral12[i].zw, turn)).rgb;
    }
    return sum * (1.0f / 12.0f);
}
#endif

float4 main(PsIn input) : COLOR
{
    // A mesh without normals hands over zero, which faces straight up in the world instead.
    float normalLength = dot(input.Normal, input.Normal);
    float3 normal = (normalLength > 1e-8f) ? input.Normal * rsqrt(normalLength) : ToWorldZ.xyz;
    float3 toEye = normalize(-input.Position);

    float4 position = float4(input.Position, 1.0f);
    float height = dot(position, ToWorldZ);
    float2 clip = float2(dot(position, ClipX), dot(position, ClipY));
    float2 screen = clip / dot(position, ClipW) * ScreenMap.xy + ScreenMap.zw;
    float2 bend = (tex2D(NormalMap, input.TexCoord).xy * 2.0f - 1.0f) * Mirror.z;

    float2 turn;
    sincos(tex2D(FrostMap, screen * FrostStep.xy).a * 6.2831853f, turn.y, turn.x);

    float distanceA = abs(height - Planes.x);
    float distanceB = abs(height - Planes.y);
    float4 mirrorA = BlurMirror(MirrorA, screen + bend, turn);
    float4 mirrorB = BlurMirror(MirrorB, screen + bend, turn);
    float4 mirror = (distanceA <= distanceB) ? mirrorA : mirrorB;
    float onPlane = saturate(1.0f - max(min(distanceA, distanceB) - Planes.w, 0.0f) * Planes.z);

    float3 bounce = reflect(-toEye, normal);
    float3 skyward = float3(dot(bounce, ToWorldX.xyz), dot(bounce, ToWorldY.xyz), dot(bounce, ToWorldZ.xyz));
    float3 sky = SkyboxColor(skyward) * SkyTint.rgb;
    float3 reflection = lerp(sky, mirror.rgb, mirror.a * onPlane) * Tint.rgb;

    // Schlick's approximation, from the reflectivity head on to all of it at a grazing angle.
    float edge = 1.0f - saturate(dot(normal, toEye));
    float edge2 = edge * edge;
    float share = Mirror.x + Mirror.y * edge2 * edge2 * edge;

#if REFRACT
    // Each crystal of the grain bends what shows through its own way, and the clouding gathers in patches.
    float4 grain = tex2D(FrostMap, float2(dot(position, ToWorldX), dot(position, ToWorldY)) * Frost.w);
    float2 scatter = (grain.rg - 0.5f) * Frost.xy * 1.5f;
    float cloud = saturate(Frost.z * (0.6f + 0.8f * grain.b));
    float3 behind = lerp(BlurScene(screen + bend + scatter, turn), SkyTint.rgb, cloud);
#endif

#if GLASS
    float outline = (Mirror.w > 0.5f) ? tex2D(MeshTexture, input.TexCoord).a * input.Diffuse.a : 1.0f;
    return float4(lerp(behind, reflection, share), outline);
#elif REFRACT
    float4 texel = tex2D(MeshTexture, input.TexCoord) * input.Diffuse;
    float3 under = (Mirror.w > 0.5f) ? behind + texel.rgb * texel.a : lerp(behind, texel.rgb, texel.a);
    return float4(lerp(under, reflection, share), 1.0f);
#else
    return float4(reflection, share);
#endif
}
