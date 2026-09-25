// Draws a particle sprite fading out where it nears the surface behind it, so smoke and fire
// meet the ground and buildings without a hard line.
//
// The sprite keeps its fixed-function colour, texture times vertex colour. Its vertices are
// already in camera space, and stage 1 hands that position over as TEXCOORD1.
//
// SOFT turns the fade on. DEPTH picks the surface. 1 reads the scene's INTZ depth from stage 1,
// which covers everything drawn so far. 0 reads the terrain's height texture from stage 1, which
// covers only the ground.
//
// FLAME shades the sprite as fire. A noise field on stage 2, fixed in the world and rising with
// time, warps the texture lookup, breaks up dim edges and makes the brightness flicker. Bright
// texels run to white at their own brightness and dim ones deepen toward their hue, so blue and
// green flames keep their colour.
//
// ELECTRIC shades a sprite or beam as electricity, after RA3's tesla shader. A noise field in the view
// plane jumps to a new place many times a second. It jitters the texture lookup, draws thin arcs
// along its middle contour and strobes the brightness. Arcs take the sprite's hue halfway to white.
//
// LASER shades a beam or streak as a laser. Stage 2 hands over the beam coordinates as TEXCOORD2,
// x from 0 to 1 across the beam and y along it in world units. A thin core in the beam's own hue,
// taken most of the way to white, runs along the axis and wavers in width. Noise slid along the
// beam sends pulses of brightness towards its far end, and the edges melt into glow.

#ifndef SOFT
#define SOFT 1
#endif
#ifndef FLAME
#define FLAME 0
#endif
#ifndef ELECTRIC
#define ELECTRIC 0
#endif
#ifndef LASER
#define LASER 0
#endif

sampler2D ParticleTexture : register(s0);
sampler2D SurfaceTexture  : register(s1);
sampler2D NoiseTexture    : register(s2);

#if SOFT && DEPTH
float4 ClipX       : register(c0);   // camera space to clip x, y and w, one row each
float4 ClipY       : register(c1);
float4 ClipW       : register(c2);
float4 ScreenMap   : register(c3);   // xy scale and zw offset from clip space to depth texture
float4 Linearize   : register(c4);   // projection terms that turn stored depth back into camera z
#elif SOFT
float4 WorldX      : register(c0);   // camera space to world x, y and z, one row each
float4 WorldY      : register(c1);
float4 WorldZ      : register(c2);
float4 HeightMap   : register(c3);   // xy scale and zw offset from world xy to height texture
float4 HeightDecode : register(c4);  // weights of the high and low height bytes
#endif
float4 Params      : register(c5);   // x = 1 / fade distance, y = 1 to fade colour too, z = 1 to fade towards white, w = sign of depth along the view
#if FLAME
float4 Flame       : register(c6);   // x = noise rise so far, y = texture warp, z = world to noise scale, w = heat gain
float4 FlameWorldX : register(c7);   // camera space to world x, y and z, one row each
float4 FlameWorldY : register(c8);
float4 FlameWorldZ : register(c9);
float4 FlameShape  : register(c10);  // x = flicker swing, y = fringe breakup, z = the default camera's distance to the ground it looks at
#elif ELECTRIC
float4 Electric       : register(c6);   // xy = this jump's noise offset, z = camera space to noise scale, w = texture jitter
float4 ElectricShape  : register(c7);   // x = strobe swing, y = arc sharpness, z = half the arc brightness, w = depth where arcs start to widen
float4 ElectricClamp  : register(c8);   // xy = lowest and zw = highest jittered uv, open along a beam whose texture tiles
float4 ElectricStrobe : register(c9);   // x = 1 - half the strobe swing
#elif LASER
float4 Laser      : register(c6);   // x = pulse travel so far, y = world to noise scale, z = -3 / core width squared, w = core brightness
float4 LaserShape : register(c7);   // x = core waver, y = pulse swing
#endif

struct PsIn
{
    float4 Diffuse   : COLOR0;
    float2 TexCoord  : TEXCOORD0;
    float3 Position  : TEXCOORD1;
#if LASER
    float2 Beam      : TEXCOORD2;
#endif
};

float4 main(PsIn input) : COLOR
{
    float4 position = float4(input.Position, 1.0f);

#if FLAME
    float3 flameWorld = float3(dot(position, FlameWorldX), dot(position, FlameWorldY), dot(position, FlameWorldZ));

    // Two upright slices through the field, so the pattern changes whichever way the camera faces.
    float4 noiseA = tex2D(NoiseTexture, float2(flameWorld.x, flameWorld.z) * Flame.z - float2(0.0f, Flame.x));
    float4 noiseB = tex2D(NoiseTexture, float2(flameWorld.y, flameWorld.z) * (Flame.z * 1.7f) - float2(0.0f, Flame.x * 1.3f));

    float2 uv = saturate(input.TexCoord + (noiseA.rg + noiseB.gr - 1.0f) * Flame.y);
    float4 texel = tex2D(ParticleTexture, uv);
    float4 color = texel * input.Diffuse;

    // Beyond the default camera's distance a flame is small on screen, so the breakup and deepening that
    // shape it up close ease off with depth, and it keeps the body and brightness that make it readable.
    float nearness = min(1.0f, FlameShape.z / max(input.Position.z * Params.w, 1.0f));

    // Adding ignores alpha, and additive systems often leave it at zero.
    float coverage = lerp(texel.a, 1.0f, Params.y);

    // Only the texture's faintest fringe breaks up, so particles dimming with age keep their size.
    float shape = max(texel.r, max(texel.g, texel.b)) * coverage;
    float keep = saturate(1.0f + (shape * 4.0f - noiseA.b - noiseB.b) * FlameShape.y * nearness);

    float peak = max(color.r, max(color.g, color.b));
    float heat = saturate((peak * lerp(input.Diffuse.a, 1.0f, Params.y) * coverage - 0.5f) * Flame.w);

    // Halfway to the squared colour, so dim parts redden without losing much light.
    float3 deep = lerp(color.rgb, color.rgb * (color.rgb + peak) / max(2.0f * peak, 0.001f), nearness);
    // Alpha blending breaks up through alpha alone, so its fringe does not darken twice.
    color.rgb = lerp(deep, peak.xxx, heat * heat) * (lerp(1.0f, keep, Params.y) * (1.0f + FlameShape.x * (noiseB.b - 0.5f)));
    color.a *= keep;
#elif ELECTRIC
    // The field lies in the view plane, so arcs keep their shape on ground-aligned sprites the pitched camera sees at a slant.
    float2 field = input.Position.xy * Electric.z + Electric.xy;
    float4 noiseA = tex2D(NoiseTexture, field);
    float4 noiseB = tex2D(NoiseTexture, field * 2.0f + Electric.yx);

    float2 uv = clamp(input.TexCoord + (noiseA.rg - 0.5f) * Electric.w, ElectricClamp.xy, ElectricClamp.zw);
    float4 texel = tex2D(ParticleTexture, uv);
    float4 color = texel * input.Diffuse;

    // Adding ignores alpha, and additive systems often leave it at zero.
    float coverage = lerp(texel.a, 1.0f, Params.y);

    // Beyond the default camera's distance arcs widen with depth, so they stay visible at the top of the screen and zoomed out.
    float depth = max(input.Position.z * Params.w, 1.0f);
    float sharpness = ElectricShape.y * min(1.0f, ElectricShape.w / depth);
    float2 arcs = saturate(1.0f - abs(float2(noiseA.b, noiseB.g) - 0.5f) * sharpness);
    arcs *= arcs;
    float arc = max(arcs.x, arcs.y);

    // The square root carries arcs out into the faint fringe, and they still fade with the particle.
    float peak = max(color.r, max(color.g, color.b));
    float3 hue = color.rgb / (peak + 0.001f);
    float reach = sqrt(peak * coverage);
    float strobe = ElectricShape.x * noiseB.b + ElectricStrobe.x;
    color.rgb = color.rgb * strobe + (hue + 1.0f) * (arc * reach * ElectricShape.z);
#elif LASER
    float side = input.Beam.x * 2.0f - 1.0f;
    float along = input.Beam.y * Laser.y;
    float4 noiseA = tex2D(NoiseTexture, float2(along - Laser.x, 0.25f));
    float4 noiseB = tex2D(NoiseTexture, float2(along * 2.3f - Laser.x * 1.5f, 0.75f));

    float4 texel = tex2D(ParticleTexture, input.TexCoord);
    float4 color = texel * input.Diffuse;

    // Adding ignores alpha, and additive systems often leave it at zero.
    float coverage = lerp(texel.a, 1.0f, Params.y);

    // Two fields at different speeds, so pulses swell and fade as they travel.
    float pulse = 1.0f + LaserShape.y * (noiseA.r + noiseB.g - 1.0f);
    float core = exp2(side * side * Laser.z * (1.0f + LaserShape.x * (noiseB.b - 0.5f)));

    // Most of the way to white at the beam's own brightness, so the core fades out with the beam.
    float peak = max(color.r, max(color.g, color.b));
    float3 hot = color.rgb * 0.35f + peak * 0.65f;
    float edge = saturate(4.0f - 4.0f * abs(side));
    color.rgb = (color.rgb * edge + hot * (core * coverage * Laser.w)) * pulse;
    color.a *= edge;
#else
    float4 color = tex2D(ParticleTexture, input.TexCoord) * input.Diffuse;
#endif

#if SOFT
#if DEPTH
    float w = dot(position, ClipW);
    float2 ndc = float2(dot(position, ClipX), dot(position, ClipY)) / w;
    float stored = tex2D(SurfaceTexture, ndc * ScreenMap.xy + ScreenMap.zw).r;

    // Depth along the view for both, with the sign that makes further away larger.
    float sceneZ = (Linearize.x - stored * Linearize.y) / (stored * Linearize.z - Linearize.w);
    float gap = (sceneZ - input.Position.z) * Params.w;
#else
    float3 world = float3(dot(position, WorldX), dot(position, WorldY), dot(position, WorldZ));
    float2 heightBytes = tex2D(SurfaceTexture, world.xy * HeightMap.xy + HeightMap.zw).rg;
    float gap = world.z - dot(heightBytes, HeightDecode.xy);
#endif

    float fade = saturate(gap * Params.x);
    color.a *= fade;
    color.rgb *= lerp(1.0f, fade, Params.y);
    color.rgb = lerp(color.rgb, lerp(float3(1.0f, 1.0f, 1.0f), color.rgb, fade), Params.z);
#endif
    return color;
}
