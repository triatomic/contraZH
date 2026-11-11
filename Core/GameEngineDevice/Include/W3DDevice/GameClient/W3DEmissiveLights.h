#pragma once

#if defined(RTS_ZEROHOUR)
#include "W3DDevice/GameClient/W3DShaderManager.h"
#include "WWMath/matrix3d.h"
#include <map>
#include <vector>

class CameraClass;
class RenderObjClass;
class MeshClass;
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
		Int group;
	};

	/// Samples that sit together and face one way, which see the same ground.
	struct Group
	{
		Vector3 origin;	///< the sample nearest their middle
		Real area;		///< their largest sample
	};
	struct ModelSamples
	{
		RefCountPtr<MeshModelClass> model;
		std::vector<Sample> samples;
		std::vector<Group> groups;
	};

	/// The light a group casts on the ground once its own object has blocked part of it.
	struct GroundFit
	{
		GroundFit();
		Vector3 offset;	///< from the sample to the middle of the lit ground, in the mesh's space
		Real scale;		///< brightness, and 0 lights no ground
		Real spread;	///< the lit ground's radius as a fraction of the unblocked one
	};
	struct Cluster
	{
		Cluster();
		Vector3 position;
		Vector3 color;
		Real weight;
		Real radius;
		Vector3 groundPosition;
		Vector3 groundColor;
		Real groundWeight;
		Real groundSpread;
		Bool fitted;	///< the ground takes a light of its own
	};

	/// An opaque mesh of the object being fitted.
	struct Blocker
	{
		MeshModelClass *model;
		Matrix3D worldToMesh;
		Vector3 center;
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
		Matrix3D pose;		///< its placement within the drawn object when the fits were made
		Real maxRadius;
		Real fitHeight;		///< its height over the ground when the fits were made
		UnsignedInt fitFrame;
		Bool refit;			///< it moved since its fits and waits for its turn or the frame's budget
		std::vector<GroundFit> fits;	///< one a group
	};
	TextureSamples *textureSamples(TextureClass *texture);
	static Vector3 sampleTexture(const TextureSamples *texture, const Vector2 &uv, Bool useAlpha = FALSE);
	Bool sampleModel(MeshModelClass &model, std::vector<Sample> &samples, std::vector<Group> &groups);
	static void add(ClusterMap &clusters, const Vector3 &position, const Vector3 &color, Real area, Real radius);
	static void add(ClusterMap &clusters, const Vector3 &position, const Vector3 &color, Real area, Real radius,
		const Vector3 &groundPosition, const GroundFit &fit);
	void addObject(RenderObjClass &object, Real opacity, RenderObjClass &root);
	void gatherBlockers(RenderObjClass &object);
	Bool isBlocked(const Vector3 &from, const Vector3 &to) const;
	void fitGround(RenderObjClass &root, const ModelSamples &model, const Matrix3D &transform, std::vector<GroundFit> &fits);
	void fitGroup(const Vector3 &origin, Real radius, Real ground, GroundFit &fit) const;

	std::map<TextureClass *, TextureSamples> m_textures;
	std::map<ModelKey, ModelSamples> m_models;
	std::map<RenderObjClass *, ObjectLights> m_objects;
	ClusterMap m_clusters;
	ClusterMap m_objectClusters;	///< one mesh's clusters while they are rebuilt
	std::vector<Blocker> m_blockers;
	RenderObjClass *m_blockerRoot;	///< the object m_blockers came from this frame
	std::vector<Vector3> m_groundOffsets;
	Int m_fitBudget;	///< the groups this frame may still fit
	std::vector<W3DShaderManager::PixelLight> m_lights;
	UnsignedInt m_frame;
};
#endif
