"""Exercise the production emissive light clustering and laser sampling without the game."""

from pathlib import Path
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[1]
VCVARS = Path(r"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat")

PREAMBLE = r'''
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <map>
#include <string>
#include <vector>
using std::min;
using std::max;
typedef int Int;
typedef float Real;
typedef bool Bool;
#define TRUE true
#define FALSE false
#define RTS_ZEROHOUR 1
#define DEBUG_ASSERTCRASH(test, message) assert(test)
struct Vector2
{
    float X, Y;
    Vector2(float x=0, float y=0) : X(x), Y(y) {}
    Vector2 operator*(float s) const { return Vector2(X*s,Y*s); }
    Vector2 operator+(Vector2 v) const { return Vector2(X+v.X,Y+v.Y); }
};
struct Vector3
{
    float X, Y, Z;
    Vector3() : X(0), Y(0), Z(0) {}
    Vector3(float x, float y, float z) : X(x), Y(y), Z(z) {}
    Vector3 operator+(Vector3 v) const { return Vector3(X+v.X, Y+v.Y, Z+v.Z); }
    Vector3 operator-(Vector3 v) const { return Vector3(X-v.X, Y-v.Y, Z-v.Z); }
    Vector3 operator*(float s) const { return Vector3(X*s, Y*s, Z*s); }
    Vector3 operator/(float s) const { return *this * (1/s); }
    Vector3& operator+=(Vector3 v) { *this = *this+v; return *this; }
    Vector3& operator*=(float s) { *this = *this*s; return *this; }
    Vector3& operator/=(float s) { *this = *this/s; return *this; }
    void Set(float x,float y,float z) { X=x; Y=y; Z=z; }
    static void Cross_Product(Vector3 a,Vector3 b,Vector3* out)
    { out->Set(a.Y*b.Z-a.Z*b.Y,a.Z*b.X-a.X*b.Z,a.X*b.Y-a.Y*b.X); }
    float Length2() const { return X*X+Y*Y+Z*Z; }
    float Length() const { return sqrtf(Length2()); }
};
struct SphereClass
{
    Vector3 Center;
    float Radius;
    SphereClass(Vector3 center, float radius) : Center(center), Radius(radius) {}
};
struct Matrix3D
{
    Vector3 offset;
    float angle, scale;
    Matrix3D() : angle(0), scale(1) {}
    void Get_X_Vector(Vector3* axis) const { *axis=Vector3(scale,0,0); }
    static void Transform_Vector(const Matrix3D& tm, const Vector3& in, Vector3* out)
    {
        const Vector3 result((in.X*cosf(tm.angle)-in.Y*sinf(tm.angle))*tm.scale,
                             (in.X*sinf(tm.angle)+in.Y*cosf(tm.angle))*tm.scale, in.Z*tm.scale);
        *out=result+tm.offset;
    }
};
struct AABoxClass { Vector3 Center, Extent; };
class CameraClass
{
public:
    bool hidden;
    CameraClass() : hidden(false) {}
    bool Cull_Sphere(const SphereClass&) const { return hidden; }
};
class TextureClass {};
class ShaderClass
{
public:
    enum { DSTBLEND_ONE, SRCBLEND_ONE, SRCBLEND_SRC_ALPHA };
    int source;
    ShaderClass() : source(SRCBLEND_SRC_ALPHA) {}
    int Get_Dst_Blend_Func() const { return DSTBLEND_ONE; }
    int Get_Src_Blend_Func() const { return source; }
};
class VertexMaterialClass
{
public:
    void Get_Emissive(Vector3* v) { *v=Vector3(); }
    void Get_Diffuse(Vector3* v) { *v=Vector3(1,1,1); }
    float Get_Opacity() { return 1; }
};
struct TriIndex { int index[3]; int operator[](int i) const { return index[i]; } };
class MeshModelClass
{
public:
    Vector3 vertices[3];
    TriIndex triangle;
    Vector2 uv[3];
    ShaderClass shader;
    VertexMaterialClass material;
    TextureClass texture;
    MeshModelClass() : triangle{0,1,2} { vertices[1]=Vector3(10,0,0); vertices[2]=Vector3(0,10,0); }
    const Vector3* Get_Vertex_Array() { return vertices; }
    const TriIndex* Get_Polygon_Array() { return &triangle; }
    int Get_Pass_Count() { return 1; }
    int Get_Polygon_Count() { return 1; }
    const Vector2* Get_UV_Array(int) { return uv; }
    ShaderClass Get_Shader(int,int) { return shader; }
    VertexMaterialClass* Peek_Material(int,int) { return &material; }
    TextureClass* Peek_Texture(int,int) { return &texture; }
    bool Get_Flag(int) { return false; }
    bool Is_Alternate_Material_Description_Enabled() { return false; }
};
struct MeshGeometryClass { enum { SKIN }; };
template<class T> struct RefCountPtr
{
    T* ptr;
    RefCountPtr& operator=(T* value) { ptr=value; return *this; }
};
class RenderObjClass
{
public:
    enum { CLASSID_HLOD, CLASSID_MESH };
    Matrix3D transform;
    bool hidden;
    RenderObjClass() : hidden(false) {}
    bool Is_Not_Hidden_At_All() { return !hidden; }
    virtual int Class_ID() { return CLASSID_MESH; }
    int Get_Num_Sub_Objects() { return 0; }
    RenderObjClass* Get_Sub_Object(int) { return NULL; }
    void Release_Ref() {}
    const Matrix3D& Get_Transform() { return transform; }
};
class HLodClass : public RenderObjClass
{
public:
    int Get_LOD_Level() { return 0; }
    int Get_Lod_Model_Count(int) { return 0; }
    RenderObjClass* Peek_Lod_Model(int,int) { return NULL; }
    int Get_Additional_Model_Count() { return 0; }
    RenderObjClass* Peek_Additional_Model(int) { return NULL; }
};
class W3DShaderManager
{
public:
    static TextureClass* findEmissiveMap(TextureClass*) { return NULL; }
    // PIXEL_LIGHT_STRUCT
    enum { MAX_PIXEL_LIGHTS = 8, MAX_UNIT_PIXEL_LIGHTS = 8 };
    static void filterPixelLights(const AABoxClass&, std::vector<int>&);
    static int getPixelLightCount();
    static const PixelLight& getPixelLight(int);
    static int pickPixelLights(const AABoxClass&, int*);
    static int pickPixelLights(const AABoxClass&, std::vector<int>&);
    static void setPixelLights(const PixelLight*, int);
};
static std::vector<W3DShaderManager::PixelLight> PixelLights(64);
static int PixelLightCount;
int W3DShaderManager::getPixelLightCount() { return PixelLightCount; }
const W3DShaderManager::PixelLight& W3DShaderManager::getPixelLight(int i) { return PixelLights[i]; }
class RTS3DScene { public: int pickObjectPixelLights(const SphereClass&, int*); };
namespace WWMath { float Clamp(float x,float lo,float hi) { return min(max(x,lo),hi); } }
static size_t ObjectSpecularPassesUsed;
static void Update_Lights_Debug() {}
enum { TIME_OF_DAY_NIGHT };
struct GlobalData
{
    bool m_laserRef, m_usePathTracing, m_useSpecular;
    int m_timeOfDay;
    float m_unitEmissiveNightIntensity, m_unitEmissiveIntensity;
};
static GlobalData globalData = { true, true, true, 0, 1, 1 };
static GlobalData* TheGlobalData = &globalData;
#define private public
// EMISSIVE_HEADER
#undef private
static const Real EMISSIVE_CELL_SIZE = 24.0f;
// EMISSIVE_FUNCTIONS
W3DEmissiveLights::TextureSamples* W3DEmissiveLights::textureSamples(TextureClass* texture)
{
    return texture != NULL ? &m_textures[texture] : NULL;
}
class W3DLaserGlow
{
public:
    struct Glow { Vector3 start, end; Real reach; Vector3 color; };
    Glow m_glows[64];
    Int m_count;
    void appendObjectLights(std::vector<W3DShaderManager::PixelLight>&) const;
};
// LASER_FUNCTION
// PICK_FUNCTION
// TUNE_FUNCTION
// OBJECT_LIGHTS
struct Vector4 { float X,Y,Z,W; };
struct IDirect3DVertexDeclaration9 {};
struct IDirect3DVertexShader9 {};
struct IDirect3DVertexBuffer9 {};
class DX8PolygonRendererClass
{
public:
    unsigned Get_Index_Offset() { return 0; }
    unsigned Get_Index_Count() { return 6; }
    unsigned Get_Min_Vertex_Index() { return 0; }
    unsigned Get_Vertex_Index_Range() { return 4; }
};
class MeshClass : public RenderObjClass
{
public:
    MeshModelClass* model;
    MeshClass() : model(NULL) {}
    MeshModelClass* Peek_Model() { return model; }
    void* Get_Lighting_Environment() { return NULL; }
    int Get_Base_Vertex_Offset() { return 0; }
};
// ADD_OBJECT_FUNCTION
class MaterialPassClass
{
public:
    mutable std::vector<MeshClass*> meshes;
    void Install_Polygon_Materials(DX8PolygonRendererClass*,MeshClass* mesh=NULL) const { meshes.push_back(mesh); }
};
class DX8InstancingClass
{
public:
    enum { PASS_LIT };
    static int Get_Pass() { return PASS_LIT; }
};
class DX8VertexShadingClass
{
public:
    enum { MAX_LIGHTS=3, CONSTANT_LIGHT_DIRECTION=0 };
    static void Get_Light_Constants(void*,Vector4*) {}
};
class DX8Wrapper
{
public:
    static bool active;
    static std::vector<unsigned> counts;
    static std::vector<unsigned> offsets;
    static void Begin_Instanced_Drawing(IDirect3DVertexDeclaration9*,IDirect3DVertexShader9*) { active=true; }
    static void End_Instanced_Drawing() { active=false; }
    static void Set_Vertex_Shader_Constant(int,Vector4*,int) {}
    static void Set_Index_Buffer_Index_Offset(int) {}
    static void Draw_Instanced_Triangles(unsigned,unsigned,unsigned,unsigned,IDirect3DVertexBuffer9*,unsigned offset,unsigned,unsigned count)
    {
        assert(active);
        offsets.push_back(offset);
        counts.push_back(count);
    }
};
bool DX8Wrapper::active=false;
std::vector<unsigned> DX8Wrapper::counts,DX8Wrapper::offsets;
struct InstanceRecord { float data[20]; };
static IDirect3DVertexBuffer9* InstanceBuffer=NULL;
// INSTANCED_DRAW
enum { MAX_DRAW_DATA = 9, WIN_STATUS_HIDDEN = 16 };
struct WinInstanceData
{
    int m_id, m_style, m_enabledText, m_disabledText, m_hiliteText;
    int m_enabledDrawData[9], m_disabledDrawData[9], m_hiliteDrawData[9];
};
class GameWindow
{
public:
    int x, y, width, height;
    GameWindow *parent, *child, *next;
    WinInstanceData look;
    std::wstring label, tooltip;
    GameWindow(int left, int top, int w, int h) : x(left), y(top), width(w), height(h), parent(NULL), child(NULL), next(NULL), look() {}
    void winGetPosition(int* a, int* b) { *a=x; *b=y; }
    void winGetSize(int* a, int* b) { *a=width; *b=height; }
    GameWindow* winGetParent() { return parent; }
    GameWindow* winGetChild() { return child; }
    GameWindow* winGetNext() { return next; }
    const WinInstanceData* winGetInstanceData() { return &look; }
    int winGetStatus() { return 8|128; }
    void* winGetFont() { return NULL; }
    void winSetTooltip(const wchar_t* text) { tooltip=text; }
};
static GameWindow *WinShaders, *checkAmbientOcclusion, *existingCheck;
static GameWindow* findOptionsWindow(const char*) { return existingCheck; }
static void GadgetCheckBoxSetText(GameWindow* w, const wchar_t* text) { w->label=text; }
struct NameKeys { int nameToKey(const char*) { return 42; } };
static NameKeys names;
static NameKeys* TheNameKeyGenerator = &names;
struct GameText { const wchar_t* fetch(const char*, const wchar_t* text) { return text; } };
static GameText texts;
static GameText* TheGameText = &texts;
#define FETCH_OR_SUBSTITUTE fetch
struct WindowManager
{
    GameWindow* gogoGadgetCheckbox(GameWindow* parent, int, int x, int y, int w, int h, WinInstanceData* data, void*, bool)
    {
        existingCheck = new GameWindow(x,y,w,h);
        existingCheck->parent = parent;
        existingCheck->look = *data;
        return existingCheck;
    }
};
static WindowManager windows;
static WindowManager* TheWindowManager = &windows;
// CHECKBOX_FUNCTION
'''

TESTS = r'''
static bool near(float a, float b) { return fabsf(a-b) < 0.0001f; }
int main()
{
    CameraClass camera;
    W3DEmissiveLights lights;
    lights.add(Vector3(4, 4, 4), Vector3(1, 0, 0), 64, 24);
    const auto& one = lights.finish(camera);
    assert(one.size() == 1);
    assert(near(one[0].position.X, 4) && near(one[0].diffuse.X, 1));
    assert(one[0].diffuse.Y == 0 && one[0].diffuse.Z == 0);
    assert(!one[0].terrainOnly && !one[0].objectsOnly);
    lights.reset();

    // Brightness weights the source position when a red and blue lamp share a cell.
    lights.add(Vector3(2, 2, 2), Vector3(1, 0, 0), 64, 24);
    lights.add(Vector3(10, 2, 2), Vector3(0, 0, 1), 64, 36);
    const auto& merged = lights.finish(camera);
    assert(merged.size() == 1 && near(merged[0].position.X, 6));
    assert(near(merged[0].diffuse.X, merged[0].diffuse.Z));
    assert(merged[0].outerRadius == 36);
    lights.reset();

    // Sources across zero occupy different cells, as sources across any other boundary do.
    lights.add(Vector3(-1, 0, 0), Vector3(1, 1, 1), 64, 24);
    lights.add(Vector3(1, 0, 0), Vector3(1, 1, 1), 64, 24);
    assert(lights.finish(camera).size() == 2);
    lights.reset();
    lights.add(Vector3(), Vector3(), 64, 24);
    lights.add(Vector3(), Vector3(1, 1, 1), 0, 24);
    lights.add(Vector3(), Vector3(1, 1, 1), 64, 0);
    assert(lights.finish(camera).empty());
    for (int i = 0; i < 10000; i++)
    {
        lights.add(Vector3(), Vector3(1, 0.5f, 0), 100, 24);
    }
    const auto& bright = lights.finish(camera);
    assert(bright.size() == 1 && near(bright[0].diffuse.X, 1.5f));
    assert(near(bright[0].diffuse.Y, 0.75f));
    lights.reset();
    camera.hidden = true;
    lights.add(Vector3(), Vector3(1, 1, 1), 64, 24);
    assert(lights.finish(camera).empty());
    camera.hidden = false;
    lights.reset();

    W3DEmissiveLights::TextureSamples texture;
    for (int i = 0; i < 32*32; i++)
    {
        texture.pixels[i] = Vector3(float(i), 0, 0);
    }
    assert(W3DEmissiveLights::sampleTexture(&texture, Vector2(1, 1)).X == 0);
    assert(W3DEmissiveLights::sampleTexture(&texture, Vector2(-0.01f, -0.01f)).X == 1023);
    assert(W3DEmissiveLights::sampleTexture(&texture, Vector2(0.99999f, 0.99999f)).X == 1023);

    MeshModelClass model;
    W3DEmissiveLights::TextureSamples& meshTexture=lights.m_textures[&model.texture];
    for (int i=0; i<32*32; i++)
    {
        meshTexture.pixels[i]=Vector3(1,1,1);
        meshTexture.alpha[i]=0;
    }
    std::vector<W3DEmissiveLights::Sample> samples;
    assert(lights.sampleModel(model,samples) && samples.empty());
    for (int i=0; i<32*32; i++)
    {
        meshTexture.alpha[i]=0.5f;
    }
    assert(lights.sampleModel(model,samples) && samples.size()==4);
    assert(near(samples[0].color.X,0.5f));
    samples.clear();
    model.shader.source=ShaderClass::SRCBLEND_ONE;
    assert(lights.sampleModel(model,samples) && samples.size()==4);
    assert(near(samples[0].color.X,1));
    lights.reset();

    // A rigid emitter keeps its lights while translating and rotating across world cells.
    MeshClass moving;
    moving.model=&model;
    auto& movingTexture=lights.m_textures[&model.texture];
    for (int i=0; i<32*32; i++)
    {
        movingTexture.pixels[i]=Vector3(1,1,1);
        movingTexture.alpha[i]=1;
    }
    lights.addObject(moving,1);
    const auto reference=lights.finish(camera);
    assert(reference.size()==1);
    for (int frame=0; frame<300; frame++)
    {
        lights.m_clusters.clear();
        lights.m_lights.clear();
        moving.transform.offset=Vector3(frame*0.5f-75,frame*0.3f-45,frame*0.2f-30);
        moving.transform.angle=frame*0.03f;
        lights.addObject(moving,1);
        const auto& current=lights.finish(camera);
        assert(current.size()==reference.size());
        Vector3 expected;
        Matrix3D::Transform_Vector(moving.transform,reference[0].position,&expected);
        assert((current[0].position-expected).Length()<0.0001f);
        assert(near(current[0].diffuse.X,reference[0].diffuse.X));
        assert(near(current[0].outerRadius,reference[0].outerRadius));
    }
    lights.m_lights.clear();
    lights.add(Vector3(100,100,100),Vector3(1,0,0),64,24);
    lights.addObject(moving,1);
    lights.addObject(moving,1);
    assert(lights.finish(camera).size()==3);
    lights.m_lights.clear();
    moving.hidden=true;
    lights.addObject(moving,1);
    assert(lights.finish(camera).empty());
    moving.hidden=false;
    lights.addObject(moving,0);
    assert(lights.finish(camera).empty());
    lights.addObject(moving,1);
    camera.hidden=true;
    assert(lights.finish(camera).empty());
    camera.hidden=false;
    lights.reset();

    MeshClass firstInstance,secondInstance,thirdInstance,lastInstance;
    MeshClass* instanceMeshes[]={&firstInstance,&secondInstance,&thirdInstance,&lastInstance};
    DX8PolygonRendererClass polygon;
    DX8PolygonRendererClass* renderers[]={&polygon,&polygon,&polygon};
    const int instanceCounts[]={1,2,1};
    MaterialPassClass materialPass;
    Draw_Instances(NULL,NULL,renderers,instanceCounts,3,instanceMeshes,0,&materialPass);
    assert(materialPass.meshes.size()==3);
    assert(materialPass.meshes[0]==&firstInstance && materialPass.meshes[1]==NULL && materialPass.meshes[2]==&lastInstance);
    assert(DX8Wrapper::counts==std::vector<unsigned>({1,2,1}));
    assert(DX8Wrapper::offsets==std::vector<unsigned>({0,80,240}));
    assert(!DX8Wrapper::active);

    W3DLaserGlow laser;
    laser.m_count = 1;
    laser.m_glows[0] = { Vector3(0,0,10), Vector3(96,0,10), 24, Vector3(1,0,0) };
    std::vector<W3DShaderManager::PixelLight> beam;
    laser.appendObjectLights(beam);
    assert(beam.size() == 4 && near(beam.front().position.X, 12) && near(beam.back().position.X, 84));
    for (const auto& light : beam)
    {
        assert(light.objectsOnly && !light.terrainOnly);
        assert(light.outerRadius > light.innerRadius && light.diffuse.X > 0);
    }
    beam.clear();
    laser.m_glows[0].end = laser.m_glows[0].start;
    laser.appendObjectLights(beam);
    assert(beam.size() == 1 && std::isfinite(beam[0].diffuse.X));
    beam.clear();
    laser.m_glows[0].end = Vector3(100000,0,10);
    laser.appendObjectLights(beam);
    assert(beam.size() == 16);
    assert(beam[0].outerRadius >= 100000.0f/16);
    TheGlobalData->m_laserRef = false;
    beam.clear();
    laser.appendObjectLights(beam);
    assert(!beam[0].objectsOnly);
    TheGlobalData->m_laserRef = true;

    // The laser's existing terrain pass must not be lit again by its object lights.
    PixelLightCount = 2;
    PixelLights[0] = beam[0];
    PixelLights[0].objectsOnly = true;
    PixelLights[0].position = Vector3();
    PixelLights[1] = beam[0];
    PixelLights[1].objectsOnly = false;
    PixelLights[1].position = Vector3();
    AABoxClass box;
    int picked[8];
    assert(W3DShaderManager::pickPixelLights(box, picked) == 1 && picked[0] == 1);

    // Dense scenes keep emitters beyond both the global and per-draw limits.
    std::vector<W3DShaderManager::PixelLight> dense(257, PixelLights[1]);
    dense.back().objectsOnly = true;
    W3DShaderManager::setPixelLights(&dense[0], (int)dense.size());
    assert(PixelLightCount == 257 && PixelLights.size() == 257);
    std::vector<int> allLights;
    assert(W3DShaderManager::pickPixelLights(box, allLights) == 256);
    assert(allLights.front() == 0 && allLights.back() == 255);

    PixelLights[0].position=Vector3(0,0,100);
    PixelLights[0].outerRadius=20;
    PixelLights[1].position=Vector3(100,100,0);
    PixelLights[1].outerRadius=20;
    PixelLights[2].position=Vector3(2.5f,0,0);
    PixelLights[2].outerRadius=3;
    PixelLights[2].objectsOnly=true;
    std::vector<int> local={0,1,2};
    W3DShaderManager::filterPixelLights(box,local);
    assert(local.size()==1 && local[0]==2);
    W3DShaderManager::filterPixelLights(box,local);
    assert(local.size()==1 && local[0]==2);
    local.clear();
    W3DShaderManager::filterPixelLights(box,local);
    assert(local.empty());

    RTS3DScene scene;
    std::vector<int> objectLights(PixelLightCount);
    PixelLights[3].terrainOnly=true;
    assert(scene.pickObjectPixelLights(SphereClass(Vector3(),1),&objectLights[0])==254);
    TheGlobalData->m_usePathTracing=false;
    assert(scene.pickObjectPixelLights(SphereClass(Vector3(),1),&objectLights[0])==8);
    TheGlobalData->m_usePathTracing=true;

    W3DShaderManager::PixelLight original = dense[0];
    original.innerRadius = 10;
    original.outerRadius = 60;
    W3DShaderManager::PixelLight tuned = original;
    assert(Tune_Emissive_Light(tuned, 30, 0.25f));
    assert(near(tuned.outerRadius, 30) && near(tuned.innerRadius, 5));
    assert(near(tuned.diffuse.X, original.diffuse.X * 0.25f));
    assert(near(tuned.position.X, original.position.X) && tuned.objectsOnly == original.objectsOnly);
    tuned = original;
    tuned.outerRadius = 20;
    assert(Tune_Emissive_Light(tuned, 30, 1.0f));
    assert(near(tuned.outerRadius, 20) && near(tuned.innerRadius, 10));
    assert(near(tuned.diffuse.X, original.diffuse.X));
    tuned = original;
    assert(Tune_Emissive_Light(tuned, 0, 1.0f));
    assert(near(tuned.outerRadius, 60));
    assert(Tune_Emissive_Light(tuned, -1, 1.0f));
    assert(near(tuned.outerRadius, 60));
    assert(!Tune_Emissive_Light(tuned, 30, 0.0f));
    assert(!Tune_Emissive_Light(tuned, 30, -1.0f));

    // Existing layouts receive one checkbox in a free row, with the surrounding controls' look.
    GameWindow panel(0,0,600,300), anchor(10,40,230,20), occupied(10,63,230,20);
    WinShaders = &panel;
    checkAmbientOcclusion = &anchor;
    anchor.parent = &panel;
    anchor.look.m_style = 123;
    panel.child = &anchor;
    anchor.next = &occupied;
    GameWindow* checkbox = createPathTracingCheck();
    assert(checkbox != NULL && checkbox->x == 10 && checkbox->y == 86);
    assert(checkbox->look.m_id == 42 && checkbox->look.m_style == 123);
    assert(checkbox->label == L"Path tracing" && !checkbox->tooltip.empty());
    assert(createPathTracingCheck() == checkbox);
    delete checkbox;
    existingCheck = NULL;
    WinShaders = NULL;
    assert(createPathTracingCheck() == NULL);
    puts("PASS: single-instance mesh handoff, light bounds, object selection, emissive clustering, transparent meshes, tuning and lasers");
}
'''


def function(source, signature, indent=""):
    start = source.index(signature)
    return source[start:source.index("\n" + indent + "}", start) + len(indent) + 2]


def main():
    client = ROOT / "Core/GameEngineDevice/Source/W3DDevice/GameClient"
    emission = (client / "W3DEmissiveLights.cpp").read_text(encoding="utf-8")
    shader = (client / "W3DShaderManager.cpp").read_text(encoding="utf-8")
    shader_header = (ROOT / "Core/GameEngineDevice/Include/W3DDevice/GameClient/W3DShaderManager.h").read_text(encoding="utf-8")
    start = shader_header.index("\tstruct PixelLight\n")
    pixel_light = shader_header[start:shader_header.index("\n\t};", start) + 4]
    header = (ROOT / "Core/GameEngineDevice/Include/W3DDevice/GameClient/W3DEmissiveLights.h").read_text(encoding="utf-8")
    header = "\n".join(line for line in header.splitlines() if not line.startswith(("#pragma", '#include "')))
    names = ["W3DEmissiveLights::W3DEmissiveLights()", "W3DEmissiveLights::~W3DEmissiveLights()",
             "W3DEmissiveLights::Cluster::Cluster()", "void W3DEmissiveLights::reset()",
             "void W3DEmissiveLights::flushClusters(",
             "void W3DEmissiveLights::add(", "const std::vector<W3DShaderManager::PixelLight> &W3DEmissiveLights::finish(",
             "Vector3 W3DEmissiveLights::sampleTexture(", "Bool W3DEmissiveLights::sampleModel("]
    harness = PREAMBLE.replace("// PIXEL_LIGHT_STRUCT", pixel_light).replace("// EMISSIVE_HEADER", header)
    harness = harness.replace("// EMISSIVE_FUNCTIONS", "\n".join(function(emission, name) for name in names))
    harness = harness.replace("// ADD_OBJECT_FUNCTION", function(emission, "void W3DEmissiveLights::addObject("))
    harness = harness.replace("// LASER_FUNCTION", function((client / "W3DLaserGlow.cpp").read_text(encoding="utf-8"),
                              "void W3DLaserGlow::appendObjectLights("))
    harness = harness.replace("// PICK_FUNCTION", function(shader, "Int W3DShaderManager::pickPixelLights(const AABoxClass &box, Int *lights)")
                              + "\n" + function(shader, "Int W3DShaderManager::pickPixelLights(const AABoxClass &box, std::vector<Int> &lights)")
                              + "\n" + function(shader, "void W3DShaderManager::setPixelLights(")
                              + "\n" + function(shader, "void W3DShaderManager::filterPixelLights("))
    options = (ROOT / "GeneralsMD/Code/GameEngine/Source/GameClient/GUI/GUICallbacks/Menus/OptionsMenu.cpp").read_text(encoding="utf-8")
    scene = (ROOT / "GeneralsMD/Code/GameEngineDevice/Source/W3DDevice/GameClient/W3DScene.cpp").read_text(encoding="utf-8")
    tune = function(scene, "Bool Tune_Emissive_Light(", "\t")
    harness = harness.replace("// TUNE_FUNCTION", tune)
    harness = harness.replace("// OBJECT_LIGHTS", function(scene, "Int RTS3DScene::pickObjectPixelLights("))
    instancing = (ROOT / "Core/Libraries/Source/WWVegas/WW3D2/dx8instancing.cpp").read_text(encoding="utf-8")
    harness = harness.replace("// INSTANCED_DRAW", function(instancing, "static void Draw_Instances("))
    harness = harness.replace("// CHECKBOX_FUNCTION", function(options, "static GameWindow *createPathTracingCheck()")) + TESTS
    with tempfile.TemporaryDirectory(prefix="emissive-lights-", dir=ROOT / "build") as directory:
        work = Path(directory)
        (work / "test.cpp").write_text(harness, encoding="utf-8")
        (work / "run.bat").write_text(
            f'@echo off\ncall "{VCVARS}" x86 >nul\n'
            'if errorlevel 1 exit /b 1\n'
            'cl /nologo /EHsc /std:c++17 /Fe:test.exe test.cpp\n'
            'if errorlevel 1 exit /b 1\n'
            'test.exe\n', encoding="utf-8")
        subprocess.run(["cmd", "/d", "/c", str(work / "run.bat")], cwd=work, check=True)


if __name__ == "__main__":
    main()
