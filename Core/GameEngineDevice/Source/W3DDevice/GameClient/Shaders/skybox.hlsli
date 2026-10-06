// The skybox's five faces, bound by WaterRenderObjClass::bindSkyboxFaces.

sampler2D SkyNorth : register(s8);
sampler2D SkyEast  : register(s9);
sampler2D SkySouth : register(s10);
sampler2D SkyWest  : register(s11);
sampler2D SkyTop   : register(s12);

// The skybox is a box around the camera with five faces, no bottom, laid out as new_skybox.w3d
// maps them. Every face is sampled and the one the ray leaves through is kept.
float3 SkyboxColor(float3 dir)
{
    dir.z = max(dir.z, 0.02f);
    float3 a = abs(dir);
    float2 pX = dir.yz / a.x;
    float2 pY = dir.xz / a.y;
    float2 pZ = dir.xy / a.z;

    float3 north = tex2D(SkyNorth, float2(1.0f - pX.x, 1.0f - pX.y) * 0.5f).rgb;
    float3 south = tex2D(SkySouth, float2(1.0f + pX.x, 1.0f - pX.y) * 0.5f).rgb;
    float3 east  = tex2D(SkyEast,  float2(1.0f + pY.x, 1.0f - pY.y) * 0.5f).rgb;
    float3 west  = tex2D(SkyWest,  float2(1.0f - pY.x, 1.0f - pY.y) * 0.5f).rgb;
    float3 top   = tex2D(SkyTop,   float2(1.0f - pZ.x, 1.0f + pZ.y) * 0.5f).rgb;

    float3 side = (a.x >= a.y) ? ((dir.x < 0.0f) ? north : south) : ((dir.y < 0.0f) ? east : west);
    return (a.z >= max(a.x, a.y)) ? top : side;
}
