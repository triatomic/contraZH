// Terrain passes that also receive the sun's cast shadow, or bump the sun's light, or both.
//
// Each build matches one legacy terrain shader exactly. terrain.nvp blends two
// textures by vertex alpha and applies vertex lighting, and terrainnoise.nvp and
// terrainnoise2.nvp then modulate by one or two cloud or light maps. The shadow
// then scales the colour the way the legacy stencil shadow did, by a factor taken
// from the shadow colour.
//
// NOISE_COUNT (0-2) picks the legacy shader and PACKED picks the depth format. With
// two, the second map is W3DGroundNoise's texture, read through groundnoise.hlsli, and
// the first is the cloud map or white.
// SHADOWED (default 1) picks whether the shadow map is read at all. The shadow map
// sits on the first stage after the noise maps. Fixed-function vertex processing
// hands out texcoord sets in stage order, so a gap in the stages would move the
// shadow coordinates into a different register.
//
// BUMP adds the terrain normal maps, laid out like the colour atlas so the base and
// blend UVs index them too. The world position comes on the next stage and the
// normal atlas on the one after. The vertex lighting stays, and only the sun's share
// is redone per pixel, so the normal maps need ps_2_a for the derivatives that build
// their frame.
//
// LIGHTS adds the dynamic point lights the vertex lighting leaves out. They need the
// world position, on the same stage as for BUMP, and take ps_2_a for their length.
//
// Every build blends the two textures by height through heightblend.hlsli, which reads c3
// and sampler s10, and which the constants turn back into the legacy blend when it is off.
//
// SEABED hides the atlas textures' tiling under standing water with the water's hex cells.
// Each atlas texture is a block that wraps seamlessly, so a cell shifts and turns its read
// within the block. The block comes from a lookup of the atlas slot, and the read follows
// the world position, which matches the vertex UVs on every cell but cliffs, which keep
// their own. It needs the world position too, and leaves room for only three point lights.
//
// GLINT adds the sun's glint through terrainglint.hlsli. Every build with the world position has it, so
// its constants turn it off, and GLINT alone gives the world position to terrain without bumps or lights.
// Each texture's strength and gloss come from Terrain.ini through a map laid out like the colour atlas.

#ifndef EMISSIVE_ONLY
#define EMISSIVE_ONLY 0
#endif

#ifndef SHADOWED
#define SHADOWED 1
#endif

#ifndef BUMP
#define BUMP 0
#endif

#ifndef LIGHTS
#define LIGHTS 0
#endif

#ifndef SEABED
#define SEABED 0
#endif

#ifndef GLINT
#define GLINT (BUMP || LIGHTS || SEABED)
#endif

#define CONCAT_(a, b) a##b
#define CONCAT(a, b) CONCAT_(a, b)

#define SHADOW_STAGE (2 + NOISE_COUNT)

#define HEIGHT_BLEND_REGISTER c3
#include "heightblend.hlsli"

sampler2D BaseTexture  : register(s0);
sampler2D BlendTexture : register(s1);

#if NOISE_COUNT >= 1
sampler2D Noise1Texture : register(s2);
#endif
#if NOISE_COUNT >= 2
sampler2D Noise2Texture : register(s3);
#endif

#if NOISE_COUNT >= 2
#include "groundnoise.hlsli"
#endif

#if SHADOWED

#if SHADOW_STAGE == 2
sampler2D ShadowMap : register(s2);
#define SHADOW_TEXCOORD TEXCOORD2
#elif SHADOW_STAGE == 3
sampler2D ShadowMap : register(s3);
#define SHADOW_TEXCOORD TEXCOORD3
#else
sampler2D ShadowMap : register(s4);
#define SHADOW_TEXCOORD TEXCOORD4
#endif

#include "shadowreceive.hlsli"

#endif

#if BUMP || LIGHTS || SEABED || GLINT

// Stage numbers, spelled out because register names need a literal digit.
#if NOISE_COUNT + SHADOWED == 0
#define POSITION_INDEX 2
#define NORMAL_INDEX 3
#elif NOISE_COUNT + SHADOWED == 1
#define POSITION_INDEX 3
#define NORMAL_INDEX 4
#elif NOISE_COUNT + SHADOWED == 2
#define POSITION_INDEX 4
#define NORMAL_INDEX 5
#else
#define POSITION_INDEX 5
#define NORMAL_INDEX 6
#endif

#endif

#if LIGHTS
// Eight fill c5 to c22 before the glint's c23 to c25, and fxc needs the rest for literals, so W3DShaderManager::MAX_PIXEL_LIGHTS must match.
// The seabed keeps three, whose room ends inside c13, which W3DShaderManager::SEABED_PIXEL_LIGHTS matches.
#define POINT_LIGHT_REGISTER c5
#if SEABED
#define POINT_LIGHT_COUNT 3
#else
#define POINT_LIGHT_COUNT 8
#endif
#include "pointlights.hlsli"
#endif

#if SEABED

sampler2D ClassMap  : register(s8);   // per atlas slot, its texture block's first slot column and row and its width in tiles, over 255
sampler2D WaterMask : register(s9);   // standing water's coverage in alpha, its level at 1/16 unit in red and green times coverage
sampler2D Painted   : register(s13);  // painted stochastic terrain on the mask's layout: strength in red, hex weight exponent over 12 in blue
sampler2D PaintedSeed : register(s14); // the same unfiltered, whose green seed each hex cell takes whole from its centre

float4 SeabedAtlas : register(c14);   // xy = atlas size in texels, zw = 1 / that
float4 SeabedWorld : register(c15);   // x = atlas texels per world unit, y = texels of the map border, z = 1 / fade depth under the waterline
float4 SeabedHex   : register(c16);   // x = 1 / hex cell spacing, y = weight exponent, z = how far cells shift, w = twice the tangent of half the widest turn
float4 SeabedMask  : register(c17);   // hex lattice units, world xy times SeabedHex.x, to water mask texcoords: xy scale, zw offset
float4 SeabedSlot  : register(c18);   // atlas texels to lookup texcoords: x scale, y offset; z = 255 times the slot's texels, w = the atlas border's

struct HexCells
{
    float3 weight;
    float2 offset0;
    float2 offset1;
    float2 offset2;
    float2 turn0;     // cosine and sine of each cell's turn
    float2 turn1;
    float2 turn2;
};

float3 HexHash(float2 cell)
{
    float3 p = frac(float3(cell, cell.x + cell.y) * 0.1031f);
    p = frac(p + dot(p, p.yzx + 20.0f));
    p = frac(p + dot(p, p.yzx + 20.0f));
    return p;
}

// The half-angle tangent gives an exact cosine and sine without the cost of sincos.
float2 HexTurn(float random)
{
    float t = (random - 0.5f) * SeabedHex.w;
    return float2(1.0f - t * t, 2.0f * t) / (1.0f + t * t);
}

// A hex cell's painted seed, read at its centre, as an offset for its hash. Unpainted cells read zero and keep the water's.
float2 HexSeed(float2 lattice)
{
    float2 centre = float2(lattice.x + 0.5f * lattice.y, lattice.y);
    return lattice + tex2D(PaintedSeed, centre * SeabedMask.xy + SeabedMask.zw).g * float2(173.0f, 311.0f);
}

// The water's hex cells at st in lattice units, with shifts as fractions of a texture block, and the weight exponent the paint may set.
HexCells FindHexCells(float2 st, float exponent)
{
    float2 skewed = float2(st.x - 0.5f * st.y, st.y);
    float2 base = floor(skewed);
    float3 corner = float3(frac(skewed), 0.0f);
    corner.z = 1.0f - corner.x - corner.y;
    float s = step(0.0f, -corner.z);
    float s2 = 2.0f * s - 1.0f;

    float3 weight = pow(saturate(float3(-corner.z * s2, s - corner.y * s2, s - corner.x * s2)), exponent);

    HexCells cells;
    cells.weight = weight / dot(weight, 1.0f);
    float3 random0 = HexHash(HexSeed(base + float2(s, s)));
    float3 random1 = HexHash(HexSeed(base + float2(s, 1.0f - s)));
    float3 random2 = HexHash(HexSeed(base + float2(1.0f - s, s)));
    cells.offset0 = random0.xy * SeabedHex.z;
    cells.offset1 = random1.xy * SeabedHex.z;
    cells.offset2 = random2.xy * SeabedHex.z;
    cells.turn0 = HexTurn(random0.z);
    cells.turn1 = HexTurn(random1.z);
    cells.turn2 = HexTurn(random2.z);
    return cells;
}

// A mul and a mad, as two dot products would be two dp2adds, which the runtime counts as two slots each against ps_2_a's 512.
float2 Turn(float2 texel, float2 turn)
{
    return texel.x * turn + texel.y * float2(-turn.y, turn.x);
}

// A texel position wrapped into its block, as atlas texcoords.
float2 BlockUV(float2 texel, float3 block)
{
    return (block.xy + texel - block.z * floor(texel / block.z)) * SeabedAtlas.zw;
}

// One atlas texture read through the hex cells. The blend has no mean to hold contrast against,
// since the atlas's smallest mip mixes every texture, so it is a plain weighted sum.
float4 SeabedSample(sampler2D atlas, float4 plain, float2 uv, float2 texel, float2 dx, float2 dy, HexCells cells)
{
    float2 atlasTexel = uv * SeabedAtlas.xy;
    // Point sampling picks the slot.
    float3 slots = tex2D(ClassMap, atlasTexel * SeabedSlot.x + SeabedSlot.y).rgb;
    float3 block = float3(slots.xy * SeabedSlot.z + SeabedSlot.w, slots.z * (255.0f * 64.0f));

    // Cliff cells lay out their own texels, which the world position does not reach.
    float2 drift = texel - (atlasTexel - block.xy);
    drift -= block.z * floor(drift / block.z + 0.5f);
    float onGrid = step(dot(drift, drift), 1.0f);

    // Each read's gradients turn in texels, then scale, since the atlas need not be square.
    float4 sum = cells.weight.x * tex2Dgrad(atlas, BlockUV(Turn(texel, cells.turn0) + cells.offset0 * block.z, block), Turn(dx, cells.turn0) * SeabedAtlas.zw, Turn(dy, cells.turn0) * SeabedAtlas.zw);
    sum += cells.weight.y * tex2Dgrad(atlas, BlockUV(Turn(texel, cells.turn1) + cells.offset1 * block.z, block), Turn(dx, cells.turn1) * SeabedAtlas.zw, Turn(dy, cells.turn1) * SeabedAtlas.zw);
    sum += cells.weight.z * tex2Dgrad(atlas, BlockUV(Turn(texel, cells.turn2) + cells.offset2 * block.z, block), Turn(dx, cells.turn2) * SeabedAtlas.zw, Turn(dy, cells.turn2) * SeabedAtlas.zw);
    return lerp(plain, sum, onGrid);
}

#endif

#if BUMP || GLINT
float4 ToSun      : register(c1);   // world space, w = normal map strength
float4 SunColor   : register(c2);   // the sun's diffuse colour in the vertex lighting, w = 1 for the debug view
#endif

#if GLINT
#include "terrainglint.hlsli"

// Point sampled, since each texture's block holds one value. Red is the strength over GlintAlbedo's scale, green the gloss over GlintEye.w.
sampler2D GlintMaterials : register(s12);
#endif

#if BUMP

sampler2D NormalAtlas : register(CONCAT(s, NORMAL_INDEX));

// The atlas holds x in luminance and y in alpha, and z comes back from unit length.
float3 AtlasNormal(float2 uv)
{
    float4 texel = tex2D(NormalAtlas, uv);
    float2 xy = float2(texel.r, texel.a) * 2.0f - 1.0f;
    return float3(xy * ToSun.w, sqrt(saturate(1.0f - dot(xy, xy))));
}

// Schuler's cotangent frame from the UV derivatives, so cliff cells with their own
// UV layout bump the right way too. The determinant's sign keeps it the right way round.
float3 BumpNormal(float3 normal, float3 dpdyPerp, float3 dpdxPerp, float side, float2 uv)
{
    float2 duvdx = ddx(uv);
    float2 duvdy = ddy(uv);
    float3 tangent = dpdyPerp * duvdx.x + dpdxPerp * duvdy.x;
    float3 bitangent = dpdyPerp * duvdx.y + dpdxPerp * duvdy.y;
    float scale = side * rsqrt(max(max(dot(tangent, tangent), dot(bitangent, bitangent)), 1e-30f));

    float3 texel = AtlasNormal(uv);
    return (tangent * texel.x + bitangent * texel.y) * scale + normal * texel.z;
}

#endif

struct PsIn
{
    float4 Diffuse   : COLOR0;
    float2 BaseUV    : TEXCOORD0;
    float2 BlendUV   : TEXCOORD1;
#if NOISE_COUNT >= 1
    float2 Noise1UV  : TEXCOORD2;
#endif
#if NOISE_COUNT >= 2
    float2 Noise2UV  : TEXCOORD3;
#endif
#if SHADOWED
    float4 ShadowPos : SHADOW_TEXCOORD;
#endif
#if BUMP || LIGHTS || SEABED || GLINT
    float3 WorldPos  : CONCAT(TEXCOORD, POSITION_INDEX);
#endif
};

float4 main(PsIn input) : COLOR
{
    float4 base = tex2D(BaseTexture, input.BaseUV);
    float4 blend = tex2D(BlendTexture, input.BlendUV);

#if SEABED
    // The mask's level is premultiplied by its coverage, so dividing it back out blends only water cells.
    float2 st = input.WorldPos.xy * SeabedHex.x;
    float2 maskUV = st * SeabedMask.xy + SeabedMask.zw;
    float4 mask = tex2D(WaterMask, maskUV);
    float level = dot(mask.rg, float2(255.0f * 256.0f / 16.0f, 255.0f / 16.0f)) / max(mask.a, 0.001f);
    float seabed = saturate((level - input.WorldPos.z) * SeabedWorld.z) * step(0.5f, mask.a);

    // Painted cells blend by their rate, from 12 for patches to 1 for a smooth mix, and unpainted ones by the water's.
    float4 painted = tex2D(Painted, maskUV);
    seabed = max(seabed, painted.r);
    float exponent = painted.b * 12.0f;

    // The atlas runs u along the world's x and v against its y, SeabedWorld.x texels per unit.
    float2 texel = float2(input.WorldPos.x, -input.WorldPos.y) * SeabedWorld.x + float2(SeabedWorld.y, -SeabedWorld.y);
    float2 texelDx = ddx(texel);
    float2 texelDy = ddy(texel);

    // ps_2_a cannot branch, so dry pixels pay for this too, and W3DShaderManager only draws tiles with water through it.
    HexCells cells = FindHexCells(st, exponent);
    base = lerp(base, SeabedSample(BaseTexture, base, input.BaseUV, texel, texelDx, texelDy, cells), seabed);
    blend = lerp(blend, SeabedSample(BlendTexture, blend, input.BlendUV, texel, texelDx, texelDy, cells), seabed);
#endif

    float weight = HeightBlendWeight(input.Diffuse.a, tex2D(HeightAtlas, input.BaseUV).r, tex2D(HeightAtlas, input.BlendUV).r);
    float4 color = lerp(base, blend, weight);
#if EMISSIVE_ONLY
    return float4(color.rgb * HeightBlend.w, 0.0f);
#else
#if GLINT
    float2 material = lerp(tex2D(GlintMaterials, input.BaseUV).rg, tex2D(GlintMaterials, input.BlendUV).rg, weight);
    float glintStrength = (dot(color.rgb, GlintAlbedo.xyz) + GlintAlbedo.w) * material.r;
    float glintGloss = max(material.g * GlintEye.w, 1.0f);
#endif

#if SHADOWED
    float lit = ShadowLit(input.ShadowPos);
#else
    float lit = 1.0f;
#endif

#if BUMP || LIGHTS
    // The facet's own normal, turned up, since terrain never faces down.
    float3 dpdx = ddx(input.WorldPos);
    float3 dpdy = ddy(input.WorldPos);
    float3 facet = cross(dpdx, dpdy);
    facet *= (facet.z < 0.0f) ? -1.0f : 1.0f;
    float3 normal = facet * rsqrt(max(dot(facet, facet), 1e-30f));
#endif

#if BUMP
    float3 dpdyPerp = cross(dpdy, normal);
    float3 dpdxPerp = cross(normal, dpdx);
    float side = (dot(dpdx, dpdyPerp) < 0.0f) ? -1.0f : 1.0f;

    float3 bumped = lerp(BumpNormal(normal, dpdyPerp, dpdxPerp, side, input.BaseUV),
                         BumpNormal(normal, dpdyPerp, dpdxPerp, side, input.BlendUV), weight);
    bumped = (dot(bumped, bumped) > 1e-20f) ? normalize(bumped) : normal;

    // The vertex lighting holds the sun on the smooth surface. The bump only changes the sun's share.
    float change = saturate(dot(bumped, ToSun.xyz)) - saturate(dot(normal, ToSun.xyz));
    float3 light = input.Diffuse.rgb + SunColor.rgb * change * lit;
#elif LIGHTS
    float3 bumped = normal;
    float3 light = input.Diffuse.rgb;
#endif

#if LIGHTS && LIGHTS_DEBUG
    return float4(PointLighting(input.WorldPos, bumped), color.a * input.Diffuse.a);
#elif LIGHTS
    light += PointLighting(input.WorldPos, bumped);
#endif

#if BUMP || LIGHTS
    color.rgb *= saturate(light);
    color.a *= input.Diffuse.a;
#else
    color *= input.Diffuse;
#endif

#if NOISE_COUNT >= 1
    float4 cloud = tex2D(Noise1Texture, input.Noise1UV);
#if !SHADOWED
    color *= cloud;
#endif
#endif
#if NOISE_COUNT >= 2
    color.rgb *= GroundNoise(Noise2Texture, input.Noise2UV);
#endif

#if SHADOWED && NOISE_COUNT >= 1
    color.rgb *= CloudShadowFactor(lit, cloud.rgb);
#elif SHADOWED
    color.rgb *= lerp(ShadowColor.rgb, float3(1.0f, 1.0f, 1.0f), lit);
#endif

#if GLINT
    // The bump tilts the smooth normal as far as it tilts the facet's.
#if BUMP
    float3 glintNormal = normalize(GlintNormal(input.WorldPos) + bumped - normal);
#else
    float3 glintNormal = GlintNormal(input.WorldPos);
#endif
    float glint = Glint(input.WorldPos, glintNormal, glintStrength * lit, glintGloss);
#if SEABED
    glint *= 1.0f - seabed;
#endif
#if NOISE_COUNT >= 1
    color.rgb += SunColor.rgb * cloud.rgb * glint;
#else
    color.rgb += SunColor.rgb * glint;
#endif
#endif

#if BUMP
    // The debug view shows only the bump's shading, 4x, on grey.
    color.rgb = lerp(color.rgb, saturate(0.5f + change * 4.0f).xxx, SunColor.w);
#endif
    return color;
#endif
}
