// Summing in float keeps each tap off the target's 8 bits; a tap is (u offset, v offset, weight, unused)
// SHRINK=1 builds the 4 tap shrink pass, which thresholds each tap before the average dilutes thin sparks
// Knee is (threshold, knee width, 1 / (4 * knee width), 2 * knee width); a zero threshold passes everything

sampler2D Source : register(s0);
float4 Taps[5] : register(c0);
float4 Knee : register(c5);

#if SHRINK

float4 main(float2 uv : TEXCOORD0) : COLOR
{
    float4 color[4];
    float4 brightness;
    for (int i = 0; i < 4; ++i)
    {
        color[i] = tex2D(Source, uv + Taps[i].xy);
        brightness[i] = max(color[i].r, max(color[i].g, color[i].b));
    }

    // a quadratic ramp around the threshold, so the glow fades in instead of cutting on; one lane per tap
    float4 above = brightness - Knee.x;
    float4 soft = clamp(above + Knee.y, 0.0f, Knee.w);
    soft = soft * soft * Knee.z;
    float4 scale = max(soft, above) / max(brightness, 0.0001f);

    // the shrink weighs its four taps equally
    float4 sum = 0.0f;
    for (int j = 0; j < 4; ++j)
    {
        sum += float4(color[j].rgb * scale[j], color[j].a);
    }
    return sum * 0.25f;
}

#else

float4 main(float2 uv : TEXCOORD0) : COLOR
{
    float4 sum = 0.0f;
    for (int i = 0; i < 5; ++i)
    {
        sum += tex2D(Source, uv + Taps[i].xy) * Taps[i].z;
    }
    return sum;
}

#endif
