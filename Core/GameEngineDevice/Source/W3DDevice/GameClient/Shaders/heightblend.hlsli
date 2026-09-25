// Blends two terrain textures by their heights instead of by the vertex alpha alone.
//
// The legacy blend runs the vertex alpha straight across a cell. Here the heights from
// TerrainHeightTextureClass push that ramp towards the taller texture, so the other shows
// only in its hollows, and the sharpness narrows what is left of the ramp. The push is
// greatest mid-ramp and nothing at its ends, so cells without a blend keep one texture.
// A strength of 0 and a sharpness of 1 give back the legacy blend exactly.
//
// The including shader defines HEIGHT_BLEND_REGISTER. The constants carry the 0.5 too, since the
// heaviest terrain shaders have no register left for another literal.

sampler2D HeightAtlas : register(s10);
float4 HeightBlend    : register(HEIGHT_BLEND_REGISTER);   // x = 4 times the strength, y = sharpness, z = 0.5

// How much of the second texture shows.
float HeightBlendWeight(float alpha, float firstHeight, float secondHeight)
{
    float lean = (secondHeight - firstHeight) * HeightBlend.x * (alpha - alpha * alpha);
    return saturate((alpha - HeightBlend.z + lean) * HeightBlend.y + HeightBlend.z);
}
