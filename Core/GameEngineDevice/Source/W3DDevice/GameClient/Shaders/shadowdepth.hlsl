// Depth-only pass that fills the shadow map.
//
// On the hardware depth path nothing needs writing: the depth buffer is the shadow
// map and the pixel shader only exists to honour alpha cutout. On the packed path
// the same depth is encoded into RGBA8 because the depth surface cannot be sampled.

float4x4 SunViewProj    : register(c0);
float    AlphaTestCutoff : register(c4);

sampler2D DiffuseMap : register(s0);

struct VsIn
{
    float4 Position : POSITION;
    float2 TexCoord : TEXCOORD0;
};

struct VsOut
{
    float4 Position : POSITION;
    float2 TexCoord : TEXCOORD0;
    float2 Depth    : TEXCOORD1;   // (z, w) kept off the interpolated position
};

VsOut mainVS(VsIn input)
{
    VsOut output;
    output.Position = mul(input.Position, SunViewProj);
    output.TexCoord = input.TexCoord;
    output.Depth    = output.Position.zw;
    return output;
}

// Hardware depth path: the only job is to discard cutout texels, so a tree
// billboard casts its canopy rather than its whole rectangle.
float4 mainPS(VsOut input) : COLOR
{
    clip(tex2D(DiffuseMap, input.TexCoord).a - AlphaTestCutoff);
    return float4(0, 0, 0, 0);
}

// Packed path: the same depth encoded into RGBA8.
//
// Each channel is split on 255 rather than 256. An 8-bit channel stores k/255, so a
// 256-based split leaves the coarse channel unable to represent its value exactly and
// loses roughly 1/510 of the range, which is several times any sane depth bias.
float4 mainPackedPS(VsOut input) : COLOR
{
    clip(tex2D(DiffuseMap, input.TexCoord).a - AlphaTestCutoff);

    float depth = input.Depth.x / input.Depth.y;

    const float4 shift = float4(1.0f, 255.0f, 255.0f * 255.0f, 255.0f * 255.0f * 255.0f);
    const float4 mask  = float4(1.0f / 255.0f, 1.0f / 255.0f, 1.0f / 255.0f, 0.0f);

    float4 packed = frac(depth * shift);
    packed -= packed.yzww * mask;
    return packed;
}
