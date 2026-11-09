/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
**
**	This program is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

// FILE: W3DPlanarMirror.h ////////////////////////////////////////////////////////////////////////

#pragma once

#include "Lib/BaseType.h"
#include "WWMath/vector4.h"
#include "WW3D2/dx8compat.h"
#include "GameClient/PlanarMirrorShader.h"
#include <map>
#include <vector>

class CameraClass;
class MaterialPassClass;
class MeshClass;
class ShaderClass;
class TextureClass;

// The meshes a model's PlanarMirror keys pick out, drawn by planarmirror.hlsl as level mirrors.
class W3DPlanarMirrorManager
{
public:
	W3DPlanarMirrorManager();
	~W3DPlanarMirrorManager();

	/// Mirrors the scene in the planes of the mirrors the last frame showed, before the views draw.
	void renderReflections(CameraClass *camera);

	/// Starts the main scene's draws after its shadow map, the only ones that shade mirrors.
	void beginScenePass(Bool fresh = TRUE);
	/// Stops the shading at the scene's end, or for a draw inside it, and says whether to begin again without a fresh start.
	Bool suspendScenePass();

	/// The pass that lays a mirror's reflection over its opaque mesh, or null. Notes where every mirror the scene draws lies.
	/// A glass mesh's pass replaces the mesh, and draws after the opaque scene.
	MaterialPassClass *takeMirrorPass(MeshClass *mesh, Bool sorted, Bool &replaces);

	/// Shades a translucent mirror mesh for the soft particle hook, with its draw state applied. False draws it plain.
	Bool beginTranslucent(const ShaderClass &shader, const void *effectData);
	void endTranslucent();

	/// An opaque mesh's mirror, bound by its pass, as an overlay or as glass. Each polygon group hands over its own texture and shader.
	void beginOverlay(const PlanarMirrorShaderTuning *own, Bool glass);
	void setOverlayTexture(TextureClass *texture, const ShaderClass &shader);
	void endOverlay();

	void ReleaseResources();	///< drops the shaders and targets before a device reset; the next frame makes them again

private:
	enum { MAX_PLANES = 2, MAX_SIGHTINGS = 64 };

	// Mirrors the main scene drew at about one height, and the ground they cover.
	struct Sighting
	{
		Real z;
		Real minX;
		Real minY;
		Real maxX;
		Real maxY;
	};

	Bool isAvailable();
	Bool loadShaders();
	void noteSighting(MeshClass *mesh);
	Bool ensureTargets(UnsignedInt width, UnsignedInt height);
	void releaseTargets();
	Bool takeSceneCopy();
	TextureClass *findFrostTexture();
	void renderPlane(CameraClass *camera, Int plane, const Sighting &sighting, UnsignedInt width, UnsignedInt height);
	void bindShading(const PlanarMirrorShaderTuning *own, DWORD shader, Bool refract, Bool flag);
	void bindNormalMap(TextureClass *texture);
	void unbindShading(Bool refract);

	std::vector<Sighting> m_sightings;	///< what this frame's main scene drew, for the next frame's planes
	std::map<const PlanarMirrorShaderTuning *, MaterialPassClass *> m_passes;	///< one per model's settings
	PlanarMirrorShaderTuning m_tuning;	///< the bound draw's settings, resolved
	Vector4 m_mirrorParams;	///< the bound draw's c8, whose distortion waits for its texture
	IDirect3DTexture8 *m_mirrors[MAX_PLANES];	///< the scene mirrored in each plane, alpha 1 where anything drew
	IDirect3DSurface8 *m_depth;
	IDirect3DTexture8 *m_sceneCopy;	///< what lies behind the translucent mirrors
	TextureClass *m_white;
	TextureClass *m_frost;	///< rg crystal scatter, b cloud patches, a an angle per texel
	CameraClass *m_camera;
	DWORD m_overlayShader;
	DWORD m_refractShader;
	DWORD m_glassShader;	///< optional; without it glass meshes take the overlay
	Real m_planeZ[MAX_PLANES];
	Int m_planeCount;
	UnsignedInt m_planeFrame;	///< the render frame the planes were mirrored in
	Bool m_loaded;
	Bool m_sceneArmed;
	Bool m_sceneCopied;	///< taken at this scene pass's first glass or translucent mirror
	Bool m_overlayGlass;	///< the bound opaque mirror draws as glass
};

extern W3DPlanarMirrorManager *TheW3DPlanarMirrors;
