// Bends the finished scene around an expanding ring on the ground, like the air a blast pushes out.
//
// The ring is drawn as a flat quad around its centre. TEXCOORD0 is the position on the quad, -1
// to 1 from the centre, and stage 1 hands over the clip-space position so the pixel knows where
// it sits in the scene copy on stage 0.

sampler2D Scene : register(s0);

float4 Ring      : register(c0);   // x = ring radius and y = 1 / half its width, on the quad's scale; z = push in scene uv
float4 Center    : register(c1);   // xy = the ring's centre in scene uv
float4 ScreenMap : register(c2);   // xy scale and zw offset from clip space to scene uv

struct PsIn
{
    float2 Local : TEXCOORD0;
    float4 Clip  : TEXCOORD1;
};

float4 main(PsIn input) : COLOR
{
    float2 uv = input.Clip.xy / input.Clip.w * ScreenMap.xy + ScreenMap.zw;

    // A single wave across the band, pulling in ahead of the ring and pushing out behind it.
    float across = (length(input.Local) - Ring.x) * Ring.y;
    float wave = sin(clamp(across, -1.0f, 1.0f) * 3.14159265f);

    float2 away = uv - Center.xy;
    float2 direction = away * rsqrt(max(dot(away, away), 1e-8f));
    return tex2D(Scene, uv - direction * wave * Ring.z);
}
