#pragma once
#include "AutoConfig.h"
#include "Renderer/SceneRenderer.h"
#include "Math/Color.h"
#include "Renderer/Batch.h"
#include "Renderer/PBRPipline.h"
#include "Particle/ParticleSystem.h"

#include "Component/LightComponent.h"
#include "Component/MeshComponent.h"
#include "Platform/PlatformDef.h"
#include "Adapter/Ptr.h"
#include "Renderer/RendererDef.h"
#include "Resource/Texture.h"

#include <bgfx/bgfx.h>
#include <stdint.h>
namespace Auto3D
{

class OMesh;
class AWorld;
class ACameraComponent;
class FEnvironmentPipline;
class FIBLPipline;
class FShadowPipline;
class FHDREnvPipline;

class FGeometry;

/// High-level rendering subsystem. Performs rendering of 3D scenes.
class AUTO_API FForwardShadingRenderer : public IRenderer
{
public:
	/// Construct and register subsystem.
	FForwardShadingRenderer();
	/// Destruct.
	~FForwardShadingRenderer();

	void Init()override;
	/// Render scene
	void Render()override;

	void RenderBatches();
	/// Categorize actors and the components they mount.
	void CollectActors(AWorld* world, ACameraComponent* camera);

	void CollectBatch();

	void AttachShader(FGeometryPass& pass);

	void PrepareView();

	void ShutDowm();

	void SetBackBufferSize(const TVector2F& size) { _backbufferSize = size; }
	void SetDebugMode(uint32_t debug) { _debug = debug; }
	void SetResetMode(uint32_t reset) { _reset = reset; }

	int GetInvisibleBatch() { return _invisibleBatch; }
	int GetVisibleBatch() { return _visibleBatch; }

	static FEnvironmentPipline s_environmentPipline;

	static FIBLPipline s_iblPipline;

	static FShadowPipline s_shadowPipline;

	static FPBRPipline s_pbrPipline;

	static OTexture* s_albedoMap;

	static OTexture* s_normalMap;
	
	static OTexture* s_metallicMap;

	static OTexture* s_roughnessMap;
	
	static OTexture* s_aoMap;
private:
	/// This function provides this renderer to calculate batches for use.
	void UpdateBatchesCount(FGeometry* geometry);
	
	EDepthImpl::Data _depthImpl;

	/// FBatch queues per pass.
	FRenderQueue _batchQueues;

	ACameraComponent* _currentCamera;

	/// Geometries in frustum.
	TVector<AActor*> _geometriesActor;
	/// Light actor.
	TVector<AActor*> _lightActor;

	TVector2F _backbufferSize;
	/// Renderer debug mode;
	uint32_t _debug;
	/// This flag specifies where flip occurs.
	uint32_t _reset;

	FColor _backbufferColor;
	float _depth;
	uint8_t _stencil;

	int _invisibleBatch;
	int _visibleBatch;
};

}

