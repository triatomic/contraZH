#include "W3DDevice/GameClient/W3DEmissiveLights.h"

#if defined(RTS_ZEROHOUR)
#include "Common/GlobalData.h"
#include "GameClient/ParticleSys.h"
#include "W3DDevice/GameClient/W3DDisplay.h"
#include "W3DDevice/GameClient/W3DAssetManager.h"
#include "WW3D2/camera.h"
#include "WW3D2/hlod.h"
#include "WW3D2/mesh.h"
#include "WW3D2/meshmdl.h"
#include "WW3D2/surfaceclass.h"
#include "WW3D2/assetmgr.h"
#include "WW3D2/vertmaterial.h"

static const Real EMISSIVE_CELL_SIZE = 24.0f;

W3DEmissiveLights::W3DEmissiveLights()
{
}

W3DEmissiveLights::~W3DEmissiveLights()
{
}

W3DEmissiveLights::Cluster::Cluster()
	: position(0.0f, 0.0f, 0.0f), color(0.0f, 0.0f, 0.0f), weight(0.0f), radius(0.0f)
{
}

void W3DEmissiveLights::reset()
{
	m_models.clear();
	m_textures.clear();
	m_clusters.clear();
	m_lights.clear();
}

W3DEmissiveLights::TextureSamples *W3DEmissiveLights::textureSamples(TextureClass *texture)
{
	if (texture == nullptr)
	{
		return nullptr;
	}
	std::map<TextureClass *, TextureSamples>::iterator found = m_textures.find(texture);
	if (found != m_textures.end())
	{
		return &found->second;
	}
	texture->Init();
	if (!texture->Is_Initialized() || texture->Is_Missing_Texture())
	{
		return nullptr;
	}
	RefCountPtr<SurfaceClass> source;
	source.Assign_No_Add_Ref(texture->Get_Surface_Level());
	if (source == nullptr || !source->Is_Valid())
	{
		return nullptr;
	}
	SurfaceClass::SurfaceDescription desc;
	source->Get_Description(desc);
	RefCountPtr<SurfaceClass> sample;
	sample.Assign_No_Add_Ref(NEW_REF(SurfaceClass, (SAMPLE_SIZE, SAMPLE_SIZE, WW3D_FORMAT_A8R8G8B8)));
	if (!sample->Is_Valid())
	{
		return nullptr;
	}
	sample->Stretch_Copy(0, 0, SAMPLE_SIZE, SAMPLE_SIZE, 0, 0, desc.Width, desc.Height, source.Peek());
	Int pitch = 0;
	const unsigned char *bits = (const unsigned char *)sample->Lock(&pitch);
	if (bits == nullptr)
	{
		return nullptr;
	}
	TextureSamples &result = m_textures[texture];
	result.texture = texture;
	result.average.Set(0.0f, 0.0f, 0.0f);
	for (Int y = 0; y < SAMPLE_SIZE; y++)
	{
		for (Int x = 0; x < SAMPLE_SIZE; x++)
		{
			const unsigned char *pixel = bits + y * pitch + x * 4;
			result.pixels[y * SAMPLE_SIZE + x].Set(pixel[2] / 255.0f, pixel[1] / 255.0f, pixel[0] / 255.0f);
			result.average += result.pixels[y * SAMPLE_SIZE + x];
			result.alpha[y * SAMPLE_SIZE + x] = pixel[3] / 255.0f;
		}
	}
	result.average /= (Real)(SAMPLE_SIZE * SAMPLE_SIZE);
	sample->Unlock();
	return &result;
}

Vector3 W3DEmissiveLights::sampleTexture(const TextureSamples *texture, const Vector2 &uv, Bool useAlpha)
{
	if (texture == nullptr)
	{
		return Vector3(1.0f, 1.0f, 1.0f);
	}
	const Int x = min((Int)((uv.X - floorf(uv.X)) * (Real)SAMPLE_SIZE), SAMPLE_SIZE - 1);
	const Int y = min((Int)((uv.Y - floorf(uv.Y)) * (Real)SAMPLE_SIZE), SAMPLE_SIZE - 1);
	const Int index = y * SAMPLE_SIZE + x;
	return texture->pixels[index] * (useAlpha ? texture->alpha[index] : 1.0f);
}

Bool W3DEmissiveLights::sampleModel(MeshModelClass &model, std::vector<Sample> &samples)
{
	Bool ready = TRUE;
	const Vector3 *vertices = model.Get_Vertex_Array();
	const TriIndex *triangles = model.Get_Polygon_Array();
	for (Int pass = 0; pass < model.Get_Pass_Count(); pass++)
	{
		const Vector2 *uv = model.Get_UV_Array(pass);
		for (Int polygon = 0; polygon < model.Get_Polygon_Count(); polygon++)
		{
			const ShaderClass shader = model.Get_Shader(polygon, pass);
			const Bool additive = shader.Get_Dst_Blend_Func() == ShaderClass::DSTBLEND_ONE &&
				(shader.Get_Src_Blend_Func() == ShaderClass::SRCBLEND_ONE || shader.Get_Src_Blend_Func() == ShaderClass::SRCBLEND_SRC_ALPHA);
			const TriIndex &triangle = triangles[polygon];
			VertexMaterialClass *material = model.Peek_Material(triangle[0], pass);
			Vector3 emission(0.0f, 0.0f, 0.0f);
			if (material != nullptr)
			{
				material->Get_Emissive(&emission);
			}
			if (additive)
			{
				Vector3 diffuse(1.0f, 1.0f, 1.0f);
				if (material != nullptr)
				{
					material->Get_Diffuse(&diffuse);
				}
				emission += diffuse;
			}
			TextureClass *base = model.Peek_Texture(polygon, pass);
			TextureClass *mask = base != nullptr ? W3DShaderManager::findEmissiveMap(base) : nullptr;
			if (mask == nullptr && emission.Length2() <= 0.0001f)
			{
				continue;
			}
			TextureSamples *maskPixels = textureSamples(mask);
			TextureSamples *basePixels = emission.Length2() > 0.0001f ? textureSamples(base) : nullptr;
			if ((mask != nullptr && maskPixels == nullptr) || (base != nullptr && emission.Length2() > 0.0001f && basePixels == nullptr))
			{
				ready = FALSE;
				continue;
			}
			const Vector3 &a = vertices[triangle[0]];
			const Vector3 &b = vertices[triangle[1]];
			const Vector3 &c = vertices[triangle[2]];
			Vector3 normal;
			Vector3::Cross_Product(b - a, c - a, &normal);
			const Real area = normal.Length() * 0.5f;
			if (area <= 0.0001f)
			{
				continue;
			}
			normal /= area * 2.0f;
			for (Int tap = 0; tap < 4; tap++)
			{
				const Real wa = tap == 0 ? 0.7f : tap == 3 ? 1.0f / 3.0f : 0.15f;
				const Real wb = tap == 1 ? 0.7f : tap == 3 ? 1.0f / 3.0f : 0.15f;
				const Real wc = 1.0f - wa - wb;
				const Vector2 texcoord = uv != nullptr ? uv[triangle[0]] * wa + uv[triangle[1]] * wb + uv[triangle[2]] * wc : Vector2(0.0f, 0.0f);
				Sample sample;
				sample.position = a * wa + b * wb + c * wc + normal * 2.0f;
				sample.color = Vector3(0.0f, 0.0f, 0.0f);
				if (emission.Length2() > 0.0001f)
				{
					const Vector3 texel = sampleTexture(basePixels, texcoord, shader.Get_Src_Blend_Func() == ShaderClass::SRCBLEND_SRC_ALPHA);
					sample.color.Set(texel.X * emission.X, texel.Y * emission.Y, texel.Z * emission.Z);
					if (material != nullptr)
					{
						sample.color *= material->Get_Opacity();
					}
				}
				sample.mask = maskPixels != nullptr ? sampleTexture(maskPixels, texcoord) : Vector3(0.0f, 0.0f, 0.0f);
				sample.area = area * 0.25f;
				if (sample.color.Length2() + sample.mask.Length2() > 0.0001f)
				{
					samples.push_back(sample);
				}
			}
		}
	}
	return ready;
}

void W3DEmissiveLights::add(const Vector3 &position, const Vector3 &color, Real area, Real radius)
{
	const Real brightness = max(color.X, max(color.Y, color.Z));
	if (brightness <= 0.001f || area <= 0.0f || radius <= 0.0f)
	{
		return;
	}
	const Cell cell((Int)floorf(position.X / EMISSIVE_CELL_SIZE), std::make_pair(
		(Int)floorf(position.Y / EMISSIVE_CELL_SIZE), (Int)floorf(position.Z / EMISSIVE_CELL_SIZE)));
	Cluster &cluster = m_clusters[cell];
	cluster.position += position * (area * brightness);
	cluster.color += color * area;
	cluster.weight += area * brightness;
	cluster.radius = max(cluster.radius, radius);
}

void W3DEmissiveLights::begin(CameraClass &camera)
{
	m_clusters.clear();
	m_lights.clear();
	if (TheParticleSystemManager == nullptr)
	{
		return;
	}
	ParticleSystemManager::ParticleSystemList &systems = TheParticleSystemManager->getAllParticleSystems();
	for (ParticleSystemManager::ParticleSystemListIt it = systems.begin(); it != systems.end(); ++it)
	{
		ParticleSystem *system = *it;
		if (system == nullptr || system->isUsingDrawables() || system->isUsingSmudge() || system->getShaderType() != ParticleSystemInfo::ADDITIVE)
		{
			continue;
		}
		RefCountPtr<TextureClass> texture;
		texture.Assign_No_Add_Ref(W3DDisplay::m_assetManager->Get_Texture(system->getParticleTypeName().str()));
		TextureSamples *pixels = textureSamples(texture.Peek());
		if (pixels == nullptr)
		{
			continue;
		}
		const Vector3 &tint = pixels->average;
		for (Particle *particle = system->getFirstParticle(); particle != nullptr; particle = particle->m_systemNext)
		{
			const Coord3D *pos = particle->getPosition();
			const Vector3 position(pos->x, pos->y, pos->z);
			const Real size = particle->getSize();
			const Real radius = min(max(size * 3.0f, EMISSIVE_CELL_SIZE), 120.0f);
			if (size <= 0.0f || camera.Cull_Sphere(SphereClass(position, radius)))
			{
				continue;
			}
			const RGBColor *color = particle->getColor();
			const Vector3 emission(color->red * tint.X, color->green * tint.Y, color->blue * tint.Z);
			add(position, emission * WWMath::Clamp(particle->getAlpha(), 0.0f, 1.0f), size * size, radius);
		}
	}
}

void W3DEmissiveLights::addObject(RenderObjClass &object, Real opacity)
{
	if (!object.Is_Not_Hidden_At_All() || opacity <= 0.0f)
	{
		return;
	}
	if (object.Class_ID() == RenderObjClass::CLASSID_HLOD)
	{
		HLodClass &hlod = (HLodClass &)object;
		const Int lod = hlod.Get_LOD_Level();
		for (Int i = 0; i < hlod.Get_Lod_Model_Count(lod); i++)
		{
			addObject(*hlod.Peek_Lod_Model(lod, i), opacity);
		}
		for (Int i = 0; i < hlod.Get_Additional_Model_Count(); i++)
		{
			addObject(*hlod.Peek_Additional_Model(i), opacity);
		}
		return;
	}
	if (object.Class_ID() != RenderObjClass::CLASSID_MESH)
	{
		for (Int i = 0; i < object.Get_Num_Sub_Objects(); i++)
		{
			RenderObjClass *child = object.Get_Sub_Object(i);
			if (child != nullptr)
			{
				addObject(*child, opacity);
				child->Release_Ref();
			}
		}
		return;
	}
	MeshModelClass *model = ((MeshClass &)object).Peek_Model();
	if (model == nullptr || model->Get_Flag(MeshGeometryClass::SKIN))
	{
		return;
	}
	const ModelKey key(model, model->Is_Alternate_Material_Description_Enabled());
	std::map<ModelKey, ModelSamples>::iterator found = m_models.find(key);
	if (found == m_models.end())
	{
		ModelSamples entry;
		entry.model = model;
		if (!sampleModel(*model, entry.samples))
		{
			return;
		}
		found = m_models.insert(std::make_pair(key, entry)).first;
	}
	const Matrix3D &transform = object.Get_Transform();
	Vector3 axis;
	transform.Get_X_Vector(&axis);
	const Real scale = axis.Length();
	const Real maskIntensity = !TheGlobalData->m_useSpecular ? 0.0f : TheGlobalData->m_timeOfDay == TIME_OF_DAY_NIGHT
		? TheGlobalData->m_unitEmissiveNightIntensity : TheGlobalData->m_unitEmissiveIntensity;
	for (size_t i = 0; i < found->second.samples.size(); i++)
	{
		const Sample &sample = found->second.samples[i];
		Vector3 position;
		Matrix3D::Transform_Vector(transform, sample.position, &position);
		const Real area = sample.area * scale * scale;
		add(position, (sample.color + sample.mask * maskIntensity) * opacity, area, min(max(sqrtf(area) * 3.0f, EMISSIVE_CELL_SIZE), 120.0f));
	}
}

const std::vector<W3DShaderManager::PixelLight> &W3DEmissiveLights::finish(CameraClass &camera)
{
	for (std::map<Cell, Cluster>::const_iterator it = m_clusters.begin(); it != m_clusters.end(); ++it)
	{
		const Cluster &cluster = it->second;
		W3DShaderManager::PixelLight light;
		light.position = cluster.position / cluster.weight;
		light.outerRadius = cluster.radius;
		if (camera.Cull_Sphere(SphereClass(light.position, light.outerRadius)))
		{
			continue;
		}
		const Real strength = min(sqrtf(cluster.weight) / 8.0f, 1.5f);
		light.diffuse = cluster.color * (strength / cluster.weight);
		light.innerRadius = 0.5f;
		light.ambientScale = 0.1f;
		light.terrainOnly = FALSE;
		DEBUG_ASSERTCRASH(light.outerRadius > light.innerRadius && light.diffuse.Length2() > 0.0f, ("Invalid emissive light"));
		m_lights.push_back(light);
	}
	return m_lights;
}
#endif
