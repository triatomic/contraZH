// Packed depth for the shadow map, for devices that cannot sample a depth surface.
//
// Vertex processing is fixed function, so the device applies each caster's world
// transform. Texture stage 0 generates the sun view-space position, and the sun
// projection's z row maps that to depth. The hardware depth path needs no shader.

float2 DepthRow : register(c0);   // (scale, offset) of the sun projection's z row

// Each channel is split on 255 rather than 256. An 8-bit channel stores k/255, so a
// 256-based split leaves the coarse channel unable to represent its value exactly and
// loses roughly 1/510 of the range, which is several times any sane depth bias.
float4 mainPackedPS(float3 viewPosition : TEXCOORD0) : COLOR
{
    float depth = viewPosition.z * DepthRow.x + DepthRow.y;

    const float4 shift = float4(1.0f, 255.0f, 255.0f * 255.0f, 255.0f * 255.0f * 255.0f);
    const float4 mask  = float4(1.0f / 255.0f, 1.0f / 255.0f, 1.0f / 255.0f, 0.0f);

    float4 packed = frac(depth * shift);
    packed -= packed.yzww * mask;
    return packed;
}
