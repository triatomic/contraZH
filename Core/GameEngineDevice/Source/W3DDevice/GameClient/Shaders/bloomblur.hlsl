// One bloom pass, shrink or blur, as the weighted sum of five taps of the source.
//
// Summing here instead of blending one quad per tap keeps the running total in float, so
// no tap is rounded to the target's 8 bits. Each tap is (u offset, v offset, weight, unused);
// a pass with fewer taps gives the rest zero weight.

sampler2D Source : register(s0);
float4 Taps[5] : register(c0);

float4 main(float2 uv : TEXCOORD0) : COLOR
{
    float4 sum = 0.0f;
    for (int i = 0; i < 5; ++i)
    {
        sum += tex2D(Source, uv + Taps[i].xy) * Taps[i].z;
    }
    return sum;
}
