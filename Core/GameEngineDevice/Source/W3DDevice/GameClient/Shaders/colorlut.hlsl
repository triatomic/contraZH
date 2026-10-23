// Grades the finished 3D scene. In order: tint, brightness, saturation, contrast, vibrance and a
// Technicolor look; a lookup through a colour table; levels; then a vignette, film grain and dither.
//
// The table on stage 1 is a strip of N slices, each N by N texels. Red runs across a slice, green
// down it and blue from slice to slice, so a colour reads the two slices around its blue and blends
// them. LUT=0 builds the grade without a table.
//
// Vibrance and Technicolor follow SweetFX's Vibrance.fx and Technicolor2.fx, and the table's hue and
// brightness shares follow ReShade's LUT.fx.

#ifndef LUT
#define LUT 1
#endif

sampler2D Scene : register(s0);
sampler2D Table : register(s1);

float4 Grade     : register(c0);   // x = brightness, y = contrast, z = saturation, w = how much of the table's result is taken
float4 Tint      : register(c1);   // rgb multiplies the scene; w = vibrance
float4 TableSize : register(c2);   // x = N - 1, y = 1 / N, z = 1 / (N * N); w = Technicolor strength
float4 TableMix  : register(c3);   // x = share of the table's hue, y = share of its brightness; z = grain, w = dither as a colour step
float4 LevelsIn  : register(c4);   // x = black point, y = 1 / (white point - black point), z = 1 / gamma
float4 LevelsOut : register(c5);   // x = output black, y = output white - output black; z = vignette strength, w = 1 / its radius squared
float4 ViewMap   : register(c6);   // xy scale and zw offset from scene uv to the view, 0 at its centre and 1 at its top and bottom
float4 Noise     : register(c7);   // two shifts of the noise, new every frame

// How strongly the Technicolor dyes pull each channel towards the other two.
static const float TechnicolorDye = 0.2f;

float Hash(float2 p)
{
    return frac(sin(dot(p, float2(12.9898f, 78.233f))) * 43758.5453f);
}

float4 main(float2 uv : TEXCOORD0) : COLOR
{
    float4 scene = tex2D(Scene, uv);

    float3 colour = scene.rgb * Tint.rgb * Grade.x;
    float grey = dot(colour, float3(0.299f, 0.587f, 0.114f));
    colour = lerp(grey.xxx, colour, Grade.z);
    colour = saturate((colour - 0.5f) * Grade.y + 0.5f);

    // Vibrance saturates dull colours more than vivid ones.
    float spread = max(colour.r, max(colour.g, colour.b)) - min(colour.r, min(colour.g, colour.b));
    grey = dot(colour, float3(0.212656f, 0.715158f, 0.072186f));
    colour = saturate(lerp(grey.xxx, colour, 1.0f + Tint.w * (1.0f - sign(Tint.w) * spread)));

    float3 inverse = 1.0f - colour;
    float3 film = colour * inverse.grg * inverse.bbr;
    float3 dye = film * TechnicolorDye;
    colour = saturate(colour + (film - dye.grg - dye.bbr) * TableSize.w);

#if LUT
    // Texel centres hold the table's entries, so each axis spans from half a texel in to half a texel short.
    float3 cell = colour * TableSize.x;
    float slice = floor(cell.b);
    float nextSlice = min(slice + 1.0f, TableSize.x);
    float2 inSlice = float2((cell.r + 0.5f) * TableSize.z, (cell.g + 0.5f) * TableSize.y);

    float3 low = tex2D(Table, float2(inSlice.x + slice * TableSize.y, inSlice.y)).rgb;
    float3 high = tex2D(Table, float2(inSlice.x + nextSlice * TableSize.y, inSlice.y)).rgb;
    float3 looked = lerp(low, high, cell.b - slice);

    // A colour's direction is its hue and its length its brightness, and the table gives a share of each.
    float size = sqrt(max(dot(colour, colour), 0.000001f));
    float lookedSize = sqrt(max(dot(looked, looked), 0.000001f));
    float3 hue = lerp(colour / size, looked / lookedSize, TableMix.x);
    hue *= rsqrt(max(dot(hue, hue), 0.000001f));
    colour = lerp(colour, hue * lerp(size, lookedSize, TableMix.y), Grade.w);
#endif

    colour = saturate((colour - LevelsIn.x) * LevelsIn.y);
    // Written as x times x to the power less one, which keeps black at black whatever the gamma.
    colour = colour * pow(max(colour, 0.00001f), LevelsIn.z - 1.0f) * LevelsOut.y + LevelsOut.x;

    float2 fromCentre = uv * ViewMap.xy + ViewMap.zw;
    colour *= 1.0f - LevelsOut.z * saturate(dot(fromCentre, fromCentre) * LevelsOut.w);

    colour *= 1.0f + (Hash(uv + Noise.xy) - 0.5f) * TableMix.z;

    // Two noises sum to a triangle-shaped spread, which leaves no pattern in a smooth gradient.
    colour += (Hash(uv + Noise.zw) + Hash(uv - Noise.xy) - 1.0f) * TableMix.w;

    return float4(colour, scene.a);
}
