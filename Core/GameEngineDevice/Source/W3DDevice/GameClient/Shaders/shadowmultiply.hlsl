// A second pass that multiplies the sun's shadow into geometry already drawn.
//
// For surfaces that render fixed function, such as bridges, where rebuilding the
// whole lighting model in a shader would only reproduce it. The caller blends the
// output as destination times source, so a lit pixel returns white.
//
// PACKED picks the depth format. The shadow map is the only texture, on stage 0.

sampler2D ShadowMap : register(s0);

#include "shadowreceive.hlsli"

float4 main(float4 shadowPos : TEXCOORD0) : COLOR
{
    return float4(ShadowFactor(shadowPos), 1.0f);
}
