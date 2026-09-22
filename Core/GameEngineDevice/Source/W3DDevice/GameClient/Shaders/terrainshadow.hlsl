// Terrain base pass that also receives the sun's cast shadow.
//
// Matches the legacy terrain.nvp exactly -- blend two terrain textures by vertex
// alpha, then apply vertex lighting -- and scales only the lighting by the shadow
// factor, so a shadowed pixel keeps its ambient rather than going black.

sampler2D BaseTexture   : register(s0);
sampler2D BlendTexture  : register(s1);
sampler2D ShadowMap     : register(s2);

// x = shadow map texel size, y = depth bias, z = shadow strength, w = unused
float4 ShadowParams : register(c0);

struct PsIn
{
    float4 Diffuse   : COLOR0;
    float2 BaseUV    : TEXCOORD0;
    float2 BlendUV   : TEXCOORD1;
    float4 ShadowPos : TEXCOORD2;
};

// Four point-sampled compares, weighted by where the pixel sits inside its texel.
// That weighting, not the tap count, is what stops shadow edges stair-stepping:
// a box of point compares snaps every tap to the texel grid.
float SampleShadowPacked(float2 uv, float depth)
{
    const float4 unshift = float4(1.0f, 1.0f / 255.0f, 1.0f / (255.0f * 255.0f),
                                  1.0f / (255.0f * 255.0f * 255.0f));
    float texel = ShadowParams.x;
    float2 texelPos = uv / texel;
    float2 frac2 = frac(texelPos);
    float2 base = (texelPos - frac2) * texel;

    float lit = 0.0f;
    for (int y = 0; y < 2; ++y)
    {
        for (int x = 0; x < 2; ++x)
        {
            float2 tap = base + float2(x, y) * texel;
            float stored = dot(tex2D(ShadowMap, tap), unshift);
            float weight = ((x == 0) ? (1.0f - frac2.x) : frac2.x)
                         * ((y == 0) ? (1.0f - frac2.y) : frac2.y);
            lit += weight * ((depth <= stored) ? 1.0f : 0.0f);
        }
    }
    return lit;
}

float4 mainPacked(PsIn input) : COLOR
{
    float4 base  = tex2D(BaseTexture, input.BaseUV);
    float4 blend = tex2D(BlendTexture, input.BlendUV);
    float4 color = lerp(base, blend, input.Diffuse.a);

    // Guard the divide: a NaN here survives every later op and the pixel vanishes.
    float w = max(abs(input.ShadowPos.w), 1e-6f);
    float2 uv = input.ShadowPos.xy / w;
    float depth = (input.ShadowPos.z / w) - ShadowParams.y;

    float lit = SampleShadowPacked(uv, depth);

    // Outside the sun frustum nothing is shadowed. Folded in as a mask rather than
    // an early-out so the texture fetches stay in uniform flow.
    float inside = (uv.x >= 0.0f && uv.x <= 1.0f && uv.y >= 0.0f && uv.y <= 1.0f) ? 1.0f : 0.0f;
    lit = lerp(1.0f, lit, inside);

    float shadow = lerp(1.0f, lit, ShadowParams.z);
    return color * input.Diffuse * shadow;
}

// Hardware depth path: the sampler does the compare and the bilinear filter gives
// 2x2 PCF for one instruction, so no manual weighting is needed.
float4 main(PsIn input) : COLOR
{
    float4 base  = tex2D(BaseTexture, input.BaseUV);
    float4 blend = tex2D(BlendTexture, input.BlendUV);
    float4 color = lerp(base, blend, input.Diffuse.a);

    float4 shadowPos = input.ShadowPos;
    shadowPos.z -= ShadowParams.y * shadowPos.w;

    float lit = tex2Dproj(ShadowMap, shadowPos).r;

    float w = max(abs(input.ShadowPos.w), 1e-6f);
    float2 uv = input.ShadowPos.xy / w;
    float inside = (uv.x >= 0.0f && uv.x <= 1.0f && uv.y >= 0.0f && uv.y <= 1.0f) ? 1.0f : 0.0f;
    lit = lerp(1.0f, lit, inside);

    float shadow = lerp(1.0f, lit, ShadowParams.z);
    return color * input.Diffuse * shadow;
}
