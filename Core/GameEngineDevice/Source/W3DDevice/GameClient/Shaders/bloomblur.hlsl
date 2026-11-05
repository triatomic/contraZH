// Summing in float keeps each tap off the target's 8 bits; a tap is (u offset, v offset, weight, unused)
// Knee is (threshold, knee width, 1 / (4 * knee width), unused); a zero threshold passes everything

sampler2D Source : register(s0);
float4 Taps[5] : register(c0);
float4 Knee : register(c5);

float4 main(float2 uv : TEXCOORD0) : COLOR
{
    float4 sum = 0.0f;
    for (int i = 0; i < 5; ++i)
    {
        sum += tex2D(Source, uv + Taps[i].xy) * Taps[i].z;
    }

    // a quadratic ramp around the threshold, so the glow fades in instead of cutting on
    float brightness = max(sum.r, max(sum.g, sum.b));
    float soft = clamp(brightness - Knee.x + Knee.y, 0.0f, 2.0f * Knee.y);
    soft = soft * soft * Knee.z;
    sum.rgb *= max(soft, brightness - Knee.x) / max(brightness, 0.0001f);
    return sum;
}
