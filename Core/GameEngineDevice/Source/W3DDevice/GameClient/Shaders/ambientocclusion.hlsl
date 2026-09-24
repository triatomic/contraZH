// Screen-space ambient occlusion from the scene's INTZ depth, drawn over the opaque scene before
// water, decals and particles.
//
// Without BLUR, estimates how much of the hemisphere above each pixel nearby geometry hides. With
// BLUR, smooths that estimate along one axis, keeping it from bleeding across depth edges.
// TEXCOORD0 is the pixel's texel centre in the scene-sized targets.

sampler2D SceneDepth : register(s0);

#if BLUR

sampler2D Occlusion : register(s1);

float4 Linearize : register(c0);   // projection terms that turn stored depth back into camera z
float4 Step      : register(c1);   // xy = one texel along the blur axis, z = how sharply depth edges stop it

static const int BLUR_TAPS = 4;
static const float BlurWeight[BLUR_TAPS + 1] = { 1.0f, 0.9f, 0.65f, 0.38f, 0.17f };

float SceneZ(float2 uv)
{
    float stored = tex2D(SceneDepth, uv).r;
    return abs((Linearize.x - stored * Linearize.y) / (stored * Linearize.z - Linearize.w));
}

float4 main(float2 uv : TEXCOORD0) : COLOR
{
    float centerZ = SceneZ(uv);
    float edge = Step.z / centerZ;

    float sum = tex2D(Occlusion, uv).r;
    float weights = 1.0f;
    for (int i = 1; i <= BLUR_TAPS; i++)
    {
        for (int side = -1; side <= 1; side += 2)
        {
            float2 tapUV = uv + Step.xy * (i * side);
            float weight = BlurWeight[i] * saturate(1.0f - abs(SceneZ(tapUV) - centerZ) * edge);
            sum += tex2D(Occlusion, tapUV).r * weight;
            weights += weight;
        }
    }

    float ao = sum / weights;
    return float4(ao, ao, ao, 1.0f);
}

#else

float4 ToView0   : register(c0);   // texel uv and stored depth to homogeneous camera space, one row each
float4 ToView1   : register(c1);
float4 ToView2   : register(c2);
float4 ToView3   : register(c3);
float4 Radius    : register(c4);   // xy = radius in uv at clip w 1, z and w = clip w from camera z
float4 Params    : register(c5);   // x = 1 / radius squared, y = bias, z = strength over sample count, w = largest uv radius
float4 Target    : register(c6);   // xy = target size in pixels
float4 Spiral[6] : register(c7);   // twelve disc offsets, two per register

float3 ViewPosition(float2 uv)
{
    float stored = tex2D(SceneDepth, uv).r;
    float4 position = uv.x * ToView0 + uv.y * ToView1 + stored * ToView2 + ToView3;
    return position.xyz / position.w;
}

float Occluded(float2 uv, float3 center, float3 normal)
{
    float3 v = ViewPosition(uv) - center;
    float vv = dot(v, v);
    float facing = dot(v, normal) * rsqrt(vv + 1e-4f);
    return max(facing - Params.y, 0.0f) * saturate(1.0f - vv * Params.x);
}

float4 main(float2 uv : TEXCOORD0) : COLOR
{
    float stored = tex2D(SceneDepth, uv).r;
    float4 homogeneous = uv.x * ToView0 + uv.y * ToView1 + stored * ToView2 + ToView3;
    float3 center = homogeneous.xyz / homogeneous.w;

    // The camera sits at the origin, so a normal facing it points against the pixel's position.
    float3 normal = normalize(cross(ddy(center), ddx(center)));
    normal = (dot(normal, center) > 0.0f) ? -normal : normal;

    float clipW = center.z * Radius.z + Radius.w;
    float2 radius = Radius.xy / clipW;
    radius *= min(1.0f, Params.w / max(radius.x, radius.y));

    // Interleaved gradient noise turns the disc per pixel; the blur averages the pattern away.
    float2 pixel = floor(uv * Target.xy);
    float angle = 6.2831853f * frac(52.9829189f * frac(dot(pixel, float2(0.06711056f, 0.00583715f))));
    float2 turn;
    sincos(angle, turn.y, turn.x);

    float sum = 0.0f;
    for (int i = 0; i < 6; i++)
    {
        float4 pair = Spiral[i];
        float2 a = float2(pair.x * turn.x - pair.y * turn.y, pair.x * turn.y + pair.y * turn.x);
        float2 b = float2(pair.z * turn.x - pair.w * turn.y, pair.z * turn.y + pair.w * turn.x);
        sum += Occluded(uv + a * radius, center, normal);
        sum += Occluded(uv + b * radius, center, normal);
    }

    // Cleared depth is empty space, which nothing occludes.
    float ao = (stored < 1.0f) ? saturate(1.0f - sum * Params.z) : 1.0f;
    return float4(ao, ao, ao, 1.0f);
}

#endif
