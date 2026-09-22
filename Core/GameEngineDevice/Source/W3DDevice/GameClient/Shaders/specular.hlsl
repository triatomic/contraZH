// Per-pixel specular highlight, added over a mesh that has already drawn.
//
// The mesh keeps its fixed-function lighting and this pass adds a Blinn-Phong highlight
// on top, blended one plus one. Fixed-function vertex processing hands the shader the
// camera-space normal and position through generated texcoords, so no vertex shader is
// needed. The mesh's own texture scales the highlight by its brightness, so bright metal
// shines and dark paint barely does.
//
// SHADOWED picks whether the highlight is taken out where the sun's shadow falls, and
// PACKED the shadow map's depth format. Stages 0 to 3 hold the texture, normal, position
// and shadow map, contiguous because texcoord sets are handed out in stage order.

sampler2D MeshTexture : register(s0);

#if SHADOWED
sampler2D ShadowMap : register(s3);
// The highlight only needs to know it is in shadow, and a soft edge would not fit ps_2_0.
#define SHADOW_SINGLE_TAP 1
#include "shadowreceive.hlsli"
#endif

float4 SunDirection : register(c1);   // camera space, towards the sun
float4 SunColor     : register(c2);   // sun colour times intensity
float4 Gloss        : register(c3);   // x = specular power, y = 1 for the debug view

struct PsIn
{
    float2 TexCoord  : TEXCOORD0;
    float3 Normal    : TEXCOORD1;
    float3 Position  : TEXCOORD2;
#if SHADOWED
    float4 ShadowPos : TEXCOORD3;
#endif
};

float4 main(PsIn input) : COLOR
{
    // A mesh without normals hands over zero, which normalises to NaN and would draw black.
    float normalLength = dot(input.Normal, input.Normal);
    float3 normal = input.Normal * rsqrt(max(normalLength, 1e-8f));

    float3 toSun = SunDirection.xyz;
    float3 toEye = normalize(-input.Position);
    float3 halfway = normalize(toSun + toEye);

    // Only faces turned towards the sun catch a highlight.
    float facing = saturate(dot(normal, toSun) * 4.0f);
    float highlight = pow(saturate(dot(normal, halfway)), Gloss.x) * facing;
    highlight *= (normalLength > 1e-6f) ? 1.0f : 0.0f;

    float3 texel = tex2D(MeshTexture, input.TexCoord).rgb;
    float brightness = dot(texel, float3(0.3f, 0.59f, 0.11f));
    highlight *= brightness * brightness;

#if SHADOWED
    highlight *= ShadowLit(input.ShadowPos);
#endif

    // The debug view tints everything the pass covers faintly and shows the highlight 8x.
    float3 debugColor = float3(1.0f, 0.0f, 1.0f) * (0.15f + highlight * 8.0f);
    return float4(lerp(SunColor.rgb * highlight, debugColor, Gloss.y), 1.0f);
}
