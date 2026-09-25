// Standing water and rivers, shaded per pixel over a copy of the scene behind them.
//
// The water draws with blending off and composites itself. It refracts the scene copy
// through the waves, tints it by the water's depth over the terrain, and adds a Fresnel
// reflection, a sun glint and foam. The reflection is the scene mirrored in the water plane
// where W3DWater rendered one, and the map's skybox wherever that mirror is empty or absent.
// The shadow map darkens the glint, the reflection and the water colour where units and
// cliffs shade the water.
//
// Fixed-function vertex processing hands out texcoord sets in stage order, so stages 0 to
// 2 each hold a texture. Stage 2 carries the world position, and every later sampler reads
// coordinates computed from it.
//
// PACKED picks the shadow map's depth format. SWELL picks the ps_3_0 build that follows
// shaderwaterswell.hlsl. It reads the same swell per pixel at fixed world points, to tilt the
// normal and raise foam on the crests without shimmering as a camera-centred grid slides.
// RADIAL adds the water mask for its camera-centred grid, which spans every lake at one level.
// RIVER picks the river build. It fades to the untouched scene by the river texture's
// alpha and its edge texture, and one wave layer follows the flow.
//
// Hex-tile stochastic texturing hides the tiling of the water texture, waves and foam.
//
// DEPTH_TEST keeps refraction from pulling in anything standing above the water, by the scene's
// depth. The packed ps_2_a water build has no constant register left for it, nor do rivers.
// HEX_TURN also turns each hex cell's water texture by its own angle, in the swell builds, which
// alone have the temps and slots for it.

#define DEPTH_TEST (!RIVER && (SWELL || !PACKED))
#define HEX_TURN (DEPTH_TEST && SWELL)

sampler2D WaterTexture  : register(s0);
#if DEPTH_TEST
sampler2D SceneDepth    : register(s1);   // the scene's INTZ depth, or white where it is unavailable
#else
sampler2D EdgeTexture   : register(s1);
#endif
sampler2D NormalMap     : register(s2);
sampler2D ShroudTexture : register(s3);
sampler2D HeightTexture : register(s4);
sampler2D SceneTexture  : register(s5);
sampler2D ShadowMap     : register(s6);
sampler2D FoamTexture   : register(s7);
sampler2D SkyNorth      : register(s8);
sampler2D SkyEast       : register(s9);
sampler2D SkySouth      : register(s10);
sampler2D SkyWest       : register(s11);
sampler2D SkyTop        : register(s12);
sampler2D Reflection    : register(s13);   // mirrored scene at half size, alpha 1 where anything drew
#if RADIAL
sampler2D WaterMask     : register(s14);   // standing water's coverage in alpha, its level at 1/16 unit in red and green times coverage
#endif
#if SWELL
sampler2D SwellMap      : register(s15);   // the vertex shader's swell texture
#endif

// c0 and c4 belong to shadowreceive.hlsli.
float4 ScreenU       : register(c1);   // world to scene-copy texcoords, before the divide by ScreenW
float4 ScreenV       : register(c2);
float4 ScreenW       : register(c3);
float4 Camera        : register(c5);   // world position, w = time
float4 ToSun         : register(c6);   // world space, w = specular power
float4 SunColor      : register(c7);   // sun colour times specular strength, w = how far from land shore foam reaches, in world units
float4 SkyTint       : register(c8);   // the map's light on the skybox, w = reflection strength
float4 HeightMapping : register(c9);   // world xy to height texcoords: xy scale, zw offset
float4 HeightDecode  : register(c10);  // x,y = high and low byte weights, z = wave texcoord scale, w = wave strength
float4 WaterParams   : register(c11);  // x = clarity, y = deep opacity, z = 1 / foam depth, w = refraction strength
float4 Absorption    : register(c12);  // per-channel absorption, red fastest, w = 1 when the shadow map is bound
float4 ShroudMapping : register(c13);  // world xy to shroud texcoords: xy scale, zw offset
float4 ShadowU       : register(c14);  // world to shadow map, one output component each
float4 ShadowV       : register(c15);
float4 ShadowZ       : register(c16);
float4 ShadowW       : register(c17);
float4 Planar        : register(c18);  // x = mirror plane height, far below all water without a mirror, y = 1 / fade distance, z = distortion, w = foam strength
float4 PlanarMap     : register(c19);  // xy = texel centre shift from the scene copy, z = height tolerance, w = added reflection
#if RADIAL
float4 RadialPlane   : register(c20);  // x = water level of this draw
#endif
float4 Surface       : register(c21);  // x = 1 / shore fade depth, y = 1 / hex cell spacing, z = hex weight exponent, w = how far hex cells shift, 0 when off
#if SWELL
float4 SwellShape    : register(c22);  // as the vertex shader's Swell: x = world to texcoord scale, y = height, zw = drift
float4 SwellChannel  : register(c23);  // picks the channel holding height
float4 SwellStep     : register(c24);  // x = world step for the slope
#endif
#if DEPTH_TEST
float4 DepthMapping  : register(c25);  // xy = stored scene depth of a point at clip w, as x + y / w, z = twice the tangent of half the widest hex turn
#endif

#include "shadowreceive.hlsli"

struct PsIn
{
    float4 Diffuse  : COLOR0;
    float2 BaseUV   : TEXCOORD0;
    float2 EdgeUV   : TEXCOORD1;
    float3 WorldPos : TEXCOORD2;
};

struct HexCells
{
    float3 weight;
    float norm;       // 1 / length of the weights, which keeps the blend's contrast
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
#if HEX_TURN
    float t = (random - 0.5f) * DepthMapping.z;
    return float2(1.0f - t * t, 2.0f * t) / (1.0f + t * t);
#else
    return float2(1.0f, 0.0f);
#endif
}

// The three hex cells around a world point, after Mikkelsen's hex-tiling.
HexCells FindHexCells(float2 world)
{
    // A plain half shear gives near-equilateral cells without a sqrt 3 constant, which ps_2_a has no room for.
    float2 st = world * Surface.y;
    float2 skewed = float2(st.x - 0.5f * st.y, st.y);
    float2 base = floor(skewed);
    float3 corner = float3(frac(skewed), 0.0f);
    corner.z = 1.0f - corner.x - corner.y;
    float s = step(0.0f, -corner.z);
    float s2 = 2.0f * s - 1.0f;

    float3 weight = pow(saturate(float3(-corner.z * s2, s - corner.y * s2, s - corner.x * s2)), Surface.z);
    weight /= dot(weight, 1.0f);

    HexCells cells;
    cells.weight = weight;
    // Shifted samples differ less as the shift shrinks, so the contrast correction shrinks with it.
    cells.norm = lerp(1.0f, rsqrt(dot(weight, weight)), Surface.w);
    float3 random0 = HexHash(base + float2(s, s));
    float3 random1 = HexHash(base + float2(s, 1.0f - s));
    float3 random2 = HexHash(base + float2(1.0f - s, s));
    cells.offset0 = random0.xy * Surface.w;
    cells.offset1 = random1.xy * Surface.w;
    cells.offset2 = random2.xy * Surface.w;
    cells.turn0 = HexTurn(random0.z);
    cells.turn1 = HexTurn(random1.z);
    cells.turn2 = HexTurn(random2.z);
    return cells;
}

// The offset jumps between cells, so the mip comes from the unshifted texcoords' gradients.
float4 HexSample(sampler2D map, HexCells cells, float2 uv, float2 dx, float2 dy, float4 mean)
{
    float4 sum = cells.weight.x * (tex2Dgrad(map, uv + cells.offset0, dx, dy) - mean);
    sum += cells.weight.y * (tex2Dgrad(map, uv + cells.offset1, dx, dy) - mean);
    sum += cells.weight.z * (tex2Dgrad(map, uv + cells.offset2, dx, dy) - mean);
    return mean + sum * cells.norm;
}

float2 Turn(float2 uv, float2 turn)
{
    return float2(dot(uv, float2(turn.x, -turn.y)), dot(uv, turn.yx));
}

// As HexSample, with each cell turning the pattern about its origin before the shared drift, so every cell flows one way.
// Turning keeps the gradients' lengths, which is all the isotropic filter's mip choice reads.
float4 HexSampleTurned(sampler2D map, HexCells cells, float2 uv, float2 drift, float2 dx, float2 dy, float4 mean)
{
    float4 sum = cells.weight.x * (tex2Dgrad(map, Turn(uv, cells.turn0) + cells.offset0 + drift, dx, dy) - mean);
    sum += cells.weight.y * (tex2Dgrad(map, Turn(uv, cells.turn1) + cells.offset1 + drift, dx, dy) - mean);
    sum += cells.weight.z * (tex2Dgrad(map, Turn(uv, cells.turn2) + cells.offset2 + drift, dx, dy) - mean);
    return mean + sum * cells.norm;
}

float4 TextureMean(sampler2D map)
{
    return tex2Dbias(map, float4(0.0f, 0.0f, 0.0f, 20.0f));
}

float2 WaveSlope(float2 uv)
{
    return tex2D(NormalMap, uv).rg * 2.0f - 1.0f;
}

float2 HexWaveSlope(HexCells cells, float2 uv, float2 dx, float2 dy, float4 mean)
{
    return HexSample(NormalMap, cells, uv, dx, dy, mean).rg * 2.0f - 1.0f;
}

// A world-space tilt as a screen offset, with the view's right along u and ahead up the screen.
float2 TiltOnScreen(float2 tilt, float2 ahead)
{
    return float2(tilt.x * ahead.y - tilt.y * ahead.x, -dot(tilt, ahead));
}

#if SWELL
float SwellLayer(float2 uv)
{
    return dot(tex2D(SwellMap, uv), SwellChannel) * 2.0f - 1.0f;
}

// The height the vertex shader lifts the water by at this world point.
float SwellHeight(float2 world)
{
    float2 uv = world * SwellShape.x;
    return (0.65f * SwellLayer(uv + SwellShape.zw) + 0.35f * SwellLayer(uv * 1.7f - SwellShape.wz * 1.3f)) * SwellShape.y;
}
#endif

// The skybox is a box around the camera with five faces, no bottom, laid out as new_skybox.w3d
// maps them. Every face is sampled and the one the ray leaves through is kept.
float3 SkyboxColor(float3 dir)
{
    dir.z = max(dir.z, 0.02f);
    float3 a = abs(dir);
    float2 pX = dir.yz / a.x;
    float2 pY = dir.xz / a.y;
    float2 pZ = dir.xy / a.z;

    float3 north = tex2D(SkyNorth, float2(1.0f - pX.x, 1.0f - pX.y) * 0.5f).rgb;
    float3 south = tex2D(SkySouth, float2(1.0f + pX.x, 1.0f - pX.y) * 0.5f).rgb;
    float3 east  = tex2D(SkyEast,  float2(1.0f + pY.x, 1.0f - pY.y) * 0.5f).rgb;
    float3 west  = tex2D(SkyWest,  float2(1.0f - pY.x, 1.0f - pY.y) * 0.5f).rgb;
    float3 top   = tex2D(SkyTop,   float2(1.0f - pZ.x, 1.0f + pZ.y) * 0.5f).rgb;

    float3 side = (a.x >= a.y) ? ((dir.x < 0.0f) ? north : south) : ((dir.y < 0.0f) ? east : west);
    return (a.z >= max(a.x, a.y)) ? top : side;
}

float4 main(PsIn input) : COLOR
{
    float3 world = input.WorldPos;
    float4 worldPoint = float4(world, 1.0f);
    float time = Camera.w;
    float2 mapUV = world.xy * HeightMapping.xy + HeightMapping.zw;

#if RADIAL
    // Dividing by coverage undoes the premultiply, so filtering blends only covered levels.
    float4 mask = tex2D(WaterMask, mapUV);
    float maskLevel = dot(mask.rg, float2(255.0f * 256.0f / 16.0f, 255.0f / 16.0f)) / max(mask.a, 0.001f);
    clip(mask.a - 0.5f);
    clip(0.5f - abs(maskLevel - RadialPlane.x));
#endif

    // The height texture holds each terrain height as a high and a low byte.
    float2 heightBytes = tex2D(HeightTexture, mapUV).rg;
    float depth = max(world.z - dot(heightBytes, HeightDecode.xy), 0.0f);

    HexCells cells = FindHexCells(world.xy);
    float2 waveUV = world.xy * HeightDecode.z;
    float2 waveDx = ddx(waveUV);
    float2 waveDy = ddy(waveUV);
    float4 waveMean = TextureMean(NormalMap);
    float2 slope = HexWaveSlope(cells, waveUV + time * float2(0.031f, 0.017f), waveDx, waveDy, waveMean);
    // The fine, fast layer repeats too small and moves too quickly to show, so it skips the hex cells.
    slope += WaveSlope(waveUV * 2.7f + time * float2(-0.023f, 0.037f));
#if RIVER
    slope += WaveSlope(input.BaseUV * float2(1.0f, 2.0f));
    slope *= 0.33f;
#else
    slope *= 0.5f;
#endif

    // Distortion follows the ripples about their average, so a lean in the normal map or the swell cannot slide whole images aside.
    float2 ripple = slope - (waveMean.rg * 2.0f - 1.0f);
#if SWELL
    // The vertex shader flattens the swell towards the shore, so its shading and crest foam follow.
    float shoal = saturate(depth / max(2.0f * SwellShape.y, 0.001f));
    float swellHere = SwellHeight(world.xy);
    float2 swellSlope = float2(swellHere - SwellHeight(world.xy + float2(SwellStep.x, 0.0f)),
        swellHere - SwellHeight(world.xy + float2(0.0f, SwellStep.x))) * (shoal / SwellStep.x);
    swellHere *= shoal;
    float3 normal = normalize(float3(slope * HeightDecode.w + swellSlope, 1.0f));
#else
    float3 normal = normalize(float3(slope * HeightDecode.w, 1.0f));
#endif

    // A bent sample far from the straight one likely landed on something standing in or
    // over the water, which must not smear into it, so the straight sample is kept there.
    float invW = 1.0f / dot(worldPoint, ScreenW);
    float2 screen = float2(dot(worldPoint, ScreenU), dot(worldPoint, ScreenV)) * invW;
    // Clip-space w grows along the view, so its world gradient points the camera ahead however it turns.
    float2 ahead = ScreenW.xy * rsqrt(max(dot(ScreenW.xy, ScreenW.xy), 1.0f / 255.0f));
    float2 bend = TiltOnScreen(ripple, ahead) * WaterParams.w * saturate(depth * 0.125f);
    float3 straight = tex2D(SceneTexture, screen).rgb;
    float3 bent = tex2D(SceneTexture, screen + bend).rgb;
    float3 delta = bent - straight;
    float bendable = saturate((0.15f - dot(delta, delta)) * 20.0f);
#if DEPTH_TEST
    // Anything nearer than the water surface stands above it, so the bent sample must not pull it in.
    bendable *= step(DepthMapping.x + DepthMapping.y * invW, tex2D(SceneDepth, screen + bend).r);
#endif
    float3 scene = lerp(straight, bent, bendable);

    float4 shadowPos = float4(dot(worldPoint, ShadowU), dot(worldPoint, ShadowV), dot(worldPoint, ShadowZ), dot(worldPoint, ShadowW));
    float lit = lerp(1.0f, ShadowLit(shadowPos), Absorption.w);
    float3 shade = lerp(ShadowColor.rgb, float3(1.0f, 1.0f, 1.0f), lit);

#if RIVER
    float4 water = tex2D(WaterTexture, input.BaseUV);
#else
    float4 water = saturate(HexSampleTurned(WaterTexture, cells, input.BaseUV, 0.0f, ddx(input.BaseUV), ddy(input.BaseUV), TextureMean(WaterTexture)));
#endif
    float3 body = water.rgb * input.Diffuse.rgb * shade;
    float3 transmission = exp(-depth * WaterParams.x * Absorption.rgb);
    float3 opacity = WaterParams.y * (1.0f - transmission);

    float3 toEye = normalize(Camera.xyz - world);
    float facing = saturate(dot(normal, toEye));
    float fresnel = 0.02f + 0.98f * pow(1.0f - facing, 5.0f);
    float3 sky = SkyboxColor(reflect(-toEye, normal)) * SkyTint.rgb * lerp(0.6f, 1.0f, lit);

    // Water off the mirror plane, as on other lakes or sloping rivers, keeps the skybox.
    float4 mirror = tex2D(Reflection, screen + PlanarMap.xy + TiltOnScreen(ripple * HeightDecode.w, ahead) * Planar.z);
    float onPlane = saturate(1.0f - max(abs(world.z - Planar.x) - PlanarMap.z, 0.0f) * Planar.y);
    float mirrored = mirror.a * onPlane;
    sky = lerp(sky, mirror.rgb * lerp(0.6f, 1.0f, lit), mirrored);

    // Some water colour always shows through.
    float reflection = min(fresnel * SkyTint.w + mirrored * PlanarMap.w, 0.8f);

    float3 halfway = normalize(ToSun.xyz + toEye);
    float highlight = dot(normal, halfway);
    float glint = (pow(saturate(highlight), ToSun.w) + 0.08f * pow(saturate(highlight), ToSun.w * 0.06f)) * lit;

    // Land within reach counts as shallow, so walls and jetties rising out of deep water gather foam too.
    float2 reach = SunColor.w * HeightMapping.xy;
    float4 ground = float4(dot(tex2D(HeightTexture, mapUV + float2(reach.x, 0.0f)).rg, HeightDecode.xy),
        dot(tex2D(HeightTexture, mapUV - float2(reach.x, 0.0f)).rg, HeightDecode.xy),
        dot(tex2D(HeightTexture, mapUV + float2(0.0f, reach.y)).rg, HeightDecode.xy),
        dot(tex2D(HeightTexture, mapUV - float2(0.0f, reach.y)).rg, HeightDecode.xy));
    float shallowest = min(depth, max(world.z - max(max(ground.x, ground.y), max(ground.z, ground.w)), 0.0f));
    float foamMask = saturate(1.0f - shallowest * WaterParams.z);
    foamMask *= foamMask;
#if SWELL
    // A foam depth of 0 reaches the shader as 1 / 10000, and turns crest foam off with the rest.
    foamMask = max(foamMask, saturate((swellHere / max(SwellShape.y, 0.001f) - 0.35f) * 2.5f) * step(WaterParams.z, 1000.0f));
#endif
    // The shadow receiver leaves ShadowParams.z unused, and W3DWater puts the foam's world to texcoord scale there.
    // Past about two texels a pixel, each doubling of the ground a pixel covers doubles the foam, so its web stays readable from any height.
    float2 worldDx = ddx(world.xy);
    float2 worldDy = ddy(world.xy);
#if SWELL
    // Halving the log of the squared length skips a square root, which the radial build has no slot for.
    float octave = max(0.5f * log2(max(dot(worldDx, worldDx), dot(worldDy, worldDy)) * ShadowParams.z * ShadowParams.z) + 7.0f, 0.0f);
#else
    float octave = max(log2(sqrt(max(dot(worldDx, worldDx), dot(worldDy, worldDy))) * ShadowParams.z) + 7.0f, 0.0f);
#endif
    float fineScale = ShadowParams.z * exp2(-floor(octave));
    float coarseScale = fineScale * 0.5f;

    // Both sizes drift alike, so the coarse one becomes the next fine one without a jump.
    float2 foamShift = slope * 0.04f + time * float2(0.011f, -0.007f);
    float4 foamMean = TextureMean(FoamTexture);
    float foam = saturate(HexSample(FoamTexture, cells, world.xy * fineScale + foamShift, worldDx * fineScale, worldDy * fineScale, foamMean).r);
    float foamCoarse = saturate(HexSample(FoamTexture, cells, world.xy * coarseScale + foamShift, worldDx * coarseScale, worldDy * coarseScale, foamMean).r);
    foam = lerp(foam, foamCoarse, frac(octave));
    foam *= foamMask * Planar.w;

    // As the legacy soft water edge did, the surface fades out at the waterline instead of ending in a line.
    float edge = saturate(depth * Surface.x);
    reflection *= edge;
    glint *= edge;
    // Foam gathers at the waterline, so it fades in much sooner, as 1 - (1 - edge)^4.
    float dry = 1.0f - edge;
    dry *= dry;
    foam *= 1.0f - dry * dry;

    // The scene copy is already shrouded, so the shroud only darkens the water's own light.
    float3 shroud = tex2D(ShroudTexture, world.xy * ShroudMapping.xy + ShroudMapping.zw).rgb;
    float3 under = scene * (1.0f - opacity) + body * opacity * shroud;
    float3 color = lerp(under, sky * shroud, reflection);
    color += (SunColor.rgb * glint + input.Diffuse.rgb * shade * foam) * shroud;

#if RIVER
    float coverage = water.a * tex2D(EdgeTexture, input.EdgeUV).a;
    color = lerp(straight, color, coverage);
#endif
    return float4(color, 1.0f);
}
