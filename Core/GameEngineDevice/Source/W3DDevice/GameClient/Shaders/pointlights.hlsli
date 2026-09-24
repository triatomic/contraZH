// Dynamic point lights added per pixel, with the falloff the terrain's vertex lighting uses.
//
// From POINT_LIGHT_REGISTER on, each light takes two registers:
// - position, and the falloff's scale per unit of distance
// - diffuse colour, and the falloff's offset
// One more register per four lights follows with each light's ambient colour as a fraction of its diffuse.
// An unused light has zero colour. Position and surface share one space, world or camera.

#ifndef POINT_LIGHT_COUNT
#define POINT_LIGHT_COUNT 8
#endif

float4 PointLights[POINT_LIGHT_COUNT * 2 + (POINT_LIGHT_COUNT + 3) / 4] : register(POINT_LIGHT_REGISTER);

float3 PointLighting(float3 position, float3 normal)
{
    float3 sum = float3(0.0f, 0.0f, 0.0f);
    for (int i = 0; i < POINT_LIGHT_COUNT; i++)
    {
        float4 place = PointLights[i * 2];
        float4 color = PointLights[i * 2 + 1];
        float ambient = PointLights[POINT_LIGHT_COUNT * 2 + i / 4][i % 4];

        float3 toLight = place.xyz - position;
        float distance = length(toLight);
        float factor = saturate(color.w - distance * place.w);
        float shade = saturate(dot(normal, toLight) / max(distance, 1e-3f));
        sum += color.rgb * (factor * (shade + ambient));
    }
    return sum;
}
