// Instanced shadow map casters. Each instance brings its world matrix, and the outputs match
// what fixed-function vertex processing hands the depth pass for a single caster.
//
// PACKED=1 feeds shadowdepth.hlsl, which reads the sun view-space position from TEXCOORD1.
// PACKED=0 feeds the fixed-function pixel pipeline, whose stages read both UV sets.
// SKINNED=1 draws one skin mesh, each vertex taking its world rows from the bone palette.

float4 ViewProjectionColumns[4] : register(c0);
float4 ViewColumns[3]           : register(c4);
float2 DiffuseAlpha             : register(c7);   // x = constant alpha, y = weight of the vertex alpha
float4 Palette[210]             : register(c44);  // three world rows per bone

struct VsIn
{
    float4 Position  : POSITION;
    float4 Diffuse   : COLOR0;
    float2 TexCoord0 : TEXCOORD0;
    float2 TexCoord1 : TEXCOORD1;
#if SKINNED
    float4 Bone      : COLOR1;
#else
    float4 World0    : TEXCOORD4;
    float4 World1    : TEXCOORD5;
    float4 World2    : TEXCOORD6;
#endif
};

struct VsOut
{
    float4 Position  : POSITION;
    float4 Diffuse   : COLOR0;
    float2 TexCoord0 : TEXCOORD0;
#if PACKED
    float3 TexCoord1 : TEXCOORD1;
#else
    float2 TexCoord1 : TEXCOORD1;
#endif
};

VsOut main(VsIn input)
{
#if SKINNED
    int bone = D3DCOLORtoUBYTE4(input.Bone).x * 3;
    float4 world0 = Palette[bone];
    float4 world1 = Palette[bone + 1];
    float4 world2 = Palette[bone + 2];
#else
    float4 world0 = input.World0;
    float4 world1 = input.World1;
    float4 world2 = input.World2;
#endif

    float4 local = float4(input.Position.xyz, 1.0f);
    float4 world = float4(dot(world0, local), dot(world1, local), dot(world2, local), 1.0f);

    VsOut output;
    output.Position = float4(dot(world, ViewProjectionColumns[0]), dot(world, ViewProjectionColumns[1]),
        dot(world, ViewProjectionColumns[2]), dot(world, ViewProjectionColumns[3]));

    // Colour writes are off in the depth pass, so only alpha reaches a cutout.
    output.Diffuse = float4(1.0f, 1.0f, 1.0f, lerp(DiffuseAlpha.x, input.Diffuse.a, DiffuseAlpha.y));
    output.TexCoord0 = input.TexCoord0;
#if PACKED
    output.TexCoord1 = float3(dot(world, ViewColumns[0]), dot(world, ViewColumns[1]), dot(world, ViewColumns[2]));
#else
    output.TexCoord1 = input.TexCoord1;
#endif
    return output;
}
