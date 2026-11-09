// Dynamic point lights added over fixed-function geometry that has already drawn.
//
// The pass redraws the geometry at the same depth and adds the texture lit by the lights,
// weighted by the texture's alpha as the first draw blended it. The world position comes
// on stage 1. The geometry carries no usable normal, so the facet's own is taken and
// turned towards the eye, which also lights both sides of two-sided meshes.
//
// The lights take ps_2_a for their length.

sampler2D MeshTexture : register(s0);

float4 EyePosition : register(c1);   // world space

// Eight fill c5 to c22, since W3DShaderManager::MAX_PIXEL_LIGHTS packs them the same for every ground shader.
#define POINT_LIGHT_REGISTER c5
#define POINT_LIGHT_COUNT 8
#include "pointlights.hlsli"

struct PsIn
{
    float2 TexCoord : TEXCOORD0;
    float3 WorldPos : TEXCOORD1;
};

float4 main(PsIn input) : COLOR
{
    float4 texel = tex2D(MeshTexture, input.TexCoord);

    float3 facet = cross(ddx(input.WorldPos), ddy(input.WorldPos));
    facet *= (dot(facet, EyePosition.xyz - input.WorldPos) < 0.0f) ? -1.0f : 1.0f;
    float3 normal = facet * rsqrt(max(dot(facet, facet), 1e-30f));

#if LIGHTS_DEBUG
    return float4(PointLighting(input.WorldPos, normal), texel.a);
#else
    return float4(texel.rgb * PointLighting(input.WorldPos, normal), texel.a);
#endif
}
