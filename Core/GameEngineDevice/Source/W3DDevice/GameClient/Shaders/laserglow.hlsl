// Lights the ground near a laser beam, as a light shaped like the beam itself. It draws on a mesh that
// follows the terrain and blends as dest * (1 + light), which lights the ground's own colour a second
// time. The light arrives divided by the scene's light, so a dim night lights up as much as a bright day.
//
// COLOR0 carries the terrain normal's x and y, TEXCOORD0 the world x and y, and TEXCOORD1.x the world z.

sampler2D NoiseTexture : register(s0);

float4 BeamStart : register(c0);   // xyz = beam start, w = its along coordinate in noise tiles
float4 BeamSpan  : register(c1);   // xyz = start to end, w = 1 / length squared
float4 Glow      : register(c2);   // rgb = light over the scene's light, w = 1 / reach
float4 Pulse     : register(c3);   // x = pulse travel so far, y = beam length in noise tiles, z = pulse swing
float4 Shape     : register(c4);   // x = falloff power, y = light on ground facing away, from 0 to 1

struct PsIn
{
    float4 Normal  : COLOR0;
    float2 WorldXY : TEXCOORD0;
    float2 WorldZ  : TEXCOORD1;
};

float4 main(PsIn input) : COLOR
{
    float3 world = float3(input.WorldXY, input.WorldZ.x);

    // The nearest point on the beam lights this spot, so a beam high in the air lights little.
    float t = saturate(dot(world - BeamStart.xyz, BeamSpan.xyz) * BeamSpan.w);
    float3 toBeam = BeamStart.xyz + BeamSpan.xyz * t - world;
    float distance = length(toBeam);
    float falloff = pow(saturate(1.0f - distance * Glow.w), Shape.x);

    // Wrapped, so ground facing away from the beam keeps some light, and slopes facing it light most.
    float2 slope = input.Normal.xy * 2.0f - 1.0f;
    float3 normal = float3(slope, sqrt(saturate(1.0f - dot(slope, slope))));
    float facing = lerp(saturate(dot(normal, toBeam) / max(distance, 0.001f)), 1.0f, Shape.y);

    // The laser shader's pulses light the ground as they pass.
    float along = BeamStart.w + t * Pulse.y;
    float4 noiseA = tex2D(NoiseTexture, float2(along - Pulse.x, 0.25f));
    float4 noiseB = tex2D(NoiseTexture, float2(along * 2.3f - Pulse.x * 1.5f, 0.75f));
    float pulse = 1.0f + Pulse.z * (noiseA.r + noiseB.g - 1.0f);

    return float4(Glow.rgb * (falloff * facing * pulse), 1.0f);
}
