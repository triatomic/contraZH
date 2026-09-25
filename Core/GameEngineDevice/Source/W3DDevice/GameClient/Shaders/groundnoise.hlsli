// The ground's light and colour from W3DGroundNoise's texture, which stands in for the legacy light map.
//
// Every channel of the texture holds its own tiling noise. Red shades the broadest patches and alpha
// tints them warm or cool, both read at the light map's texcoords. Green and blue shade finer ones,
// read after one and two turns, each shrinking the pattern about 2.24 times and turning it about 27
// degrees. The finer tilings fall across the broad one at angles, so only the broad tile, far wider
// than a view, ever repeats. W3DGroundNoise bakes the GameData settings into the channels, and the
// literals are ones the heaviest terrain shaders already hold, since they have no registers to spare.

float2 GroundNoiseTurn(float2 uv)
{
    return uv * 2.0f + float2(-uv.y, uv.x);
}

// What the ground's colour is multiplied by.
float3 GroundNoise(sampler2D noise, float2 uv)
{
    float4 broad = tex2D(noise, uv);
    float2 midUV = GroundNoiseTurn(uv);
    float shade = broad.r + tex2D(noise, midUV).g + tex2D(noise, GroundNoiseTurn(midUV)).b - 1.0f;
    float tint = broad.a - 0.5f;
    return shade + float3(tint, 0.0f, -tint);
}
