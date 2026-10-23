// Draws an object's collision footprint on the ground as a glowing hexagon. Two flat sides run along the
// footprint's length and two points close its ends, so a round footprint gives a regular hexagon.
//
// RING draws a circle in the same style instead, for a range.
//
// TEXCOORD0 spans the decal from 0 to 1 along the object's axes. COLOR0 carries the tint and opacity.

float4 Extent : register(c0);   // xy = decal size, z = 1 when the points lie along y, w = half width between the flat sides
float4 Shape  : register(c1);   // xy = outward normal of the slanted sides, z = distance to the points or the ring's radius, w = line half width
float4 Motion : register(c2);   // xy = sweep direction, z = 1 / inner glow reach, w = 1 / outer glow reach
float4 Glow   : register(c3);   // x = glow strength, y = half length of each of the two cuts

struct PsIn
{
    float4 Tint : COLOR0;
    float2 UV   : TEXCOORD0;
};

float4 main(PsIn input) : COLOR
{
    float2 position = (input.UV - 0.5f) * Extent.xy;

    // The signed distance to the outline, negative inside it, and the distance along it from the nearest cut.
#if defined(RING)
    float edge = length(position) - Shape.z;
    float fromCut = abs(position.x);
#else
    // One quadrant stands for all four. Only the flat sides are cut.
    float2 quadrant = abs(lerp(position.xy, position.yx, Extent.z));
    float flatSide = quadrant.y - Extent.w;
    float slantSide = dot(quadrant - float2(Shape.z, 0.0f), Shape.xy);
    float edge = max(flatSide, slantSide);
    float fromCut = (flatSide > slantSide) ? quadrant.x : Shape.z;
#endif

    // The line keeps a pixel and a half of width when the camera pulls back.
    float pixel = max(fwidth(edge), 0.0001f);
    float halfWidth = max(Shape.w, pixel * 0.75f);
    float outline = saturate((halfWidth - abs(edge)) / pixel + 0.5f);

    // Two cuts open the outline around the middle of its sides, which leaves it as two brackets.
    float linked = saturate((fromCut - Glow.y) / pixel + 0.5f);
    outline *= linked;

    // The glow near the outline opens with it, and deeper inside it stays whole.
    float glow = (edge < 0.0f) ? exp(edge * Motion.z) : exp(-edge * Motion.w);
    glow *= max(linked, saturate(-edge * Motion.z));

    // Two highlights travel around the outline, opposite each other.
    float along = dot(normalize(position + 0.0001f), Motion.xy);
    float hot = pow(abs(along), 12.0f);

    float alpha = saturate(outline * (0.65f + 0.35f * hot) + glow * Glow.x * (1.0f + hot));
    float3 color = lerp(input.Tint.rgb, 1.0f, outline * hot * 0.6f);
    return float4(color, alpha * input.Tint.a);
}
