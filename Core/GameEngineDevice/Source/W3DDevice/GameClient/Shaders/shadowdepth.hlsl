// Packed depth for the shadow map, for devices that cannot sample a depth surface.
//
// Casters draw with their own texture on stage 0 and fixed-function vertex processing,
// so the device applies each caster's world transform. Stage 1 generates the sun
// view-space position, and the sun projection's z row maps that to depth. The hardware
// depth path needs no shader.

float2 DepthRow : register(c0);   // (scale, offset) of the sun projection's z row
float2 Cutout   : register(c1);   // x = alpha cutoff, negative for none; y = 1 for inverted alpha

sampler2D CasterTexture : register(s0);

struct PsIn
{
    float4 Diffuse      : COLOR0;
    float2 TexCoord     : TEXCOORD0;
    float3 ViewPosition : TEXCOORD1;
};

// Each channel is split on 255 rather than 256. An 8-bit channel stores k/255, so a
// 256-based split leaves the coarse channel unable to represent its value exactly and
// loses roughly 1/510 of the range, which is several times any sane depth bias.
float4 mainPackedPS(PsIn input) : COLOR
{
    // Cut where the caster's own shader would, so a leaf card casts a leaf.
    float alpha = tex2D(CasterTexture, input.TexCoord).a * input.Diffuse.a;
    alpha = lerp(alpha, 1.0f - alpha, Cutout.y);
    clip(Cutout.x < 0.0f ? 1.0f : alpha - Cutout.x);

    float depth = input.ViewPosition.z * DepthRow.x + DepthRow.y;

    const float4 shift = float4(1.0f, 255.0f, 255.0f * 255.0f, 255.0f * 255.0f * 255.0f);
    const float4 mask  = float4(1.0f / 255.0f, 1.0f / 255.0f, 1.0f / 255.0f, 0.0f);

    float4 packed = frac(depth * shift);
    packed -= packed.yzww * mask;
    return packed;
}
