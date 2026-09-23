// Instanced meshes in the main scene, and their shadow receiver and specular passes. The passes
// redraw a mesh with a depth test of EQUAL, so the base pass and its passes share this one shader:
// the same instructions on the same inputs reach the same depth.
//
// The base pass lights as the fixed-function pipeline does for directional lights without specular:
// emissive + ambient * Ma + sum(max(N.L, 0) * diffuse * Md), saturated, alpha from Md. Each instance
// brings its world matrix and the part of its lighting that differs from the group's.
//
// Each of stages 0-3 takes a source and a texture transform, which gives the base pass its UV sets
// and a pass the coordinates fixed-function texgen would generate for it.
//
// SKINNED=1 draws one skin mesh instead. Each vertex takes its world rows from the bone palette,
// and the mesh's own lights and ambient replace the group's.

float4 ViewProjectionColumns[4] : register(c0);
float4 ViewColumns[3]           : register(c4);
float4 MaterialAmbient          : register(c8);
float4 MaterialDiffuse          : register(c9);
float4 MaterialEmissive         : register(c10);
float4 ColorSource              : register(c11);  // weights of the vertex colour for ambient, diffuse, emissive; w = lighting on
float4 Fog                      : register(c12);  // factor = saturate(x - distance * y); z = weight of range over depth
float4 SkinAmbient              : register(c13);
float4 LightDirection[4]        : register(c16);  // toward the light; w = 1 for a light in use
float4 LightDiffuse[4]          : register(c20);
float4 StageSource[4]           : register(c24);  // weights of UV set 0, UV set 1, camera-space normal, camera-space position
float4 StageColumns[16]         : register(c28);  // texture transform columns, four per stage
float4 Palette[210]             : register(c44);  // three world rows per bone

struct VsIn
{
    float4 Position      : POSITION;
    float3 Normal        : NORMAL;
    float4 Diffuse       : COLOR0;
    float2 TexCoord0     : TEXCOORD0;
    float2 TexCoord1     : TEXCOORD1;
#if SKINNED
    float4 Bone          : COLOR1;
#else
    float3 DiffuseOffset : TEXCOORD3;
    float4 World0        : TEXCOORD4;
    float4 World1        : TEXCOORD5;
    float4 World2        : TEXCOORD6;
    float3 Ambient       : TEXCOORD7;
#endif
};

struct VsOut
{
    float4 Position  : POSITION;
    float4 Diffuse   : COLOR0;
    float4 Specular  : COLOR1;
    float4 TexCoord0 : TEXCOORD0;
    float4 TexCoord1 : TEXCOORD1;
    float4 TexCoord2 : TEXCOORD2;
    float4 TexCoord3 : TEXCOORD3;
    float  Fog       : FOG;
};

float4 StageCoord(int stage, float4 sources[4])
{
    float4 source = StageSource[stage].x * sources[0] + StageSource[stage].y * sources[1] +
        StageSource[stage].z * sources[2] + StageSource[stage].w * sources[3];
    return float4(dot(source, StageColumns[stage * 4 + 0]), dot(source, StageColumns[stage * 4 + 1]),
        dot(source, StageColumns[stage * 4 + 2]), dot(source, StageColumns[stage * 4 + 3]));
}

VsOut main(VsIn input)
{
#if SKINNED
    int bone = D3DCOLORtoUBYTE4(input.Bone).x * 3;
    float4 world0 = Palette[bone];
    float4 world1 = Palette[bone + 1];
    float4 world2 = Palette[bone + 2];
    float3 ambientLight = SkinAmbient.xyz;
    float3 diffuseOffset = float3(0.0f, 0.0f, 0.0f);
#else
    float4 world0 = input.World0;
    float4 world1 = input.World1;
    float4 world2 = input.World2;
    float3 ambientLight = input.Ambient;
    float3 diffuseOffset = input.DiffuseOffset;
#endif

    float4 local = float4(input.Position.xyz, 1.0f);
    float4 world = float4(dot(world0, local), dot(world1, local), dot(world2, local), 1.0f);
    float3 view = float3(dot(world, ViewColumns[0]), dot(world, ViewColumns[1]), dot(world, ViewColumns[2]));

    VsOut output;
    output.Position = float4(dot(world, ViewProjectionColumns[0]), dot(world, ViewProjectionColumns[1]),
        dot(world, ViewProjectionColumns[2]), dot(world, ViewProjectionColumns[3]));

    // A mesh without normals lights nothing and hands texgen zero, as fixed function does.
    float3 worldNormal = float3(dot(world0.xyz, input.Normal), dot(world1.xyz, input.Normal),
        dot(world2.xyz, input.Normal));
    float normalLength = dot(worldNormal, worldNormal);
    worldNormal = (normalLength > 0.0f) ? worldNormal * rsqrt(normalLength) : float3(0.0f, 0.0f, 0.0f);
    float3 viewNormal = float3(dot(worldNormal, ViewColumns[0].xyz), dot(worldNormal, ViewColumns[1].xyz),
        dot(worldNormal, ViewColumns[2].xyz));

    float4 ambient = lerp(MaterialAmbient, input.Diffuse, ColorSource.x);
    float4 diffuse = lerp(MaterialDiffuse, input.Diffuse, ColorSource.y);
    float4 emissive = lerp(MaterialEmissive, input.Diffuse, ColorSource.z);

    float3 color = emissive.rgb + ambientLight * ambient.rgb;
    for (int i = 0; i < 4; ++i)
    {
        float3 light = LightDiffuse[i].rgb + diffuseOffset * LightDirection[i].w;
        color += max(dot(worldNormal, LightDirection[i].xyz), 0.0f) * light * diffuse.rgb;
    }

    // Unlit meshes take the vertex colour, which is opaque white when the mesh has none.
    output.Diffuse = lerp(input.Diffuse, float4(saturate(color), diffuse.a), ColorSource.w);
    output.Specular = float4(0.0f, 0.0f, 0.0f, 0.0f);

    float4 sources[4];
    sources[0] = float4(input.TexCoord0, 1.0f, 0.0f);
    sources[1] = float4(input.TexCoord1, 1.0f, 0.0f);
    sources[2] = float4(viewNormal, 1.0f);
    sources[3] = float4(view, 1.0f);
    output.TexCoord0 = StageCoord(0, sources);
    output.TexCoord1 = StageCoord(1, sources);
    output.TexCoord2 = StageCoord(2, sources);
    output.TexCoord3 = StageCoord(3, sources);

    float distance = lerp(view.z, length(view), Fog.z);
    output.Fog = saturate(Fog.x - distance * Fog.y);
    return output;
}
