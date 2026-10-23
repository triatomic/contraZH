// One level of a texture's slope map for the derived bumps, read from the texture's next mip down so fine noise does not sparkle.

sampler2D Source : register(s0);

float4 Texel  : register(c0);   // xy = one texel of the level drawn, in uv; zw = the level's size over the top level's
float4 Encode : register(c1);   // x = how hard large jumps soften, y = the scale onto the stored range, z = its centre

float Height(float2 uv)
{
    return dot(tex2D(Source, uv).rgb, float3(0.3f, 0.59f, 0.11f));
}

float4 main(float2 uv : TEXCOORD0) : COLOR
{
    float2 change = float2(
        Height(uv + float2(Texel.x, 0.0f)) - Height(uv - float2(Texel.x, 0.0f)),
        Height(uv + float2(0.0f, Texel.y)) - Height(uv - float2(0.0f, Texel.y))) * 0.5f;

    // Paint lines and team colour edges jump far more than surface grain, so large jumps are softened.
    change /= 1.0f + abs(change) * Encode.x;

    // Red holds the change along u and green along v, per texel of the top level.
    return float4(change * Texel.zw * Encode.y + Encode.z, 0.0f, 1.0f);
}
