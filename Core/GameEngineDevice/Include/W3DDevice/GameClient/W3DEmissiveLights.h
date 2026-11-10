#pragma once

#if defined(RTS_ZEROHOUR)
#include "W3DDevice/GameClient/W3DShaderManager.h"
#include "WWMath/matrix3d.h"
#include <map>
#include <vector>

class CameraClass;
class RenderObjClass;
class MeshModelClass;
class Vector2;

class W3DEmissiveLights
{
public:
	W3DEmissiveLights();
	~W3DEmissiveLights();
	void reset();
	void begin(CameraClass &camera);
	void addObject(RenderObjClass &object, Real opacity);
	const std::vector<W3DShaderManager::PixelLight> &finish(CameraClass &camera);

private:
	enum { SAMPLE_SIZE = 32 };
	struct TextureSamples
	{
		RefCountPtr<TextureClass> texture;
		Vector3 average;
		Vector3 pixels[SAMPLE_SIZE * SAMPLE_SIZE];
		Real alpha[SAMPLE_SIZE * SAMPLE_SIZE];
	};
	struct Sample
	{
		Vector3 position;
		Vector3 color;
		Vector3 mask;
		Real area;
	};
	struct ModelSamples
	{
		RefCountPtr<MeshModelClass> model;
		std::vector<Sample> samples;
	};
	struct Cluster
	{
		Cluster();
		Vector3 position;
		Vector3 color;
		Real weight;
		Real radius;
	};
	typedef std::pair<Int, std::pair<Int, Int> > Cell;
	typedef std::pair<MeshModelClass *, Bool> ModelKey;
	typedef std::map<Cell, Cluster> ClusterMap;

	/// A mesh's clusters at full opacity, good while its model, placement and mask intensity stay as they were.
	struct ObjectLights
	{
		ObjectLights();
		MeshModelClass *model;
		Bool alternate;
		Matrix3D transform;
		Real maskIntensity;
		UnsignedInt frame;	///< the last frame that drew it
		std::vector<std::pair<Cell, Cluster> > clusters;
	};
	TextureSamples *textureSamples(TextureClass *texture);
	static Vector3 sampleTexture(const TextureSamples *texture, const Vector2 &uv, Bool useAlpha = FALSE);
	Bool sampleModel(MeshModelClass &model, std::vector<Sample> &samples);
	static void add(ClusterMap &clusters, const Vector3 &position, const Vector3 &color, Real area, Real radius);

	std::map<TextureClass *, TextureSamples> m_textures;
	std::map<ModelKey, ModelSamples> m_models;
	std::map<RenderObjClass *, ObjectLights> m_objects;
	ClusterMap m_clusters;
	ClusterMap m_objectClusters;	///< one mesh's clusters while they are rebuilt
	std::vector<W3DShaderManager::PixelLight> m_lights;
	UnsignedInt m_frame;
};
#endif
