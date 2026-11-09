sampler2D Scene : register(s0);
sampler2D SceneDepth : register(s1);

float4 Linearize : register(c0);
float4 Fog : register(c1); // x = start, y = inverse range, z = strength
float4 FogColor : register(c2);

float4 main(float2 uv : TEXCOORD0) : COLOR
{
    float4 scene = tex2D(Scene, uv);
    float stored = tex2D(SceneDepth, uv).r;
    float distance = abs((Linearize.x - stored * Linearize.y) / (stored * Linearize.z - Linearize.w));
    float amount = saturate((distance - Fog.x) * Fog.y);
    amount = amount * amount * (3.0f - 2.0f * amount) * Fog.z;
    // Black shroud and cleared depth must stay untouched.
    amount *= saturate(max(scene.r, max(scene.g, scene.b)) * 32.0f) * (stored < 1.0f);
    return float4(lerp(scene.rgb, FogColor.rgb, amount), scene.a);
}
