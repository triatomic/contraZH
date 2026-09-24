// Per-pixel specular highlight and bump shading, added over a mesh that has already drawn.
//
// The mesh keeps its fixed-function lighting and this pass adds a Blinn-Phong highlight
// on top. Fixed-function vertex processing hands the shader the camera-space normal and
// position through generated texcoords, so no vertex shader is needed. The mesh's own
// texture scales the highlight by its brightness, so bright metal shines and dark paint
// barely does.
//
// The pass blends one plus destination times source alpha. Alpha darkens what the mesh
// drew where a bump turns away from the sun, and colour adds the highlight and the light
// a bump turns towards it.
//
// SHADOWED picks whether the sun is taken out where its shadow falls, and PACKED the
// shadow map's depth format. Stages 0 to 3 hold the texture, normal, position and shadow
// map, contiguous because texcoord sets are handed out in stage order.
//
// BUMP picks the surface detail. 0 is none, 1 treats the texture's brightness as height,
// and 2 reads a tangent-space normal map from stage 4 on the mesh's own UVs. Both bumped
// variants build their frame from screen-space derivatives, because meshes carry no
// tangents, so they need ps_2_a.
//
// Every variant adds the texture's _emi glow mask from stage 5, unaffected by shadow and
// bumps. Meshes without one get zero intensity, so the empty stage adds nothing.
//
// LIGHTS adds the dynamic point lights, in camera space, in place of the fixed-function
// ones the mesh was drawn without. They need ps_2_a for their length.

sampler2D MeshTexture : register(s0);

#if SHADOWED
sampler2D ShadowMap : register(s3);
// The pass only needs to know it is in shadow, and a soft edge would not fit ps_2_0.
#define SHADOW_SINGLE_TAP 1
#include "shadowreceive.hlsli"
#endif

#if BUMP == 2
sampler2D NormalMap : register(s4);
#endif

sampler2D EmissiveMap : register(s5);

float4 SunDirection : register(c1);   // camera space, towards the sun
float4 SunColor     : register(c2);   // sun colour times specular intensity
float4 Gloss        : register(c3);   // x = specular power, y = 1 for the debug view
float4 Bump         : register(c6);   // x = height of full brightness, y = normal map strength, z = ambient brightness
float4 SunDiffuse   : register(c5);   // sun colour the mesh was lit with
float4 TextureInfo  : register(c7);   // x = glow mask intensity, 0 without a mask; yz = one texel of the mesh texture in uv

#if LIGHTS
// Eight fill c8 to c25, and fxc needs the rest for literals, so W3DShaderManager::MAX_UNIT_PIXEL_LIGHTS must match.
#define POINT_LIGHT_REGISTER c8
#define POINT_LIGHT_COUNT 8
#include "pointlights.hlsli"
#endif

struct PsIn
{
    float2 TexCoord  : TEXCOORD0;
    float3 Normal    : TEXCOORD1;
    float3 Position  : TEXCOORD2;
#if SHADOWED
    float4 ShadowPos : TEXCOORD3;
#endif
};

float Brightness(float3 color)
{
    return dot(color, float3(0.3f, 0.59f, 0.11f));
}

// Edge-on pixels can leave no bump direction, and those keep the smooth normal.
float3 NormalizeOr(float3 v, float3 fallback)
{
    return (dot(v, v) > 1e-20f) ? normalize(v) : fallback;
}

#if BUMP == 1

// Brightness as height, from one mip blurrier than the pixel needs, so fine texture noise does not sparkle.
float Height(float2 uv)
{
    return Brightness(tex2Dbias(MeshTexture, float4(uv, 0.0f, 1.0f)).rgb);
}

// Height change per pixel along one screen axis. The samples sit at least a texel apart on either side,
// so a magnified texture gives smooth slopes rather than one flat facet per texel.
float HeightSlope(float2 uv, float2 step)
{
    float2 texels = step / TextureInfo.yz;
    float stretch = max(1.0f, rsqrt(max(dot(texels, texels), 1e-8f)));
    float2 reach = step * stretch;
    float change = (Height(uv + reach) - Height(uv - reach)) * 0.5f;

    // Paint lines and team colour edges jump far more than surface grain, so large jumps are softened.
    change /= 1.0f + abs(change) * 6.0f;
    return change / stretch;
}

// Mikkelsen's surface gradient.
float3 BumpNormal(float3 normal, float3 position, float2 uv)
{
    float3 dpdx = ddx(position);
    float3 dpdy = ddy(position);

    float3 r1 = cross(dpdy, normal);
    float3 r2 = cross(normal, dpdx);
    float det = dot(dpdx, r1);

    float dhdx = HeightSlope(uv, ddx(uv));
    float dhdy = HeightSlope(uv, ddy(uv));

    float3 gradient = sign(det) * (dhdx * r1 + dhdy * r2) * Bump.x;
    return NormalizeOr(abs(det) * normal - gradient, normal);
}

#elif BUMP == 2

// Schuler's cotangent frame. The determinant's sign keeps the frame the right way round
// whatever the handedness of camera space and the UV layout.
float3 BumpNormal(float3 normal, float3 position, float2 uv)
{
    float3 dpdx = ddx(position);
    float3 dpdy = ddy(position);
    float2 duvdx = ddx(uv);
    float2 duvdy = ddy(uv);

    float3 dpdyPerp = cross(dpdy, normal);
    float3 dpdxPerp = cross(normal, dpdx);
    float3 tangent = dpdyPerp * duvdx.x + dpdxPerp * duvdy.x;
    float3 bitangent = dpdyPerp * duvdx.y + dpdxPerp * duvdy.y;
    float side = (dot(dpdx, dpdyPerp) < 0.0f) ? -1.0f : 1.0f;
    float scale = side * rsqrt(max(max(dot(tangent, tangent), dot(bitangent, bitangent)), 1e-30f));

    float3 texel = tex2D(NormalMap, uv).xyz * 2.0f - 1.0f;
    texel.xy *= Bump.y;
    return NormalizeOr((tangent * texel.x + bitangent * texel.y) * scale + normal * texel.z, normal);
}

#endif

float4 main(PsIn input) : COLOR
{
    // A mesh without normals hands over zero, which must not light or bump anything.
    float normalLength = dot(input.Normal, input.Normal);
    float3 normal = input.Normal * rsqrt(max(normalLength, 1e-8f));

    float3 toSun = SunDirection.xyz;
    float3 toEye = normalize(-input.Position);
    float3 halfway = normalize(toSun + toEye);

#if BUMP
    float3 surface = BumpNormal(normal, input.Position, input.TexCoord);
#else
    float3 surface = normal;
#endif

    // Only faces turned towards the sun catch a highlight.
    float facing = saturate(dot(normal, toSun) * 4.0f);
    float highlight = pow(saturate(dot(surface, halfway)), Gloss.x) * facing;
    highlight *= (normalLength > 1e-6f) ? 1.0f : 0.0f;

    float3 texel = tex2D(MeshTexture, input.TexCoord).rgb;
    float brightness = Brightness(texel);
    highlight *= brightness * brightness;

#if SHADOWED
    float lit = ShadowLit(input.ShadowPos);
#else
    float lit = 1.0f;
#endif
    highlight *= lit;

    float3 color = SunColor.rgb * highlight;
    float darken = 1.0f;

#if BUMP
    // The mesh was lit by its smooth normal. The bump's change to the sun's share
    // darkens by the ratio of the two where it turns away, and adds where it turns towards.
    float geometric = saturate(dot(normal, toSun));
    float change = (saturate(dot(surface, toSun)) - geometric) * lit;
    float sun = Brightness(SunDiffuse.rgb);
    float before = Bump.z + sun * geometric * lit;
    darken = saturate(1.0f + sun * min(change, 0.0f) / max(before, 0.05f));
    color += texel * SunDiffuse.rgb * max(change, 0.0f);
#endif

#if LIGHTS
    color += texel * PointLighting(input.Position, surface);
#endif

    color += tex2D(EmissiveMap, input.TexCoord).rgb * TextureInfo.x;

    // The debug view tints everything the pass covers faintly and shows the highlight 8x.
    float3 debugColor = float3(1.0f, 0.0f, 1.0f) * (0.15f + highlight * 8.0f);
    return float4(lerp(color, debugColor, Gloss.y), darken);
}
