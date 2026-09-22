// Shadow map lookup shared by every receiving pixel shader.
//
// The including shader declares ShadowMap on its shadow stage and defines PACKED.
// W3DShadowMap::bindReceiver fills c0 and the shadow stage's texcoord.

// x = shadow map texel size, y = depth bias, z = shadow strength, w = unused
float4 ShadowParams : register(c0);

#if PACKED

// Four point-sampled compares, weighted by where the pixel sits inside its texel.
// That weighting, not the tap count, is what stops shadow edges stair-stepping,
// because a box of point compares snaps every tap to the texel grid.
float SampleShadow(float2 uv, float depth)
{
    const float4 unshift = float4(1.0f, 1.0f / 255.0f, 1.0f / (255.0f * 255.0f),
                                  1.0f / (255.0f * 255.0f * 255.0f));
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

// The sampler does the compare on a depth texture, and its bilinear filter gives
// 2x2 PCF for one instruction.
float SampleShadow(float2 uv, float depth)
{
    return tex2Dproj(ShadowMap, float4(uv, depth, 1.0f)).r;
}

#endif

// The factor to scale colour by, 1 where the pixel is lit.
float ShadowFactor(float4 shadowPos)
{
    // Guarded because a zero w turns every later op into NaN, which draws black.
    float w = max(abs(shadowPos.w), 1e-6f);
    float2 uv = shadowPos.xy / w;
    float depth = shadowPos.z / w - ShadowParams.y;

    // Outside the sun's box nothing is shadowed. Applied as a mask rather than an
    // early out so the texture fetches stay in uniform flow.
    float inside = (uv.x >= 0.0f && uv.x <= 1.0f && uv.y >= 0.0f && uv.y <= 1.0f) ? 1.0f : 0.0f;
    float lit = lerp(1.0f, SampleShadow(uv, depth), inside);

    return lerp(1.0f, lit, ShadowParams.z);
}
