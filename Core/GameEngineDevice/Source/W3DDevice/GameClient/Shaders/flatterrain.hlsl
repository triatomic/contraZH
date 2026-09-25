// Flat terrain tiles with the dynamic point lights added to their vertex lighting.
//
// Each build matches one legacy flat terrain shader. fterrain0.nvp reads only the tile's
// texture on stage 1. fterrain.nvp, fterrainnoise.nvp and fterrainnoise2.nvp multiply in
// stage 0 and then stages 2 and 3, whichever of shroud, cloud and light map were set.
// Every texture and the vertex diffuse are multiplied on colour and alpha alike.
//
// TEXTURE_COUNT (1-4) picks the legacy shader. The world position comes on the first
// stage past the textures, and never below 2, since stage 1 always holds the tile.
// The lights take ps_2_a for their length.

#define CONCAT_(a, b) a##b
#define CONCAT(a, b) CONCAT_(a, b)

#if TEXTURE_COUNT <= 2
#define POSITION_INDEX 2
#elif TEXTURE_COUNT == 3
#define POSITION_INDEX 3
#else
#define POSITION_INDEX 4
#endif

sampler2D TileTexture : register(s1);

#if TEXTURE_COUNT >= 2
sampler2D Stage0Texture : register(s0);
#endif
#if TEXTURE_COUNT >= 3
sampler2D Stage2Texture : register(s2);
#endif
#if TEXTURE_COUNT >= 4
sampler2D Stage3Texture : register(s3);
#endif

// Nine fill c5 to c25, and fxc needs the rest for literals, so W3DShaderManager::MAX_PIXEL_LIGHTS must match.
#define POINT_LIGHT_REGISTER c5
#define POINT_LIGHT_COUNT 9
#include "pointlights.hlsli"

struct PsIn
{
    float4 Diffuse  : COLOR0;
#if TEXTURE_COUNT >= 2
    float2 Stage0UV : TEXCOORD0;
#endif
    float2 TileUV   : TEXCOORD1;
#if TEXTURE_COUNT >= 3
    float2 Stage2UV : TEXCOORD2;
#endif
#if TEXTURE_COUNT >= 4
    float2 Stage3UV : TEXCOORD3;
#endif
    float3 WorldPos : CONCAT(TEXCOORD, POSITION_INDEX);
};

float4 main(PsIn input) : COLOR
{
    float4 color = tex2D(TileTexture, input.TileUV);

    // The facet's own normal, turned up, since terrain never faces down.
    float3 facet = cross(ddx(input.WorldPos), ddy(input.WorldPos));
    facet *= (facet.z < 0.0f) ? -1.0f : 1.0f;
    float3 normal = facet * rsqrt(max(dot(facet, facet), 1e-30f));

    color.rgb *= saturate(input.Diffuse.rgb + PointLighting(input.WorldPos, normal));
    color.a *= input.Diffuse.a;

#if TEXTURE_COUNT >= 2
    color *= tex2D(Stage0Texture, input.Stage0UV);
#endif
#if TEXTURE_COUNT >= 3
    color *= tex2D(Stage2Texture, input.Stage2UV);
#endif
#if TEXTURE_COUNT >= 4
    color *= tex2D(Stage3Texture, input.Stage3UV);
#endif
    return color;
}
