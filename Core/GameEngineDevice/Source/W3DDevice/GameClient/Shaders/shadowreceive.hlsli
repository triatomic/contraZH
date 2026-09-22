// Shadow map lookup shared by every receiving pixel shader.
//
// The including shader declares ShadowMap on its shadow stage and defines PACKED.
// W3DShadowMap::bindReceiver fills c0, c4 and the shadow stage's texcoord.

// x = shadow map texel size, y = depth bias, z = unused, w = filter tap spacing in texels
float4 ShadowParams : register(c0);

// The map's shadow colour, which multiplies fully shadowed pixels per channel.
float4 ShadowColor : register(c4);

#if PACKED

// Four point-sampled compares, weighted by where the pixel sits inside its texel.
// That weighting, not the tap count, is what stops shadow edges stair-stepping,
// because a box of point compares snaps every tap to the texel grid.
float SampleShadow(float2 uv, float depth)
{
    const float4 unshift = float4(1.0f, 1.0f / 255.0f, 1.0f / (255.0f * 255.0f),
                                  1.0f / (255.0f * 255.0f * 255.0f));

#if SHADOW_SINGLE_TAP
    // One tap, for shaders that need the slots and not a soft edge.
    return (depth <= dot(tex2D(ShadowMap, uv), unshift)) ? 1.0f : 0.0f;
#endif

    float texel = ShadowParams.x;
    float2 texelPos = uv / texel - 0.5f;
    float2 weight = frac(texelPos);
    float2 base = (texelPos - weight + 0.5f) * texel;

    float lit00 = (depth <= dot(tex2D(ShadowMap, base), unshift)) ? 1.0f : 0.0f;
    float lit10 = (depth <= dot(tex2D(ShadowMap, base + float2(texel, 0.0f)), unshift)) ? 1.0f : 0.0f;
    float lit01 = (depth <= dot(tex2D(ShadowMap, base + float2(0.0f, texel)), unshift)) ? 1.0f : 0.0f;
    float lit11 = (depth <= dot(tex2D(ShadowMap, base + float2(texel, texel)), unshift)) ? 1.0f : 0.0f;

    return lerp(lerp(lit00, lit10, weight.x), lerp(lit01, lit11, weight.x), weight.y);
}

#else

// The sampler does the compare on a depth texture and its bilinear filter blends each
// 2x2 result, so a 3x3 grid of those taps smooths the edge instead of stepping at every
// texel. The tap spacing sets how soft.
float SampleShadow(float2 uv, float depth)
{
#if SHADOW_SINGLE_TAP
    // One tap, for shaders that need the slots and not a soft edge.
    return tex2Dproj(ShadowMap, float4(uv, depth, 1.0f)).r;
#endif

    float texel = ShadowParams.x * ShadowParams.w;
    float lit = 0.0f;

    for (int y = -1; y <= 1; ++y)
    {
        for (int x = -1; x <= 1; ++x)
        {
            lit += tex2Dproj(ShadowMap, float4(uv + float2(x, y) * texel, depth, 1.0f)).r;
        }
    }
    return lit / 9.0f;
}

#endif

// How lit the pixel is, from 0 in full shadow to 1.
float ShadowLit(float4 shadowPos)
{
    // Guarded because a zero w turns every later op into NaN, which draws black.
    float w = max(abs(shadowPos.w), 1e-6f);
    float2 uv = shadowPos.xy / w;
    float depth = shadowPos.z / w - ShadowParams.y;

    // Outside the sun's box nothing is shadowed. Applied as a mask rather than an
    // early out so the texture fetches stay in uniform flow.
    float inside = (uv.x >= 0.0f && uv.x <= 1.0f && uv.y >= 0.0f && uv.y <= 1.0f) ? 1.0f : 0.0f;
    return lerp(1.0f, SampleShadow(uv, depth), inside);
}

// The factor to scale colour by, white where the pixel is lit and the map's shadow
// colour where it is not, so a tinted shadow colour tints the shadow.
float3 ShadowFactor(float4 shadowPos)
{
    return lerp(ShadowColor.rgb, float3(1.0f, 1.0f, 1.0f), ShadowLit(shadowPos));
}
