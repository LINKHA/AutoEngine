#include "Renderer/ShadowPipline.h"
#include "Platform/ProcessWindow.h"
#include "Component/TransformComponent.h"
#include "Gameplay/Actor.h"

#include <bx/timer.h>

namespace Auto3D
{
IMPLEMENT_SINGLETON(FShadowPipline)

bgfx::UniformHandle FShadowPipline::us_texColor;
bgfx::UniformHandle FShadowPipline::us_shadowMap[ShadowMapRenderTargets::Count];
FShadowMapSettings FShadowPipline::s_shadowMapSettings[ELightType::Count][EDepthImpl::Count][EShadowMapImpl::Count];
FShadowSceneSettings FShadowPipline::s_shadowSceneSettings;
uint16_t FShadowPipline::s_currentShadowMapSize;
bgfx::FrameBufferHandle FShadowPipline::s_rtShadowMap[ShadowMapRenderTargets::Count];
bgfx::FrameBufferHandle FShadowPipline::s_rtBlur;

Material FShadowPipline::s_defaultMaterial;

TMatrix4x4F FShadowPipline::s_lightMtx;
TMatrix4x4F FShadowPipline::s_shadowMapMtx[ShadowMapRenderTargets::Count];

ClearValues FShadowPipline::s_clearValues;

TMatrix4x4F FShadowPipline::s_lightView[4];
TMatrix4x4F FShadowPipline::s_lightProj[4];
TMatrix4x4F FShadowPipline::s_mtxYpr[4];


void mtxBillboard(float* __restrict _result
	, const float* __restrict _view
	, const float* __restrict _pos
	, const float* __restrict _scale)
{
	_result[0] = _view[0] * _scale[0];
	_result[1] = _view[4] * _scale[0];
	_result[2] = _view[8] * _scale[0];
	_result[3] = 0.0f;
	_result[4] = _view[1] * _scale[1];
	_result[5] = _view[5] * _scale[1];
	_result[6] = _view[9] * _scale[1];
	_result[7] = 0.0f;
	_result[8] = _view[2] * _scale[2];
	_result[9] = _view[6] * _scale[2];
	_result[10] = _view[10] * _scale[2];
	_result[11] = 0.0f;
	_result[12] = _pos[0];
	_result[13] = _pos[1];
	_result[14] = _pos[2];
	_result[15] = 1.0f;
}

void mtxYawPitchRoll(float* __restrict _result
	, float _yaw
	, float _pitch
	, float _roll
)
{
	float sroll = bx::sin(_roll);
	float croll = bx::cos(_roll);
	float spitch = bx::sin(_pitch);
	float cpitch = bx::cos(_pitch);
	float syaw = bx::sin(_yaw);
	float cyaw = bx::cos(_yaw);

	_result[0] = sroll * spitch * syaw + croll * cyaw;
	_result[1] = sroll * cpitch;
	_result[2] = sroll * spitch * cyaw - croll * syaw;
	_result[3] = 0.0f;
	_result[4] = croll * spitch * syaw - sroll * cyaw;
	_result[5] = croll * cpitch;
	_result[6] = croll * spitch * cyaw + sroll * syaw;
	_result[7] = 0.0f;
	_result[8] = cpitch * syaw;
	_result[9] = -spitch;
	_result[10] = cpitch * cyaw;
	_result[11] = 0.0f;
	_result[12] = 0.0f;
	_result[13] = 0.0f;
	_result[14] = 0.0f;
	_result[15] = 1.0f;
}

void worldSpaceFrustumCorners(float* _corners24f
	, float _near
	, float _far
	, float _projWidth
	, float _projHeight
	, const float* __restrict _invViewMtx
)
{
	// Define frustum corners in view space.
	const float nw = _near * _projWidth;
	const float nh = _near * _projHeight;
	const float fw = _far * _projWidth;
	const float fh = _far * _projHeight;

	const uint8_t numCorners = 8;
	const bx::Vec3 corners[numCorners] =
	{
		{ -nw,  nh, _near },
		{  nw,  nh, _near },
		{  nw, -nh, _near },
		{ -nw, -nh, _near },
		{ -fw,  fh, _far  },
		{  fw,  fh, _far  },
		{  fw, -fh, _far  },
		{ -fw, -fh, _far  },
	};

	// Convert them to world space.
	float(*out)[3] = (float(*)[3])_corners24f;
	for (uint8_t ii = 0; ii < numCorners; ++ii)
	{
		bx::store(&out[ii], bx::mul(corners[ii], _invViewMtx));
	}
}

/**
 * _splits = { near0, far0, near1, far1... nearN, farN }
 * N = _numSplits
 */
void splitFrustum(float* _splits, uint8_t _numSplits, float _near, float _far, float _splitWeight)
{
	const float l = _splitWeight;
	const float ratio = _far / _near;
	const int8_t numSlices = _numSplits * 2;
	const float numSlicesf = float(numSlices);

	// First slice.
	_splits[0] = _near;

	for (uint8_t nn = 2, ff = 1; nn < numSlices; nn += 2, ff += 2)
	{
		float si = float(int8_t(ff)) / numSlicesf;

		const float nearp = l * (_near*bx::pow(ratio, si)) + (1 - l)*(_near + (_far - _near)*si);
		_splits[nn] = nearp;          //near
		_splits[ff] = nearp * 1.005f; //far from previous split
	}

	// Last slice.
	_splits[numSlices - 1] = _far;
}


FShadowPipline::FShadowPipline() :
	_shadowMapImpl(EShadowMapImpl::VSM)
{
	
}
FShadowPipline::~FShadowPipline() 
{}
void FShadowPipline::Init()
{
	GProcessWindow& processWindow = GProcessWindow::Get();

	FShadowPipline::s_clearValues = ClearValues(0x00000000, 1.0f, 0);

	// Uniforms.
	_uniforms.init();

	us_texColor = bgfx::createUniform("s_texColor", bgfx::UniformType::Sampler);
	us_shadowMap[0] = bgfx::createUniform("s_shadowMap0", bgfx::UniformType::Sampler);
	us_shadowMap[1] = bgfx::createUniform("s_shadowMap1", bgfx::UniformType::Sampler);
	us_shadowMap[2] = bgfx::createUniform("s_shadowMap2", bgfx::UniformType::Sampler);
	us_shadowMap[3] = bgfx::createUniform("s_shadowMap3", bgfx::UniformType::Sampler);

	// Programs.
	_programs.Init();

	FShadowMapSettings smSettings[ELightType::Count][EDepthImpl::Count][EShadowMapImpl::Count]=
	{
		{ //LightType::Spot

			{ //EDepthImpl::InvZ

				{ //EShadowMapImpl::Hard
					10.0f, 7.0f, 12.0f, 1.0f         // m_sizePwrTwo
					, 10.0f, 1.0f, 20.0f, 1.0f         // m_depthValuePow
					, 1.0f, 1.0f, 10.0f, 1.0f          // m_near
					, 250.0f, 100.0f, 2000.0f, 50.0f   // m_far
					, 0.0035f, 0.0f, 0.01f, 0.00001f   // m_bias
					, 0.0012f, 0.0f, 0.05f, 0.00001f   // m_normalOffset
					, 0.7f, 0.0f, 1.0f, 0.01f          // m_customParam0
					, 500.0f, 1.0f, 1000.0f, 1.0f      // m_customParam1
					, 2.0f, 0.0f, 4.0f, 1.0f           // m_xNum
					, 2.0f, 0.0f, 4.0f, 1.0f           // m_yNum
					, 1.0f, 0.0f, 3.0f, 0.01f          // m_xOffset
					, 1.0f, 0.0f, 3.0f, 0.01f          // m_yOffset
					, true                             // m_doBlur
					, &_programs._packDepth[EDepthImpl::InvZ][EPackDepth::RGBA][ERenderInstanceType::STAIC] //m_progPack
					, &_programs._packDepth[EDepthImpl::InvZ][EPackDepth::RGBA][ERenderInstanceType::INSTANCE] //m_progPackInstace
					, &_programs._colorLighting[EShadowMapType::Single][EDepthImpl::InvZ][EShadowMapImpl::Hard][ERenderInstanceType::STAIC] //m_progDraw
					, &_programs._colorLighting[EShadowMapType::Single][EDepthImpl::InvZ][EShadowMapImpl::Hard][ERenderInstanceType::INSTANCE] //m_progDrawInstace
				},
				{ //EShadowMapImpl::PCF
					10.0f, 7.0f, 12.0f, 1.0f         // m_sizePwrTwo
					, 10.0f, 1.0f, 20.0f, 1.0f         // m_depthValuePow
					, 1.0f, 1.0f, 99.0f, 1.0f          // m_near
					, 250.0f, 100.0f, 2000.0f, 50.0f   // m_far
					, 0.007f, 0.0f, 0.01f, 0.00001f    // m_bias
					, 0.001f, 0.0f, 0.05f, 0.00001f    // m_normalOffset
					, 0.7f, 0.0f, 1.0f, 0.01f          // m_customParam0
					, 500.0f, 1.0f, 1000.0f, 1.0f      // m_customParam1
					, 2.0f, 0.0f, 8.0f, 1.0f           // m_xNum
					, 2.0f, 0.0f, 8.0f, 1.0f           // m_yNum
					, 1.0f, 0.0f, 3.0f, 0.01f          // m_xOffset
					, 1.0f, 0.0f, 3.0f, 0.01f          // m_yOffset
					, true                             // m_doBlur
					, &_programs._packDepth[EDepthImpl::InvZ][EPackDepth::RGBA][ERenderInstanceType::STAIC] //m_progPack
					, &_programs._packDepth[EDepthImpl::InvZ][EPackDepth::RGBA][ERenderInstanceType::INSTANCE] //m_progPackInstance
					, &_programs._colorLighting[EShadowMapType::Single][EDepthImpl::InvZ][EShadowMapImpl::PCF][ERenderInstanceType::STAIC] //m_progDraw
					, &_programs._colorLighting[EShadowMapType::Single][EDepthImpl::InvZ][EShadowMapImpl::PCF][ERenderInstanceType::STAIC] //m_progDrawInstance
				},
				{ //EShadowMapImpl::VSM
					10.0f, 7.0f, 12.0f, 1.0f         // m_sizePwrTwo
					, 10.0f, 1.0f, 20.0f, 1.0f         // m_depthValuePow
					, 8.0f, 1.0f, 10.0f, 1.0f          // m_near
					, 250.0f, 100.0f, 2000.0f, 50.0f   // m_far
					, 0.045f, 0.0f, 0.1f, 0.00001f     // m_bias
					, 0.001f, 0.0f, 0.05f, 0.00001f    // m_normalOffset
					, 0.02f, 0.0f, 0.04f, 0.00001f     // m_customParam0
					, 450.0f, 1.0f, 1000.0f, 1.0f      // m_customParam1
					, 2.0f, 0.0f, 4.0f, 1.0f           // m_xNum
					, 2.0f, 0.0f, 4.0f, 1.0f           // m_yNum
					, 1.0f, 0.0f, 3.0f, 0.01f          // m_xOffset
					, 1.0f, 0.0f, 3.0f, 0.01f          // m_yOffset
					, true                             // m_doBlur
					, &_programs._packDepth[EDepthImpl::InvZ][EPackDepth::VSM][ERenderInstanceType::STAIC] //m_progPack
					, &_programs._packDepth[EDepthImpl::InvZ][EPackDepth::VSM][ERenderInstanceType::INSTANCE] //m_progPackInstance
					, &_programs._colorLighting[EShadowMapType::Single][EDepthImpl::InvZ][EShadowMapImpl::VSM][ERenderInstanceType::STAIC] //m_progDraw
					, &_programs._colorLighting[EShadowMapType::Single][EDepthImpl::InvZ][EShadowMapImpl::VSM][ERenderInstanceType::INSTANCE] //m_progDrawInstance
				},
				{ //EShadowMapImpl::ESM
					10.0f, 7.0f, 12.0f, 1.0f         // m_sizePwrTwo
					, 10.0f, 1.0f, 20.0f, 1.0f         // m_depthValuePow
					, 3.0f, 1.0f, 10.0f, 0.01f         // m_near
					, 250.0f, 100.0f, 2000.0f, 50.0f   // m_far
					, 0.02f, 0.0f, 0.3f, 0.00001f      // m_bias
					, 0.001f, 0.0f, 0.05f, 0.00001f    // m_normalOffset
					, 0.7f, 0.0f, 1.0f, 0.01f          // m_customParam0
					, 9000.0f, 1.0f, 15000.0f, 1.0f    // m_customParam1
					, 2.0f, 0.0f, 4.0f, 1.0f           // m_xNum
					, 2.0f, 0.0f, 4.0f, 1.0f           // m_yNum
					, 1.0f, 0.0f, 3.0f, 0.01f          // m_xOffset
					, 1.0f, 0.0f, 3.0f, 0.01f          // m_yOffset
					, true                             // m_doBlur
					, &_programs._packDepth[EDepthImpl::InvZ][EPackDepth::RGBA][ERenderInstanceType::STAIC] //m_progPack
					, &_programs._packDepth[EDepthImpl::InvZ][EPackDepth::RGBA][ERenderInstanceType::INSTANCE] //m_progPackInstance
					, &_programs._colorLighting[EShadowMapType::Single][EDepthImpl::InvZ][EShadowMapImpl::ESM][ERenderInstanceType::STAIC] //m_progDraw
					, &_programs._colorLighting[EShadowMapType::Single][EDepthImpl::InvZ][EShadowMapImpl::ESM][ERenderInstanceType::INSTANCE] //m_progDrawInstance
				}

			},
			{ //EDepthImpl::Linear

				{ //EShadowMapImpl::Hard
					10.0f, 7.0f, 12.0f, 1.0f         // m_sizePwrTwo
					, 1.0f, 1.0f, 20.0f, 1.0f          // m_depthValuePow
					, 1.0f, 1.0f, 10.0f, 1.0f          // m_near
					, 250.0f, 100.0f, 2000.0f, 50.0f   // m_far
					, 0.0025f, 0.0f, 0.01f, 0.00001f   // m_bias
					, 0.0012f, 0.0f, 0.05f, 0.00001f   // m_normalOffset
					, 0.7f, 0.0f, 1.0f, 0.01f          // m_customParam0
					, 500.0f, 1.0f, 1000.0f, 1.0f      // m_customParam1
					, 2.0f, 0.0f, 4.0f, 1.0f           // m_xNum
					, 2.0f, 0.0f, 4.0f, 1.0f           // m_yNum
					, 1.0f, 0.0f, 3.0f, 0.01f          // m_xOffset
					, 1.0f, 0.0f, 3.0f, 0.01f          // m_yOffset
					, true                             // m_doBlur
					, &_programs._packDepth[EDepthImpl::Linear][EPackDepth::RGBA][ERenderInstanceType::STAIC] //m_progPack
					, &_programs._packDepth[EDepthImpl::Linear][EPackDepth::RGBA][ERenderInstanceType::INSTANCE] //m_progPackInstance
					, &_programs._colorLighting[EShadowMapType::Single][EDepthImpl::Linear][EShadowMapImpl::Hard][ERenderInstanceType::STAIC] //m_progDraw
					, &_programs._colorLighting[EShadowMapType::Single][EDepthImpl::Linear][EShadowMapImpl::Hard][ERenderInstanceType::INSTANCE] //m_progDrawInstance
				},
				{ //EShadowMapImpl::PCF
					10.0f, 7.0f, 12.0f, 1.0f         // m_sizePwrTwo
					, 1.0f, 1.0f, 20.0f, 1.0f          // m_depthValuePow
					, 1.0f, 1.0f, 99.0f, 1.0f          // m_near
					, 250.0f, 100.0f, 2000.0f, 50.0f   // m_far
					, 0.0025f, 0.0f, 0.01f, 0.00001f   // m_bias
					, 0.001f, 0.0f, 0.05f,  0.00001f   // m_normalOffset
					, 0.7f, 0.0f, 1.0f, 0.01f          // m_customParam0
					, 2000.0f, 1.0f, 2000.0f, 1.0f     // m_customParam1
					, 2.0f, 0.0f, 8.0f, 1.0f           // m_xNum
					, 2.0f, 0.0f, 8.0f, 1.0f           // m_yNum
					, 1.0f, 0.0f, 3.0f, 0.01f          // m_xOffset
					, 1.0f, 0.0f, 3.0f, 0.01f          // m_yOffset
					, true                             // m_doBlur
					, &_programs._packDepth[EDepthImpl::Linear][EPackDepth::RGBA][ERenderInstanceType::STAIC] //m_progPack
					, &_programs._packDepth[EDepthImpl::Linear][EPackDepth::RGBA][ERenderInstanceType::INSTANCE] //m_progPackInstance
					, &_programs._colorLighting[EShadowMapType::Single][EDepthImpl::Linear][EShadowMapImpl::PCF][ERenderInstanceType::STAIC] //m_progDraw
					, &_programs._colorLighting[EShadowMapType::Single][EDepthImpl::Linear][EShadowMapImpl::PCF][ERenderInstanceType::INSTANCE] //m_progDrawInstance
				},
				{ //EShadowMapImpl::VSM
					10.0f, 7.0f, 12.0f, 1.0f         // m_sizePwrTwo
					, 1.0f, 1.0f, 20.0f, 1.0f          // m_depthValuePow
					, 1.0f, 1.0f, 10.0f, 1.0f          // m_near
					, 250.0f, 100.0f, 2000.0f, 50.0f   // m_far
					, 0.006f, 0.0f, 0.01f, 0.00001f    // m_bias
					, 0.001f, 0.0f, 0.05f, 0.00001f    // m_normalOffset
					, 0.02f, 0.0f, 0.1f, 0.00001f      // m_customParam0
					, 300.0f, 1.0f, 1500.0f, 1.0f      // m_customParam1
					, 2.0f, 0.0f, 4.0f, 1.0f           // m_xNum
					, 2.0f, 0.0f, 4.0f, 1.0f           // m_yNum
					, 1.0f, 0.0f, 3.0f, 0.01f          // m_xOffset
					, 1.0f, 0.0f, 3.0f, 0.01f          // m_yOffset
					, true                             // m_doBlur
					, &_programs._packDepth[EDepthImpl::Linear][EPackDepth::VSM][ERenderInstanceType::STAIC] //m_progPack
					, &_programs._packDepth[EDepthImpl::Linear][EPackDepth::VSM][ERenderInstanceType::INSTANCE] //m_progPackInstance
					, &_programs._colorLighting[EShadowMapType::Single][EDepthImpl::Linear][EShadowMapImpl::VSM][ERenderInstanceType::STAIC] //m_progDraw
					, &_programs._colorLighting[EShadowMapType::Single][EDepthImpl::Linear][EShadowMapImpl::VSM][ERenderInstanceType::INSTANCE] //m_progDrawInstance
				},
				{ //EShadowMapImpl::ESM
					10.0f, 7.0f, 12.0f, 1.0f         // m_sizePwrTwo
					, 1.0f, 1.0f, 20.0f, 1.0f          // m_depthValuePow
					, 1.0f, 1.0f, 10.0f, 0.01f         // m_near
					, 250.0f, 100.0f, 2000.0f, 50.0f   // m_far
					, 0.0055f, 0.0f, 0.01f, 0.00001f   // m_bias
					, 0.001f, 0.0f, 0.05f, 0.00001f    // m_normalOffset
					, 0.7f, 0.0f, 1.0f, 0.01f          // m_customParam0
					, 2500.0f, 1.0f, 5000.0f, 1.0f     // m_customParam1
					, 2.0f, 0.0f, 4.0f, 1.0f           // m_xNum
					, 2.0f, 0.0f, 4.0f, 1.0f           // m_yNum
					, 1.0f, 0.0f, 3.0f, 0.01f          // m_xOffset
					, 1.0f, 0.0f, 3.0f, 0.01f          // m_yOffset
					, true                             // m_doBlur
					, &_programs._packDepth[EDepthImpl::Linear][EPackDepth::RGBA][ERenderInstanceType::STAIC] //m_progPack
					, &_programs._packDepth[EDepthImpl::Linear][EPackDepth::RGBA][ERenderInstanceType::INSTANCE] //m_progPackInstance
					, &_programs._colorLighting[EShadowMapType::Single][EDepthImpl::Linear][EShadowMapImpl::ESM][ERenderInstanceType::STAIC] //m_progDraw
					, &_programs._colorLighting[EShadowMapType::Single][EDepthImpl::Linear][EShadowMapImpl::ESM][ERenderInstanceType::INSTANCE] //m_progDrawInstance
				}

			}

		},
		{ //LightType::Point

			{ //EDepthImpl::InvZ

				{ //EShadowMapImpl::Hard
					12.0f, 9.0f, 12.0f, 1.0f         // m_sizePwrTwo
					, 10.0f, 1.0f, 20.0f, 1.0f         // m_depthValuePow
					, 1.0f, 1.0f, 10.0f, 1.0f          // m_near
					, 250.0f, 100.0f, 2000.0f, 50.0f   // m_far
					, 0.006f, 0.0f, 0.01f, 0.00001f    // m_bias
					, 0.001f, 0.0f, 0.05f, 0.00001f    // m_normalOffset
					, 0.7f, 0.0f, 1.0f, 0.01f          // m_customParam0
					, 50.0f, 1.0f, 300.0f, 1.0f        // m_customParam1
					, 2.0f, 0.0f, 4.0f, 1.0f           // m_xNum
					, 2.0f, 0.0f, 4.0f, 1.0f           // m_yNum
					, 0.25f, 0.0f, 2.0f, 0.001f        // m_xOffset
					, 0.25f, 0.0f, 2.0f, 0.001f        // m_yOffset
					, true                             // m_doBlur
					, &_programs._packDepth[EDepthImpl::InvZ][EPackDepth::RGBA][ERenderInstanceType::STAIC] //m_progPack
					, &_programs._packDepth[EDepthImpl::InvZ][EPackDepth::RGBA][ERenderInstanceType::INSTANCE] //m_progPackInstance
					, &_programs._colorLighting[EShadowMapType::Omni][EDepthImpl::InvZ][EShadowMapImpl::Hard][ERenderInstanceType::STAIC] //m_progDraw
					, &_programs._colorLighting[EShadowMapType::Omni][EDepthImpl::InvZ][EShadowMapImpl::Hard][ERenderInstanceType::INSTANCE] //m_progDrawInstance
				},
				{ //EShadowMapImpl::PCF
					12.0f, 9.0f, 12.0f, 1.0f         // m_sizePwrTwo
					, 10.0f, 1.0f, 20.0f, 1.0f         // m_depthValuePow
					, 1.0f, 1.0f, 99.0f, 1.0f          // m_near
					, 250.0f, 100.0f, 2000.0f, 50.0f   // m_far
					, 0.004f, 0.0f, 0.01f, 0.00001f    // m_bias
					, 0.001f, 0.0f, 0.05f, 0.00001f    // m_normalOffset
					, 0.7f, 0.0f, 1.0f, 0.01f          // m_customParam0
					, 50.0f, 1.0f, 300.0f, 1.0f        // m_customParam1
					, 2.0f, 0.0f, 8.0f, 1.0f           // m_xNum
					, 2.0f, 0.0f, 8.0f, 1.0f           // m_yNum
					, 1.0f, 0.0f, 3.0f, 0.001f         // m_xOffset
					, 1.0f, 0.0f, 3.0f, 0.001f         // m_yOffset
					, true                             // m_doBlur
					, &_programs._packDepth[EDepthImpl::InvZ][EPackDepth::RGBA][ERenderInstanceType::STAIC] //m_progPack
					, &_programs._packDepth[EDepthImpl::InvZ][EPackDepth::RGBA][ERenderInstanceType::INSTANCE] //m_progPackInstance
					, &_programs._colorLighting[EShadowMapType::Omni][EDepthImpl::InvZ][EShadowMapImpl::PCF][ERenderInstanceType::STAIC] //m_progDraw
					, &_programs._colorLighting[EShadowMapType::Omni][EDepthImpl::InvZ][EShadowMapImpl::PCF][ERenderInstanceType::INSTANCE] //m_progDrawInstance
				},
				{ //EShadowMapImpl::VSM
					12.0f, 9.0f, 12.0f, 1.0f         // m_sizePwrTwo
					, 10.0f, 1.0f, 20.0f, 1.0f         // m_depthValuePow
					, 8.0f, 1.0f, 10.0f, 1.0f          // m_near
					, 250.0f, 100.0f, 2000.0f, 50.0f   // m_far
					, 0.055f, 0.0f, 0.1f, 0.00001f     // m_bias
					, 0.001f, 0.0f, 0.05f, 0.00001f    // m_normalOffset
					, 0.02f, 0.0f, 0.04f, 0.00001f     // m_customParam0
					, 450.0f, 1.0f, 900.0f, 1.0f       // m_customParam1
					, 2.0f, 0.0f, 4.0f, 1.0f           // m_xNum
					, 2.0f, 0.0f, 4.0f, 1.0f           // m_yNum
					, 0.25f, 0.0f, 2.0f, 0.001f        // m_xOffset
					, 0.25f, 0.0f, 2.0f, 0.001f        // m_yOffset
					, true                             // m_doBlur
					, &_programs._packDepth[EDepthImpl::InvZ][EPackDepth::VSM][ERenderInstanceType::STAIC] //m_progPack
					, &_programs._packDepth[EDepthImpl::InvZ][EPackDepth::VSM][ERenderInstanceType::INSTANCE] //m_progPackInstance
					, &_programs._colorLighting[EShadowMapType::Omni][EDepthImpl::InvZ][EShadowMapImpl::VSM][ERenderInstanceType::STAIC] //m_progDraw
					, &_programs._colorLighting[EShadowMapType::Omni][EDepthImpl::InvZ][EShadowMapImpl::VSM][ERenderInstanceType::INSTANCE] //m_progDrawInstance
				},
				{ //EShadowMapImpl::ESM
					12.0f, 9.0f, 12.0f, 1.0f         // m_sizePwrTwo
					, 10.0f, 1.0f, 20.0f, 1.0f         // m_depthValuePow
					, 3.0f, 1.0f, 10.0f, 0.01f         // m_near
					, 250.0f, 100.0f, 2000.0f, 50.0f   // m_far
					, 0.035f, 0.0f, 0.1f, 0.00001f     // m_bias
					, 0.001f, 0.0f, 0.05f, 0.00001f    // m_normalOffset
					, 0.7f, 0.0f, 1.0f, 0.01f          // m_customParam0
					, 9000.0f, 1.0f, 15000.0f, 1.0f    // m_customParam1
					, 2.0f, 0.0f, 4.0f, 1.0f           // m_xNum
					, 2.0f, 0.0f, 4.0f, 1.0f           // m_yNum
					, 0.25f, 0.0f, 2.0f, 0.001f        // m_xOffset
					, 0.25f, 0.0f, 2.0f, 0.001f        // m_yOffset
					, true                             // m_doBlur
					, &_programs._packDepth[EDepthImpl::InvZ][EPackDepth::RGBA][ERenderInstanceType::STAIC] //m_progPack
					, &_programs._packDepth[EDepthImpl::InvZ][EPackDepth::RGBA][ERenderInstanceType::INSTANCE] //m_progPackInstance
					, &_programs._colorLighting[EShadowMapType::Omni][EDepthImpl::InvZ][EShadowMapImpl::ESM][ERenderInstanceType::STAIC] //m_progDraw
					, &_programs._colorLighting[EShadowMapType::Omni][EDepthImpl::InvZ][EShadowMapImpl::ESM][ERenderInstanceType::INSTANCE] //m_progDrawInstance
				}

			},
			{ //EDepthImpl::Linear

				{ //EShadowMapImpl::Hard
					12.0f, 9.0f, 12.0f, 1.0f         // m_sizePwrTwo
					, 1.0f, 1.0f, 20.0f, 1.0f          // m_depthValuePow
					, 1.0f, 1.0f, 10.0f, 1.0f          // m_near
					, 250.0f, 100.0f, 2000.0f, 50.0f   // m_far
					, 0.003f, 0.0f, 0.01f, 0.00001f    // m_bias
					, 0.001f, 0.0f, 0.05f, 0.00001f    // m_normalOffset
					, 0.7f, 0.0f, 1.0f, 0.01f          // m_customParam0
					, 120.0f, 1.0f, 300.0f, 1.0f       // m_customParam1
					, 2.0f, 0.0f, 4.0f, 1.0f           // m_xNum
					, 2.0f, 0.0f, 4.0f, 1.0f           // m_yNum
					, 0.25f, 0.0f, 2.0f, 0.001f        // m_xOffset
					, 0.25f, 0.0f, 2.0f, 0.001f        // m_yOffset
					, true                             // m_doBlur
					, &_programs._packDepth[EDepthImpl::Linear][EPackDepth::RGBA][ERenderInstanceType::STAIC] //m_progPack
					, &_programs._packDepth[EDepthImpl::Linear][EPackDepth::RGBA][ERenderInstanceType::INSTANCE] //m_progPackInstance
					, &_programs._colorLighting[EShadowMapType::Omni][EDepthImpl::Linear][EShadowMapImpl::Hard][ERenderInstanceType::STAIC] //m_progDraw
					, &_programs._colorLighting[EShadowMapType::Omni][EDepthImpl::Linear][EShadowMapImpl::Hard][ERenderInstanceType::INSTANCE] //m_progDrawInstance
				},
				{ //EShadowMapImpl::PCF
					12.0f, 9.0f, 12.0f, 1.0f         // m_sizePwrTwo
					, 1.0f, 1.0f, 20.0f, 1.0f          // m_depthValuePow
					, 1.0f, 1.0f, 99.0f, 1.0f          // m_near
					, 250.0f, 100.0f, 2000.0f, 50.0f   // m_far
					, 0.0035f, 0.0f, 0.01f, 0.00001f   // m_bias
					, 0.001f, 0.0f, 0.05f, 0.00001f    // m_normalOffset
					, 0.7f, 0.0f, 1.0f, 0.01f          // m_customParam0
					, 120.0f, 1.0f, 300.0f, 1.0f       // m_customParam1
					, 2.0f, 0.0f, 8.0f, 1.0f           // m_xNum
					, 2.0f, 0.0f, 8.0f, 1.0f           // m_yNum
					, 1.0f, 0.0f, 3.0f, 0.001f         // m_xOffset
					, 1.0f, 0.0f, 3.0f, 0.001f         // m_yOffset
					, true                             // m_doBlur
					, &_programs._packDepth[EDepthImpl::Linear][EPackDepth::RGBA][ERenderInstanceType::STAIC] //m_progPack
					, &_programs._packDepth[EDepthImpl::Linear][EPackDepth::RGBA][ERenderInstanceType::INSTANCE] //m_progPackInstance
					, &_programs._colorLighting[EShadowMapType::Omni][EDepthImpl::Linear][EShadowMapImpl::PCF][ERenderInstanceType::STAIC] //m_progDraw
					, &_programs._colorLighting[EShadowMapType::Omni][EDepthImpl::Linear][EShadowMapImpl::PCF][ERenderInstanceType::INSTANCE] //m_progDrawInstance
				},
				{ //EShadowMapImpl::VSM
					12.0f, 9.0f, 12.0f, 1.0f         // m_sizePwrTwo
					, 1.0f, 1.0f, 20.0f, 1.0f          // m_depthValuePow
					, 1.0f, 1.0f, 10.0f, 1.0f          // m_near
					, 250.0f, 100.0f, 2000.0f, 50.0f   // m_far
					, 0.006f, 0.0f, 0.1f, 0.00001f     // m_bias
					, 0.001f, 0.0f, 0.05f, 0.00001f    // m_normalOffset
					, 0.02f, 0.0f, 0.1f, 0.00001f      // m_customParam0
					, 400.0f, 1.0f, 900.0f, 1.0f       // m_customParam1
					, 2.0f, 0.0f, 4.0f, 1.0f           // m_xNum
					, 2.0f, 0.0f, 4.0f, 1.0f           // m_yNum
					, 0.25f, 0.0f, 2.0f, 0.001f        // m_xOffset
					, 0.25f, 0.0f, 2.0f, 0.001f        // m_yOffset
					, true                             // m_doBlur
					, &_programs._packDepth[EDepthImpl::Linear][EPackDepth::VSM][ERenderInstanceType::STAIC] //m_progPack
					, &_programs._packDepth[EDepthImpl::Linear][EPackDepth::VSM][ERenderInstanceType::INSTANCE] //m_progPackInstance
					, &_programs._colorLighting[EShadowMapType::Omni][EDepthImpl::Linear][EShadowMapImpl::VSM][ERenderInstanceType::STAIC] //m_progDraw
					, &_programs._colorLighting[EShadowMapType::Omni][EDepthImpl::Linear][EShadowMapImpl::VSM][ERenderInstanceType::INSTANCE] //m_progDrawInstance
				},
				{ //EShadowMapImpl::ESM
					12.0f, 9.0f, 12.0f, 1.0f         // m_sizePwrTwo
					, 1.0f, 1.0f, 20.0f, 1.0f          // m_depthValuePow
					, 1.0f, 1.0f, 10.0f, 0.01f         // m_near
					, 250.0f, 100.0f, 2000.0f, 50.0f   // m_far
					, 0.007f, 0.0f, 0.01f, 0.00001f    // m_bias
					, 0.001f, 0.0f, 0.05f, 0.00001f    // m_normalOffset
					, 0.7f, 0.0f, 1.0f, 0.01f          // m_customParam0
					, 8000.0f, 1.0f, 15000.0f, 1.0f    // m_customParam1
					, 2.0f, 0.0f, 4.0f, 1.0f           // m_xNum
					, 2.0f, 0.0f, 4.0f, 1.0f           // m_yNum
					, 0.25f, 0.0f, 2.0f, 0.001f        // m_xOffset
					, 0.25f, 0.0f, 2.0f, 0.001f        // m_yOffset
					, true                             // m_doBlur
					, &_programs._packDepth[EDepthImpl::Linear][EPackDepth::RGBA][ERenderInstanceType::STAIC] //m_progPack
					, &_programs._packDepth[EDepthImpl::Linear][EPackDepth::RGBA][ERenderInstanceType::INSTANCE] //m_progPackInstance
					, &_programs._colorLighting[EShadowMapType::Omni][EDepthImpl::Linear][EShadowMapImpl::ESM][ERenderInstanceType::STAIC] //m_progDraw
					, &_programs._colorLighting[EShadowMapType::Omni][EDepthImpl::Linear][EShadowMapImpl::ESM][ERenderInstanceType::INSTANCE] //m_progDrawInstance
				}

			}

		},
		{ //LightType::Directional

			{ //EDepthImpl::InvZ

				{ //EShadowMapImpl::Hard
					11.0f, 7.0f, 12.0f, 1.0f         // m_sizePwrTwo
					, 1.0f, 1.0f, 20.0f, 1.0f          // m_depthValuePow
					, 1.0f, 1.0f, 10.0f, 1.0f          // m_near
					, 550.0f, 100.0f, 2000.0f, 50.0f   // m_far
					, 0.0012f, 0.0f, 0.01f, 0.00001f   // m_bias
					, 0.001f, 0.0f, 0.04f, 0.00001f    // m_normalOffset
					, 0.7f, 0.0f, 1.0f, 0.01f          // m_customParam0
					, 200.0f, 1.0f, 400.0f, 1.0f       // m_customParam1
					, 2.0f, 0.0f, 4.0f, 1.0f           // m_xNum
					, 2.0f, 0.0f, 4.0f, 1.0f           // m_yNum
					, 0.2f, 0.0f, 1.0f, 0.01f          // m_xOffset
					, 0.2f, 0.0f, 1.0f, 0.01f          // m_yOffset
					, true                             // m_doBlur
					, &_programs._packDepth[EDepthImpl::InvZ][EPackDepth::RGBA][ERenderInstanceType::STAIC] //m_progPack
					, &_programs._packDepth[EDepthImpl::InvZ][EPackDepth::RGBA][ERenderInstanceType::INSTANCE] //m_progPackInstance
					, &_programs._colorLighting[EShadowMapType::Cascade][EDepthImpl::InvZ][EShadowMapImpl::Hard][ERenderInstanceType::STAIC] //m_progDraw
					, &_programs._colorLighting[EShadowMapType::Cascade][EDepthImpl::InvZ][EShadowMapImpl::Hard][ERenderInstanceType::INSTANCE] //m_progDrawInstance
				},
				{ //EShadowMapImpl::PCF
					11.0f, 7.0f, 12.0f, 1.0f         // m_sizePwrTwo
					, 1.0f, 1.0f, 20.0f, 1.0f          // m_depthValuePow
					, 1.0f, 1.0f, 99.0f, 1.0f          // m_near
					, 550.0f, 100.0f, 2000.0f, 50.0f   // m_far
					, 0.0012f, 0.0f, 0.01f, 0.00001f   // m_bias
					, 0.001f, 0.0f, 0.04f, 0.00001f    // m_normalOffset
					, 0.7f, 0.0f, 1.0f, 0.01f          // m_customParam0
					, 200.0f, 1.0f, 400.0f, 1.0f       // m_customParam1
					, 2.0f, 0.0f, 8.0f, 1.0f           // m_xNum
					, 2.0f, 0.0f, 8.0f, 1.0f           // m_yNum
					, 1.0f, 0.0f, 3.0f, 0.01f          // m_xOffset
					, 1.0f, 0.0f, 3.0f, 0.01f          // m_yOffset
					, true                             // m_doBlur
					, &_programs._packDepth[EDepthImpl::InvZ][EPackDepth::RGBA][ERenderInstanceType::STAIC] //m_progPack
					, &_programs._packDepth[EDepthImpl::InvZ][EPackDepth::RGBA][ERenderInstanceType::INSTANCE] //m_progPackInstance
					, &_programs._colorLighting[EShadowMapType::Cascade][EDepthImpl::InvZ][EShadowMapImpl::PCF][ERenderInstanceType::STAIC] //m_progDraw
					, &_programs._colorLighting[EShadowMapType::Cascade][EDepthImpl::InvZ][EShadowMapImpl::PCF][ERenderInstanceType::INSTANCE] //m_progDrawInstance
				},
				{ //EShadowMapImpl::VSM
					11.0f, 7.0f, 12.0f, 1.0f         // m_sizePwrTwo
					, 1.0f, 1.0f, 20.0f, 1.0f          // m_depthValuePow
					, 1.0f, 1.0f, 10.0f, 1.0f          // m_near
					, 550.0f, 100.0f, 2000.0f, 50.0f   // m_far
					, 0.004f, 0.0f, 0.01f, 0.00001f    // m_bias
					, 0.001f, 0.0f, 0.04f, 0.00001f    // m_normalOffset
					, 0.02f, 0.0f, 0.04f, 0.00001f     // m_customParam0
					, 2500.0f, 1.0f, 5000.0f, 1.0f     // m_customParam1
					, 2.0f, 0.0f, 4.0f, 1.0f           // m_xNum
					, 2.0f, 0.0f, 4.0f, 1.0f           // m_yNum
					, 0.2f, 0.0f, 1.0f, 0.01f          // m_xOffset
					, 0.2f, 0.0f, 1.0f, 0.01f          // m_yOffset
					, true                             // m_doBlur
					, &_programs._packDepth[EDepthImpl::InvZ][EPackDepth::VSM][ERenderInstanceType::STAIC] //m_progPack
					, &_programs._packDepth[EDepthImpl::InvZ][EPackDepth::VSM][ERenderInstanceType::INSTANCE] //m_progPackInstance
					, &_programs._colorLighting[EShadowMapType::Cascade][EDepthImpl::InvZ][EShadowMapImpl::VSM][ERenderInstanceType::STAIC] //m_progDraw
					, &_programs._colorLighting[EShadowMapType::Cascade][EDepthImpl::InvZ][EShadowMapImpl::VSM][ERenderInstanceType::INSTANCE] //m_progDrawInstance
				},
				{ //EShadowMapImpl::ESM
					11.0f, 7.0f, 12.0f, 1.0f         // m_sizePwrTwo
					, 1.0f, 1.0f, 20.0f, 1.0f          // m_depthValuePow
					, 1.0f, 1.0f, 10.0f, 0.01f         // m_near
					, 550.0f, 100.0f, 2000.0f, 50.0f   // m_far
					, 0.004f, 0.0f, 0.01f, 0.00001f    // m_bias
					, 0.001f, 0.0f, 0.04f, 0.00001f    // m_normalOffset
					, 0.7f, 0.0f, 1.0f, 0.01f          // m_customParam0
					, 9500.0f, 1.0f, 15000.0f, 1.0f    // m_customParam1
					, 2.0f, 0.0f, 4.0f, 1.0f           // m_xNum
					, 2.0f, 0.0f, 4.0f, 1.0f           // m_yNum
					, 0.2f, 0.0f, 1.0f, 0.01f          // m_xOffset
					, 0.2f, 0.0f, 1.0f, 0.01f          // m_yOffset
					, true                             // m_doBlur
					, &_programs._packDepth[EDepthImpl::InvZ][EPackDepth::RGBA][ERenderInstanceType::STAIC] //m_progPack
					, &_programs._packDepth[EDepthImpl::InvZ][EPackDepth::RGBA][ERenderInstanceType::INSTANCE] //m_progPackInstance
					, &_programs._colorLighting[EShadowMapType::Cascade][EDepthImpl::InvZ][EShadowMapImpl::ESM][ERenderInstanceType::STAIC] //m_progDraw
					, &_programs._colorLighting[EShadowMapType::Cascade][EDepthImpl::InvZ][EShadowMapImpl::ESM][ERenderInstanceType::INSTANCE] //m_progDrawInstance
				}

			},
			{ //EDepthImpl::Linear

				{ //EShadowMapImpl::Hard
					11.0f, 7.0f, 12.0f, 1.0f         // m_sizePwrTwo
					, 1.0f, 1.0f, 20.0f, 1.0f          // m_depthValuePow
					, 1.0f, 1.0f, 10.0f, 1.0f          // m_near
					, 550.0f, 100.0f, 2000.0f, 50.0f   // m_far
					, 0.0012f, 0.0f, 0.01f, 0.00001f   // m_bias
					, 0.001f, 0.0f, 0.04f, 0.00001f    // m_normalOffset
					, 0.7f, 0.0f, 1.0f, 0.01f          // m_customParam0
					, 500.0f, 1.0f, 1000.0f, 1.0f      // m_customParam1
					, 2.0f, 0.0f, 4.0f, 1.0f           // m_xNum
					, 2.0f, 0.0f, 4.0f, 1.0f           // m_yNum
					, 0.2f, 0.0f, 1.0f, 0.01f          // m_xOffset
					, 0.2f, 0.0f, 1.0f, 0.01f          // m_yOffset
					, true                             // m_doBlur
					, &_programs._packDepth[EDepthImpl::Linear][EPackDepth::RGBA][ERenderInstanceType::STAIC] //m_progPack
					, &_programs._packDepth[EDepthImpl::Linear][EPackDepth::RGBA][ERenderInstanceType::INSTANCE] //m_progPackInstance
					, &_programs._colorLighting[EShadowMapType::Cascade][EDepthImpl::Linear][EShadowMapImpl::Hard][ERenderInstanceType::STAIC] //m_progDraw
					, &_programs._colorLighting[EShadowMapType::Cascade][EDepthImpl::Linear][EShadowMapImpl::Hard][ERenderInstanceType::INSTANCE] //m_progDrawInstance
				},
				{ //EShadowMapImpl::PCF
					11.0f, 7.0f, 12.0f, 1.0f         // m_sizePwrTwo
					, 1.0f, 1.0f, 20.0f, 1.0f          // m_depthValuePow
					, 1.0f, 1.0f, 99.0f, 1.0f          // m_near
					, 550.0f, 100.0f, 2000.0f, 50.0f   // m_far
					, 0.0012f, 0.0f, 0.01f, 0.00001f   // m_bias
					, 0.001f, 0.0f, 0.04f, 0.00001f    // m_normalOffset
					, 0.7f, 0.0f, 1.0f, 0.01f          // m_customParam0
					, 200.0f, 1.0f, 400.0f, 1.0f       // m_customParam1
					, 2.0f, 0.0f, 8.0f, 1.0f           // m_xNum
					, 2.0f, 0.0f, 8.0f, 1.0f           // m_yNum
					, 1.0f, 0.0f, 3.0f, 0.01f          // m_xOffset
					, 1.0f, 0.0f, 3.0f, 0.01f          // m_yOffset
					, true                             // m_doBlur
					, &_programs._packDepth[EDepthImpl::Linear][EPackDepth::RGBA][ERenderInstanceType::STAIC] //m_progPack
					, &_programs._packDepth[EDepthImpl::Linear][EPackDepth::RGBA][ERenderInstanceType::INSTANCE] //m_progPackInstance
					, &_programs._colorLighting[EShadowMapType::Cascade][EDepthImpl::Linear][EShadowMapImpl::PCF][ERenderInstanceType::STAIC] //m_progDraw
					, &_programs._colorLighting[EShadowMapType::Cascade][EDepthImpl::Linear][EShadowMapImpl::PCF][ERenderInstanceType::INSTANCE] //m_progDrawInstance
				},
				{ //EShadowMapImpl::VSM
					11.0f, 7.0f, 12.0f, 1.0f         // m_sizePwrTwo
					, 1.0f, 1.0f, 20.0f, 1.0f          // m_depthValuePow
					, 1.0f, 1.0f, 10.0f, 1.0f          // m_near
					, 550.0f, 100.0f, 2000.0f, 50.0f   // m_far
					, 0.004f, 0.0f, 0.01f, 0.00001f    // m_bias
					, 0.001f, 0.0f, 0.04f, 0.00001f    // m_normalOffset
					, 0.02f, 0.0f, 0.04f, 0.00001f     // m_customParam0
					, 2500.0f, 1.0f, 5000.0f, 1.0f     // m_customParam1
					, 2.0f, 0.0f, 4.0f, 1.0f           // m_xNum
					, 2.0f, 0.0f, 4.0f, 1.0f           // m_yNum
					, 0.2f, 0.0f, 1.0f, 0.01f          // m_xOffset
					, 0.2f, 0.0f, 1.0f, 0.01f          // m_yOffset
					, true                             // m_doBlur
					, &_programs._packDepth[EDepthImpl::Linear][EPackDepth::VSM][ERenderInstanceType::STAIC] //m_progPack
					, &_programs._packDepth[EDepthImpl::Linear][EPackDepth::VSM][ERenderInstanceType::INSTANCE] //m_progPackInstance
					, &_programs._colorLighting[EShadowMapType::Cascade][EDepthImpl::Linear][EShadowMapImpl::VSM][ERenderInstanceType::STAIC] //m_progDraw
					, &_programs._colorLighting[EShadowMapType::Cascade][EDepthImpl::Linear][EShadowMapImpl::VSM][ERenderInstanceType::INSTANCE] //m_progDrawInstance
				},
				{ //EShadowMapImpl::ESM
					11.0f, 7.0f, 12.0f, 1.0f         // m_sizePwrTwo
					, 1.0f, 1.0f, 20.0f, 1.0f          // m_depthValuePow
					, 1.0f, 1.0f, 10.0f, 0.01f         // m_near
					, 550.0f, 100.0f, 2000.0f, 50.0f   // m_far
					, 0.004f, 0.0f, 0.01f, 0.00001f    // m_bias
					, 0.001f, 0.0f, 0.04f, 0.00001f    // m_normalOffset
					, 0.7f, 0.0f, 1.0f, 0.01f          // m_customParam0
					, 9500.0f, 1.0f, 15000.0f, 1.0f    // m_customParam1
					, 2.0f, 0.0f, 4.0f, 1.0f           // m_xNum
					, 2.0f, 0.0f, 4.0f, 1.0f           // m_yNum
					, 0.2f, 0.0f, 1.0f, 0.01f          // m_xOffset
					, 0.2f, 0.0f, 1.0f, 0.01f          // m_yOffset
					, true                             // m_doBlur
					, &_programs._packDepth[EDepthImpl::Linear][EPackDepth::RGBA][ERenderInstanceType::STAIC] //m_progPack
					, &_programs._packDepth[EDepthImpl::Linear][EPackDepth::RGBA][ERenderInstanceType::INSTANCE] //m_progPackInstance
					, &_programs._colorLighting[EShadowMapType::Cascade][EDepthImpl::Linear][EShadowMapImpl::ESM][ERenderInstanceType::STAIC] //m_progDraw
					, &_programs._colorLighting[EShadowMapType::Cascade][EDepthImpl::Linear][EShadowMapImpl::ESM][ERenderInstanceType::INSTANCE] //m_progDrawInstance
				}

			}
		}
	};
	bx::memCopy(s_shadowMapSettings, smSettings, sizeof(smSettings));

	s_shadowSceneSettings.m_lightType = ELightType::SpotLight;
	s_shadowSceneSettings.m_depthImpl = EDepthImpl::InvZ;
	s_shadowSceneSettings.m_smImpl = EShadowMapImpl::Hard;
	s_shadowSceneSettings.m_spotOuterAngle = 45.0f;
	s_shadowSceneSettings.m_spotInnerAngle = 30.0f;
	s_shadowSceneSettings.m_fovXAdjust = 0.0f;
	s_shadowSceneSettings.m_fovYAdjust = 0.0f;
	s_shadowSceneSettings.m_coverageSpotL = 90.0f;
	s_shadowSceneSettings.m_splitDistribution = 0.6f;
	s_shadowSceneSettings.m_numSplits = 4;
	s_shadowSceneSettings.m_drawDepthBuffer = false;
	s_shadowSceneSettings.m_showSmCoverage = false;
	s_shadowSceneSettings.m_stencilPack = true;
	s_shadowSceneSettings.m_stabilize = true;

	FShadowMapSettings* currentSmSettings = GetCurrentShadowMapSettings();

	// Render targets.
	uint16_t shadowMapSize = 1 << uint32_t(currentSmSettings->m_sizePwrTwo);
	s_currentShadowMapSize = shadowMapSize;
	float currentShadowMapSizef = float(int16_t(s_currentShadowMapSize));
	_uniforms._shadowMapTexelSize = 1.0f / currentShadowMapSizef;

	for (uint8_t ii = 0; ii < ShadowMapRenderTargets::Count; ++ii)
	{
		bgfx::TextureHandle fbtextures[] =
		{
			bgfx::createTexture2D(s_currentShadowMapSize, s_currentShadowMapSize, false, 1, bgfx::TextureFormat::BGRA8, BGFX_TEXTURE_RT),
			bgfx::createTexture2D(s_currentShadowMapSize, s_currentShadowMapSize, false, 1, bgfx::TextureFormat::D24S8, BGFX_TEXTURE_RT),
		};
		s_rtShadowMap[ii] = bgfx::createFrameBuffer(BX_COUNTOF(fbtextures), fbtextures, true);
	}
	s_rtBlur = bgfx::createFrameBuffer(s_currentShadowMapSize, s_currentShadowMapSize, bgfx::TextureFormat::BGRA8);

	_uniforms.submitConstUniforms();

	// Materials.
	s_defaultMaterial =
	{
		{ { 1.0f, 1.0f, 1.0f, 0.0f } }, //ambient
		{ { 1.0f, 1.0f, 1.0f, 0.0f } }, //diffuse
		{ { 1.0f, 1.0f, 1.0f, 0.0f } }, //specular, exponent
	};

	// Setup uniforms.
	_uniforms.setPtrs(&FShadowPipline::s_defaultMaterial
		, nullptr
		, FColor(1.0f, 1.0f, 1.0f).Data()
		, FShadowPipline::s_lightMtx.Data()
		, &FShadowPipline::s_shadowMapMtx[ShadowMapRenderTargets::First].Data()[0]
		, &FShadowPipline::s_shadowMapMtx[ShadowMapRenderTargets::Second].Data()[0]
		, &FShadowPipline::s_shadowMapMtx[ShadowMapRenderTargets::Third].Data()[0]
		, &FShadowPipline::s_shadowMapMtx[ShadowMapRenderTargets::Fourth].Data()[0]
	);

	_shadowPosLayout.begin();
	_shadowPosLayout.add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float);
	_shadowPosLayout.end();


}
void FShadowPipline::Update(ACameraComponent* camera, ALightComponent* light)
{
	if (camera == nullptr || light == nullptr)
		return;
	GProcessWindow& processWindow = GProcessWindow::Get();

	const bgfx::Caps* caps = bgfx::getCaps();

	// Set view and projection matrices.
	const float camFovy = camera->GetFov();
	const float camAspect = camera->GetAspectRatio();
	const float projHeight = bx::tan(bx::toRad(camFovy)*0.5f);
	const float projWidth = projHeight * camAspect;

	TMatrix4x4F projectionMatrix = camera->GetProjectionMatrix();
	TMatrix4x4F transposeViewMatrix = camera->GetViewMatrix().ToMatrix4().Transpose();


	float currentShadowMapSizef = float(int16_t(FShadowPipline::s_currentShadowMapSize));
	_uniforms._shadowMapTexelSize = 1.0f / currentShadowMapSizef;

	_uniforms.submitConstUniforms();

	FShadowMapSettings* currentShadowMapSettings = &FShadowPipline::s_shadowMapSettings[FShadowPipline::s_shadowSceneSettings.m_lightType][FShadowPipline::s_shadowSceneSettings.m_depthImpl][FShadowPipline::s_shadowSceneSettings.m_smImpl];
	// Update uniforms.
	_uniforms._shadowMapBias = currentShadowMapSettings->m_bias;
	_uniforms._shadowMapOffset = currentShadowMapSettings->m_normalOffset;
	_uniforms._shadowMapParam0 = currentShadowMapSettings->m_customParam0;
	_uniforms._shadowMapParam1 = currentShadowMapSettings->m_customParam1;
	_uniforms._depthValuePow = currentShadowMapSettings->m_depthValuePow;
	_uniforms._XNum = currentShadowMapSettings->m_xNum;
	_uniforms._YNum = currentShadowMapSettings->m_yNum;
	_uniforms._XOffset = currentShadowMapSettings->m_xOffset;
	_uniforms._YOffset = currentShadowMapSettings->m_yOffset;
	_uniforms._showSmCoverage = float(FShadowPipline::s_shadowSceneSettings.m_showSmCoverage);
	_uniforms._lightPtr = light;
	_uniforms._colorPtr = light->GetLightColor().Data();

	switch (light->GetLightType())
	{
		case ELightType::DirectionalLight:
			FShadowPipline::s_shadowSceneSettings.m_lightType = ELightType::DirectionalLight;
			break;
		case ELightType::PointLight:
			FShadowPipline::s_shadowSceneSettings.m_lightType = ELightType::PointLight;
			break;
		case ELightType::SpotLight:
			FShadowPipline::s_shadowSceneSettings.m_lightType = ELightType::SpotLight;
			break;

	default:
		break;
	}

	if (ELightType::SpotLight == FShadowPipline::s_shadowSceneSettings.m_lightType)
	{
		light->m_attenuationSpotOuter.m_outer = FShadowPipline::s_shadowSceneSettings.m_spotOuterAngle;
		light->m_spotDirectionInner.m_inner = FShadowPipline::s_shadowSceneSettings.m_spotInnerAngle;
	}
	else
	{
		light->m_attenuationSpotOuter.m_outer = 91.0f; //above 90.0f means point light
	}

	_uniforms.submitPerFrameUniforms();

	int64_t now = bx::getHPCounter();
	static int64_t last = now;
	const int64_t frameTime = now - last;
	last = now;
	const double freq = double(bx::getHPFrequency());
	const float deltaTime = float(frameTime / freq);

	// Update lights.
	light->ComputeViewSpaceComponents(transposeViewMatrix.Data());

	// Setup lights.
	TVector3F lightPosition = light->GetOwner()->GetTransform()->GetPosition();
	TVector3F lightDirection = light->GetOwner()->GetTransform()->GetDirection();

	if (ELightType::DirectionalLight == FShadowPipline::s_shadowSceneSettings.m_lightType)
	{
		
		light->m_position._x = -lightPosition._x;
		light->m_position._y = -lightPosition._y;
		light->m_position._z = -lightPosition._z;
	}
	else
	{
		light->m_position._x = lightPosition._x;
		light->m_position._y = lightPosition._y;
		light->m_position._z = lightPosition._z;
		light->m_spotDirectionInner.m_x = -(lightPosition._x + lightDirection._x);
		light->m_spotDirectionInner.m_y = -(lightPosition._y + lightDirection._y);
		light->m_spotDirectionInner.m_z = -(lightPosition._z + lightDirection._z);
	}

	_screenView = TMatrix4x4F::IDENTITY;

	bx::mtxOrtho(
		_screenProj.Data()
		, 0.0f
		, 1.0f
		, 1.0f
		, 0.0f
		, 0.0f
		, 100.0f
		, 0.0f
		, caps->homogeneousDepth
	);

	// Update render target size.
	uint16_t shadowMapSize = 1 << uint32_t(currentShadowMapSettings->m_sizePwrTwo);
	if (FShadowPipline::s_currentShadowMapSize != shadowMapSize)
	{
		FShadowPipline::s_currentShadowMapSize = shadowMapSize;
		_uniforms._shadowMapTexelSize = 1.0f / currentShadowMapSizef;

		{
			bgfx::destroy(FShadowPipline::s_rtShadowMap[0]);

			bgfx::TextureHandle fbtextures[] =
			{
				bgfx::createTexture2D(FShadowPipline::s_currentShadowMapSize, FShadowPipline::s_currentShadowMapSize, false, 1, bgfx::TextureFormat::BGRA8, BGFX_TEXTURE_RT),
				bgfx::createTexture2D(FShadowPipline::s_currentShadowMapSize, FShadowPipline::s_currentShadowMapSize, false, 1, bgfx::TextureFormat::D24S8, BGFX_TEXTURE_RT),
			};
			FShadowPipline::s_rtShadowMap[0] = bgfx::createFrameBuffer(BX_COUNTOF(fbtextures), fbtextures, true);
		}

		if (ELightType::DirectionalLight == FShadowPipline::s_shadowSceneSettings.m_lightType)
		{
			for (uint8_t ii = 1; ii < ShadowMapRenderTargets::Count; ++ii)
			{
				{
					bgfx::destroy(FShadowPipline::s_rtShadowMap[ii]);

					bgfx::TextureHandle fbtextures[] =
					{
						bgfx::createTexture2D(FShadowPipline::s_currentShadowMapSize, FShadowPipline::s_currentShadowMapSize, false, 1, bgfx::TextureFormat::BGRA8, BGFX_TEXTURE_RT),
						bgfx::createTexture2D(FShadowPipline::s_currentShadowMapSize, FShadowPipline::s_currentShadowMapSize, false, 1, bgfx::TextureFormat::D24S8, BGFX_TEXTURE_RT),
					};
					FShadowPipline::s_rtShadowMap[ii] = bgfx::createFrameBuffer(BX_COUNTOF(fbtextures), fbtextures, true);
				}
			}
		}

		bgfx::destroy(FShadowPipline::s_rtBlur);
		FShadowPipline::s_rtBlur = bgfx::createFrameBuffer(FShadowPipline::s_currentShadowMapSize, FShadowPipline::s_currentShadowMapSize, bgfx::TextureFormat::BGRA8);
	}

	if (ELightType::SpotLight == FShadowPipline::s_shadowSceneSettings.m_lightType)
	{
		const float fovy = FShadowPipline::s_shadowSceneSettings.m_coverageSpotL;
		const float aspect = 1.0f;
		bx::mtxProj(
			FShadowPipline::s_lightProj[ProjType::Horizontal].Data()
			, fovy
			, aspect
			, currentShadowMapSettings->m_near
			, currentShadowMapSettings->m_far
			, false
		);

		//For linear depth, prevent depth division by variable w-component in shaders and divide here by far plane
		if (EDepthImpl::Linear == FShadowPipline::s_shadowSceneSettings.m_depthImpl)
		{
			FShadowPipline::s_lightProj[ProjType::Horizontal].Data()[10] /= currentShadowMapSettings->m_far;
			FShadowPipline::s_lightProj[ProjType::Horizontal].Data()[14] /= currentShadowMapSettings->m_far;
		}

		const bx::Vec3 at = bx::add(bx::load<bx::Vec3>(light->m_position.Data()), bx::load<bx::Vec3>(light->m_spotDirectionInner.m_v));
		bx::mtxLookAt(FShadowPipline::s_lightView[TetrahedronFaces::Green].Data(), bx::load<bx::Vec3>(light->m_position.Data()), at);
	}
	else if (ELightType::PointLight == FShadowPipline::s_shadowSceneSettings.m_lightType)
	{
		float ypr[TetrahedronFaces::Count][3] =
		{
			{ bx::toRad(0.0f), bx::toRad(27.36780516f), bx::toRad(0.0f) },
			{ bx::toRad(180.0f), bx::toRad(27.36780516f), bx::toRad(0.0f) },
			{ bx::toRad(-90.0f), bx::toRad(-27.36780516f), bx::toRad(0.0f) },
			{ bx::toRad(90.0f), bx::toRad(-27.36780516f), bx::toRad(0.0f) },
		};


		if (FShadowPipline::s_shadowSceneSettings.m_stencilPack)
		{
			const float fovx = 143.98570868f + 3.51f + FShadowPipline::s_shadowSceneSettings.m_fovXAdjust;
			const float fovy = 125.26438968f + 9.85f + FShadowPipline::s_shadowSceneSettings.m_fovYAdjust;
			const float aspect = bx::tan(bx::toRad(fovx*0.5f)) / bx::tan(bx::toRad(fovy*0.5f));

			bx::mtxProj(
				FShadowPipline::s_lightProj[ProjType::Vertical].Data()
				, fovx
				, aspect
				, currentShadowMapSettings->m_near
				, currentShadowMapSettings->m_far
				, false
			);

			//For linear depth, prevent depth division by variable w-component in shaders and divide here by far plane
			if (EDepthImpl::Linear == FShadowPipline::s_shadowSceneSettings.m_depthImpl)
			{
				FShadowPipline::s_lightProj[ProjType::Vertical].Data()[10] /= currentShadowMapSettings->m_far;
				FShadowPipline::s_lightProj[ProjType::Vertical].Data()[14] /= currentShadowMapSettings->m_far;
			}

			ypr[TetrahedronFaces::Green][2] = bx::toRad(180.0f);
			ypr[TetrahedronFaces::Yellow][2] = bx::toRad(0.0f);
			ypr[TetrahedronFaces::Blue][2] = bx::toRad(90.0f);
			ypr[TetrahedronFaces::Red][2] = bx::toRad(-90.0f);
		}

		const float fovx = 143.98570868f + 7.8f + FShadowPipline::s_shadowSceneSettings.m_fovXAdjust;
		const float fovy = 125.26438968f + 3.0f + FShadowPipline::s_shadowSceneSettings.m_fovYAdjust;
		const float aspect = bx::tan(bx::toRad(fovx*0.5f)) / bx::tan(bx::toRad(fovy*0.5f));

		bx::mtxProj(
			FShadowPipline::s_lightProj[ProjType::Horizontal].Data()
			, fovy
			, aspect
			, currentShadowMapSettings->m_near
			, currentShadowMapSettings->m_far
			, caps->homogeneousDepth
		);

		//For linear depth, prevent depth division by variable w component in shaders and divide here by far plane
		if (EDepthImpl::Linear == FShadowPipline::s_shadowSceneSettings.m_depthImpl)
		{
			FShadowPipline::s_lightProj[ProjType::Horizontal].Data()[10] /= currentShadowMapSettings->m_far;
			FShadowPipline::s_lightProj[ProjType::Horizontal].Data()[14] /= currentShadowMapSettings->m_far;
		}


		for (uint8_t ii = 0; ii < TetrahedronFaces::Count; ++ii)
		{
			float mtxTmp[16];
			mtxYawPitchRoll(mtxTmp, ypr[ii][0], ypr[ii][1], ypr[ii][2]);

			float tmp[3] =
			{
				-bx::dot(bx::load<bx::Vec3>(light->m_position.Data()), bx::load<bx::Vec3>(&mtxTmp[0])),
				-bx::dot(bx::load<bx::Vec3>(light->m_position.Data()), bx::load<bx::Vec3>(&mtxTmp[4])),
				-bx::dot(bx::load<bx::Vec3>(light->m_position.Data()), bx::load<bx::Vec3>(&mtxTmp[8])),
			};

			bx::mtxTranspose(FShadowPipline::s_mtxYpr[ii].Data(), mtxTmp);

			bx::memCopy(FShadowPipline::s_lightView[ii].Data(), FShadowPipline::s_mtxYpr[ii].Data(), 12 * sizeof(float));
			FShadowPipline::s_lightView[ii].Data()[12] = tmp[0];
			FShadowPipline::s_lightView[ii].Data()[13] = tmp[1];
			FShadowPipline::s_lightView[ii].Data()[14] = tmp[2];
			FShadowPipline::s_lightView[ii].Data()[15] = 1.0f;
		}
	}
	else // LightType::DirectionalLight == settings.m_lightType
	{
		// Setup light view mtx.
		const bx::Vec3 at = { 0.0f, 0.0f, 0.0f };
		const bx::Vec3 eye =
		{
			-light->m_position._x,
			-light->m_position._y,
			-light->m_position._z,
		};
		bx::mtxLookAt(FShadowPipline::s_lightView[0].Data(), eye, at);

		// Compute camera inverse view mtx.
		float mtxViewInv[16];
		bx::mtxInverse(mtxViewInv, transposeViewMatrix.Data());

		// Compute split distances.
		const uint8_t maxNumSplits = 4;
		BX_CHECK(maxNumSplits >= settings.m_numSplits, "Error! Max num splits.");

		float splitSlices[maxNumSplits * 2];
		splitFrustum(splitSlices
			, uint8_t(FShadowPipline::s_shadowSceneSettings.m_numSplits)
			, currentShadowMapSettings->m_near
			, currentShadowMapSettings->m_far
			, FShadowPipline::s_shadowSceneSettings.m_splitDistribution
		);

		// Update uniforms.
		for (uint8_t ii = 0, ff = 1; ii < FShadowPipline::s_shadowSceneSettings.m_numSplits; ++ii, ff += 2)
		{
			// This lags for 1 frame, but it's not a problem.
			_uniforms._csmFarDistances[ii] = splitSlices[ff];
		}

		float mtxProj[16];
		bx::mtxOrtho(
			mtxProj
			, 1.0f
			, -1.0f
			, 1.0f
			, -1.0f
			, -currentShadowMapSettings->m_far
			, currentShadowMapSettings->m_far
			, 0.0f
			, caps->homogeneousDepth
		);

		const uint8_t numCorners = 8;
		float frustumCorners[maxNumSplits][numCorners][3];
		for (uint8_t ii = 0, nn = 0, ff = 1; ii < FShadowPipline::s_shadowSceneSettings.m_numSplits; ++ii, nn += 2, ff += 2)
		{
			// Compute frustum corners for one split in world space.
			worldSpaceFrustumCorners((float*)frustumCorners[ii], splitSlices[nn], splitSlices[ff], projWidth, projHeight, mtxViewInv);

			bx::Vec3 min = { 9000.0f,  9000.0f,  9000.0f };
			bx::Vec3 max = { -9000.0f, -9000.0f, -9000.0f };

			for (uint8_t jj = 0; jj < numCorners; ++jj)
			{
				// Transform to light space.
				const bx::Vec3 xyz = bx::mul(bx::load<bx::Vec3>(frustumCorners[ii][jj]), FShadowPipline::s_lightView[0].Data());

				// Update bounding box.
				min = bx::min(min, xyz);
				max = bx::max(max, xyz);
			}

			const bx::Vec3 minproj = bx::mulH(min, mtxProj);
			const bx::Vec3 maxproj = bx::mulH(max, mtxProj);

			float scalex = 2.0f / (maxproj.x - minproj.x);
			float scaley = 2.0f / (maxproj.y - minproj.y);

			if (FShadowPipline::s_shadowSceneSettings.m_stabilize)
			{
				const float quantizer = 64.0f;
				scalex = quantizer / bx::ceil(quantizer / scalex);
				scaley = quantizer / bx::ceil(quantizer / scaley);
			}

			float offsetx = 0.5f * (maxproj.x + minproj.x) * scalex;
			float offsety = 0.5f * (maxproj.y + minproj.y) * scaley;

			if (FShadowPipline::s_shadowSceneSettings.m_stabilize)
			{
				const float halfSize = currentShadowMapSizef * 0.5f;
				offsetx = bx::ceil(offsetx * halfSize) / halfSize;
				offsety = bx::ceil(offsety * halfSize) / halfSize;
			}

			float mtxCrop[16];
			bx::mtxIdentity(mtxCrop);
			mtxCrop[0] = scalex;
			mtxCrop[5] = scaley;
			mtxCrop[12] = offsetx;
			mtxCrop[13] = offsety;

			bx::mtxMul(FShadowPipline::s_lightProj[ii].Data(), mtxCrop, mtxProj);
		}
	}


	// Reset render targets.
	const bgfx::FrameBufferHandle invalidRt = BGFX_INVALID_HANDLE;
	for (uint8_t ii = RENDERVIEW_SHADOW_BEGIN; ii < RENDERVIEW_DRAWDEPTH_3_ID + 1; ++ii)
	{
		bgfx::setViewFrameBuffer(ii, invalidRt);
		bgfx::setViewRect(ii, 0, 0, processWindow._width, processWindow._height);
	}

	// Determine on-screen rectangle size where depth buffer will be drawn.
	uint16_t depthRectHeight = uint16_t(float(processWindow._height) / 2.5f);
	uint16_t depthRectWidth = depthRectHeight;
	uint16_t depthRectX = 0;
	uint16_t depthRectY = processWindow._height - depthRectHeight;

	// Setup views and render targets.
	bgfx::setViewRect(RENDERVIEW_SHADOW_BEGIN, 0, 0, processWindow._width, processWindow._height);
	bgfx::setViewTransform(RENDERVIEW_SHADOW_BEGIN, transposeViewMatrix.Data(), projectionMatrix.Data());

	if (ELightType::SpotLight == FShadowPipline::s_shadowSceneSettings.m_lightType)
	{
		/**
			* RENDERVIEW_SHADOWMAP_0_ID - Clear shadow map. (used as convenience, otherwise render_pass_1 could be cleared)
			* RENDERVIEW_SHADOWMAP_1_ID - Craft shadow map.
			* RENDERVIEW_VBLUR_0_ID - Vertical blur.
			* RENDERVIEW_HBLUR_0_ID - Horizontal blur.
			* RENDERVIEW_DRAWSCENE_0_ID - Draw scene.
			* RENDERVIEW_DRAWSCENE_1_ID - Draw floor bottom.
			* RENDERVIEW_DRAWDEPTH_0_ID - Draw depth buffer.
			*/

		bgfx::setViewRect(RENDERVIEW_SHADOWMAP_0_ID, 0, 0, FShadowPipline::s_currentShadowMapSize, FShadowPipline::s_currentShadowMapSize);
		bgfx::setViewRect(RENDERVIEW_SHADOWMAP_1_ID, 0, 0, FShadowPipline::s_currentShadowMapSize, FShadowPipline::s_currentShadowMapSize);
		bgfx::setViewRect(RENDERVIEW_VBLUR_0_ID, 0, 0, FShadowPipline::s_currentShadowMapSize, FShadowPipline::s_currentShadowMapSize);
		bgfx::setViewRect(RENDERVIEW_HBLUR_0_ID, 0, 0, FShadowPipline::s_currentShadowMapSize, FShadowPipline::s_currentShadowMapSize);
		bgfx::setViewRect(RENDERVIEW_DRAWSCENE_0_ID, 0, 0, processWindow._width, processWindow._height);
		bgfx::setViewRect(RENDERVIEW_DRAWSCENE_1_ID, 0, 0, processWindow._width, processWindow._height);
		bgfx::setViewRect(RENDERVIEW_DRAWDEPTH_0_ID, depthRectX, depthRectY, depthRectWidth, depthRectHeight);

		bgfx::setViewTransform(RENDERVIEW_SHADOWMAP_0_ID, _screenView.Data(), _screenProj.Data());
		bgfx::setViewTransform(RENDERVIEW_SHADOWMAP_1_ID, FShadowPipline::s_lightView[0].Data(), FShadowPipline::s_lightProj[ProjType::Horizontal].Data());
		bgfx::setViewTransform(RENDERVIEW_VBLUR_0_ID, _screenView.Data(), _screenProj.Data());
		bgfx::setViewTransform(RENDERVIEW_HBLUR_0_ID, _screenView.Data(), _screenProj.Data());
		bgfx::setViewTransform(RENDERVIEW_DRAWSCENE_0_ID, transposeViewMatrix.Data(), projectionMatrix.Data());
		bgfx::setViewTransform(RENDERVIEW_DRAWSCENE_1_ID, transposeViewMatrix.Data(), projectionMatrix.Data());
		bgfx::setViewTransform(RENDERVIEW_DRAWDEPTH_0_ID, _screenView.Data(), _screenProj.Data());

		bgfx::setViewFrameBuffer(RENDERVIEW_SHADOWMAP_0_ID, FShadowPipline::s_rtShadowMap[0]);
		bgfx::setViewFrameBuffer(RENDERVIEW_SHADOWMAP_1_ID, FShadowPipline::s_rtShadowMap[0]);
		bgfx::setViewFrameBuffer(RENDERVIEW_VBLUR_0_ID, FShadowPipline::s_rtBlur);
		bgfx::setViewFrameBuffer(RENDERVIEW_HBLUR_0_ID, FShadowPipline::s_rtShadowMap[0]);
	}
	else if (ELightType::PointLight == FShadowPipline::s_shadowSceneSettings.m_lightType)
	{
		/**
			* RENDERVIEW_SHADOWMAP_0_ID - Clear entire shadow map.
			* RENDERVIEW_SHADOWMAP_1_ID - Craft green tetrahedron shadow face.
			* RENDERVIEW_SHADOWMAP_2_ID - Craft yellow tetrahedron shadow face.
			* RENDERVIEW_SHADOWMAP_3_ID - Craft blue tetrahedron shadow face.
			* RENDERVIEW_SHADOWMAP_4_ID - Craft red tetrahedron shadow face.
			* RENDERVIEW_VBLUR_0_ID - Vertical blur.
			* RENDERVIEW_HBLUR_0_ID - Horizontal blur.
			* RENDERVIEW_DRAWSCENE_0_ID - Draw scene.
			* RENDERVIEW_DRAWSCENE_1_ID - Draw floor bottom.
			* RENDERVIEW_DRAWDEPTH_0_ID - Draw depth buffer.
			*/

		bgfx::setViewRect(RENDERVIEW_SHADOWMAP_0_ID, 0, 0, FShadowPipline::s_currentShadowMapSize, FShadowPipline::s_currentShadowMapSize);
		if (FShadowPipline::s_shadowSceneSettings.m_stencilPack)
		{
			const uint16_t f = FShadowPipline::s_currentShadowMapSize;   //full size
			const uint16_t h = FShadowPipline::s_currentShadowMapSize / 2; //half size
			bgfx::setViewRect(RENDERVIEW_SHADOWMAP_1_ID, 0, 0, f, h);
			bgfx::setViewRect(RENDERVIEW_SHADOWMAP_2_ID, 0, h, f, h);
			bgfx::setViewRect(RENDERVIEW_SHADOWMAP_3_ID, 0, 0, h, f);
			bgfx::setViewRect(RENDERVIEW_SHADOWMAP_4_ID, h, 0, h, f);
		}
		else
		{
			const uint16_t h = FShadowPipline::s_currentShadowMapSize / 2; //half size
			bgfx::setViewRect(RENDERVIEW_SHADOWMAP_1_ID, 0, 0, h, h);
			bgfx::setViewRect(RENDERVIEW_SHADOWMAP_2_ID, h, 0, h, h);
			bgfx::setViewRect(RENDERVIEW_SHADOWMAP_3_ID, 0, h, h, h);
			bgfx::setViewRect(RENDERVIEW_SHADOWMAP_4_ID, h, h, h, h);
		}
		bgfx::setViewRect(RENDERVIEW_VBLUR_0_ID, 0, 0, FShadowPipline::s_currentShadowMapSize, FShadowPipline::s_currentShadowMapSize);
		bgfx::setViewRect(RENDERVIEW_HBLUR_0_ID, 0, 0, FShadowPipline::s_currentShadowMapSize, FShadowPipline::s_currentShadowMapSize);
		bgfx::setViewRect(RENDERVIEW_DRAWSCENE_0_ID, 0, 0, processWindow._width, processWindow._height);
		bgfx::setViewRect(RENDERVIEW_DRAWSCENE_1_ID, 0, 0, processWindow._width, processWindow._height);
		bgfx::setViewRect(RENDERVIEW_DRAWDEPTH_0_ID, depthRectX, depthRectY, depthRectWidth, depthRectHeight);

		bgfx::setViewTransform(RENDERVIEW_SHADOWMAP_0_ID, _screenView.Data(), _screenProj.Data());
		bgfx::setViewTransform(RENDERVIEW_SHADOWMAP_1_ID, FShadowPipline::s_lightView[TetrahedronFaces::Green].Data(), FShadowPipline::s_lightProj[ProjType::Horizontal].Data());
		bgfx::setViewTransform(RENDERVIEW_SHADOWMAP_2_ID, FShadowPipline::s_lightView[TetrahedronFaces::Yellow].Data(), FShadowPipline::s_lightProj[ProjType::Horizontal].Data());
		if (FShadowPipline::s_shadowSceneSettings.m_stencilPack)
		{
			bgfx::setViewTransform(RENDERVIEW_SHADOWMAP_3_ID, FShadowPipline::s_lightView[TetrahedronFaces::Blue].Data(), FShadowPipline::s_lightProj[ProjType::Vertical].Data());
			bgfx::setViewTransform(RENDERVIEW_SHADOWMAP_4_ID, FShadowPipline::s_lightView[TetrahedronFaces::Red].Data(), FShadowPipline::s_lightProj[ProjType::Vertical].Data());
		}
		else
		{
			bgfx::setViewTransform(RENDERVIEW_SHADOWMAP_3_ID, FShadowPipline::s_lightView[TetrahedronFaces::Blue].Data(), FShadowPipline::s_lightProj[ProjType::Horizontal].Data());
			bgfx::setViewTransform(RENDERVIEW_SHADOWMAP_4_ID, FShadowPipline::s_lightView[TetrahedronFaces::Red].Data(), FShadowPipline::s_lightProj[ProjType::Horizontal].Data());
		}
		bgfx::setViewTransform(RENDERVIEW_VBLUR_0_ID, _screenView.Data(), _screenProj.Data());
		bgfx::setViewTransform(RENDERVIEW_HBLUR_0_ID, _screenView.Data(), _screenProj.Data());
		bgfx::setViewTransform(RENDERVIEW_DRAWSCENE_0_ID, transposeViewMatrix.Data(), projectionMatrix.Data());
		bgfx::setViewTransform(RENDERVIEW_DRAWSCENE_1_ID, transposeViewMatrix.Data(), projectionMatrix.Data());
		bgfx::setViewTransform(RENDERVIEW_DRAWDEPTH_0_ID, _screenView.Data(), _screenProj.Data());

		bgfx::setViewFrameBuffer(RENDERVIEW_SHADOWMAP_0_ID, FShadowPipline::s_rtShadowMap[0]);
		bgfx::setViewFrameBuffer(RENDERVIEW_SHADOWMAP_1_ID, FShadowPipline::s_rtShadowMap[0]);
		bgfx::setViewFrameBuffer(RENDERVIEW_SHADOWMAP_2_ID, FShadowPipline::s_rtShadowMap[0]);
		bgfx::setViewFrameBuffer(RENDERVIEW_SHADOWMAP_3_ID, FShadowPipline::s_rtShadowMap[0]);
		bgfx::setViewFrameBuffer(RENDERVIEW_SHADOWMAP_4_ID, FShadowPipline::s_rtShadowMap[0]);
		bgfx::setViewFrameBuffer(RENDERVIEW_VBLUR_0_ID, FShadowPipline::s_rtBlur);
		bgfx::setViewFrameBuffer(RENDERVIEW_HBLUR_0_ID, FShadowPipline::s_rtShadowMap[0]);
	}
	else // LightType::DirectionalLight == settings.m_lightType
	{
		/**
			* RENDERVIEW_SHADOWMAP_1_ID - Craft shadow map for first  split.
			* RENDERVIEW_SHADOWMAP_2_ID - Craft shadow map for second split.
			* RENDERVIEW_SHADOWMAP_3_ID - Craft shadow map for third  split.
			* RENDERVIEW_SHADOWMAP_4_ID - Craft shadow map for fourth split.
			* RENDERVIEW_VBLUR_0_ID - Vertical   blur for first  split.
			* RENDERVIEW_HBLUR_0_ID - Horizontal blur for first  split.
			* RENDERVIEW_VBLUR_1_ID - Vertical   blur for second split.
			* RENDERVIEW_HBLUR_1_ID - Horizontal blur for second split.
			* RENDERVIEW_VBLUR_2_ID - Vertical   blur for third  split.
			* RENDERVIEW_HBLUR_2_ID - Horizontal blur for third  split.
			* RENDERVIEW_VBLUR_3_ID - Vertical   blur for fourth split.
			* RENDERVIEW_HBLUR_3_ID - Horizontal blur for fourth split.
			* RENDERVIEW_DRAWSCENE_0_ID - Draw scene.
			* RENDERVIEW_DRAWSCENE_1_ID - Draw floor bottom.
			* RENDERVIEW_DRAWDEPTH_0_ID - Draw depth buffer for first  split.
			* RENDERVIEW_DRAWDEPTH_1_ID - Draw depth buffer for second split.
			* RENDERVIEW_DRAWDEPTH_2_ID - Draw depth buffer for third  split.
			* RENDERVIEW_DRAWDEPTH_3_ID - Draw depth buffer for fourth split.
			*/

		depthRectHeight = processWindow._height / 3;
		depthRectWidth = depthRectHeight;
		depthRectX = 0;
		depthRectY = processWindow._height - depthRectHeight;

		bgfx::setViewRect(RENDERVIEW_SHADOWMAP_1_ID, 0, 0, FShadowPipline::s_currentShadowMapSize, FShadowPipline::s_currentShadowMapSize);
		bgfx::setViewRect(RENDERVIEW_SHADOWMAP_2_ID, 0, 0, FShadowPipline::s_currentShadowMapSize, FShadowPipline::s_currentShadowMapSize);
		bgfx::setViewRect(RENDERVIEW_SHADOWMAP_3_ID, 0, 0, FShadowPipline::s_currentShadowMapSize, FShadowPipline::s_currentShadowMapSize);
		bgfx::setViewRect(RENDERVIEW_SHADOWMAP_4_ID, 0, 0, FShadowPipline::s_currentShadowMapSize, FShadowPipline::s_currentShadowMapSize);
		bgfx::setViewRect(RENDERVIEW_VBLUR_0_ID, 0, 0, FShadowPipline::s_currentShadowMapSize, FShadowPipline::s_currentShadowMapSize);
		bgfx::setViewRect(RENDERVIEW_HBLUR_0_ID, 0, 0, FShadowPipline::s_currentShadowMapSize, FShadowPipline::s_currentShadowMapSize);
		bgfx::setViewRect(RENDERVIEW_VBLUR_1_ID, 0, 0, FShadowPipline::s_currentShadowMapSize, FShadowPipline::s_currentShadowMapSize);
		bgfx::setViewRect(RENDERVIEW_HBLUR_1_ID, 0, 0, FShadowPipline::s_currentShadowMapSize, FShadowPipline::s_currentShadowMapSize);
		bgfx::setViewRect(RENDERVIEW_VBLUR_2_ID, 0, 0, FShadowPipline::s_currentShadowMapSize, FShadowPipline::s_currentShadowMapSize);
		bgfx::setViewRect(RENDERVIEW_HBLUR_2_ID, 0, 0, FShadowPipline::s_currentShadowMapSize, FShadowPipline::s_currentShadowMapSize);
		bgfx::setViewRect(RENDERVIEW_VBLUR_3_ID, 0, 0, FShadowPipline::s_currentShadowMapSize, FShadowPipline::s_currentShadowMapSize);
		bgfx::setViewRect(RENDERVIEW_HBLUR_3_ID, 0, 0, FShadowPipline::s_currentShadowMapSize, FShadowPipline::s_currentShadowMapSize);
		bgfx::setViewRect(RENDERVIEW_DRAWSCENE_0_ID, 0, 0, processWindow._width, processWindow._height);
		bgfx::setViewRect(RENDERVIEW_DRAWSCENE_1_ID, 0, 0, processWindow._width, processWindow._height);
		bgfx::setViewRect(RENDERVIEW_DRAWDEPTH_0_ID, depthRectX + (0 * depthRectWidth), depthRectY, depthRectWidth, depthRectHeight);
		bgfx::setViewRect(RENDERVIEW_DRAWDEPTH_1_ID, depthRectX + (1 * depthRectWidth), depthRectY, depthRectWidth, depthRectHeight);
		bgfx::setViewRect(RENDERVIEW_DRAWDEPTH_2_ID, depthRectX + (2 * depthRectWidth), depthRectY, depthRectWidth, depthRectHeight);
		bgfx::setViewRect(RENDERVIEW_DRAWDEPTH_3_ID, depthRectX + (3 * depthRectWidth), depthRectY, depthRectWidth, depthRectHeight);

		bgfx::setViewTransform(RENDERVIEW_SHADOWMAP_1_ID, FShadowPipline::s_lightView[0].Data(), FShadowPipline::s_lightProj[0].Data());
		bgfx::setViewTransform(RENDERVIEW_SHADOWMAP_2_ID, FShadowPipline::s_lightView[0].Data(), FShadowPipline::s_lightProj[1].Data());
		bgfx::setViewTransform(RENDERVIEW_SHADOWMAP_3_ID, FShadowPipline::s_lightView[0].Data(), FShadowPipline::s_lightProj[2].Data());
		bgfx::setViewTransform(RENDERVIEW_SHADOWMAP_4_ID, FShadowPipline::s_lightView[0].Data(), FShadowPipline::s_lightProj[3].Data());
		bgfx::setViewTransform(RENDERVIEW_VBLUR_0_ID, _screenView.Data(), _screenProj.Data());
		bgfx::setViewTransform(RENDERVIEW_HBLUR_0_ID, _screenView.Data(), _screenProj.Data());
		bgfx::setViewTransform(RENDERVIEW_VBLUR_1_ID, _screenView.Data(), _screenProj.Data());
		bgfx::setViewTransform(RENDERVIEW_HBLUR_1_ID, _screenView.Data(), _screenProj.Data());
		bgfx::setViewTransform(RENDERVIEW_VBLUR_2_ID, _screenView.Data(), _screenProj.Data());
		bgfx::setViewTransform(RENDERVIEW_HBLUR_2_ID, _screenView.Data(), _screenProj.Data());
		bgfx::setViewTransform(RENDERVIEW_VBLUR_3_ID, _screenView.Data(), _screenProj.Data());
		bgfx::setViewTransform(RENDERVIEW_HBLUR_3_ID, _screenView.Data(), _screenProj.Data());
		bgfx::setViewTransform(RENDERVIEW_DRAWSCENE_0_ID, transposeViewMatrix.Data(), projectionMatrix.Data());
		bgfx::setViewTransform(RENDERVIEW_DRAWSCENE_1_ID, transposeViewMatrix.Data(), projectionMatrix.Data());
		bgfx::setViewTransform(RENDERVIEW_DRAWDEPTH_0_ID, _screenView.Data(), _screenProj.Data());
		bgfx::setViewTransform(RENDERVIEW_DRAWDEPTH_1_ID, _screenView.Data(), _screenProj.Data());
		bgfx::setViewTransform(RENDERVIEW_DRAWDEPTH_2_ID, _screenView.Data(), _screenProj.Data());
		bgfx::setViewTransform(RENDERVIEW_DRAWDEPTH_3_ID, _screenView.Data(), _screenProj.Data());

		bgfx::setViewFrameBuffer(RENDERVIEW_SHADOWMAP_1_ID, FShadowPipline::s_rtShadowMap[0]);
		bgfx::setViewFrameBuffer(RENDERVIEW_SHADOWMAP_2_ID, FShadowPipline::s_rtShadowMap[1]);
		bgfx::setViewFrameBuffer(RENDERVIEW_SHADOWMAP_3_ID, FShadowPipline::s_rtShadowMap[2]);
		bgfx::setViewFrameBuffer(RENDERVIEW_SHADOWMAP_4_ID, FShadowPipline::s_rtShadowMap[3]);
		bgfx::setViewFrameBuffer(RENDERVIEW_VBLUR_0_ID, FShadowPipline::s_rtBlur);         //vblur
		bgfx::setViewFrameBuffer(RENDERVIEW_HBLUR_0_ID, FShadowPipline::s_rtShadowMap[0]); //hblur
		bgfx::setViewFrameBuffer(RENDERVIEW_VBLUR_1_ID, FShadowPipline::s_rtBlur);         //vblur
		bgfx::setViewFrameBuffer(RENDERVIEW_HBLUR_1_ID, FShadowPipline::s_rtShadowMap[1]); //hblur
		bgfx::setViewFrameBuffer(RENDERVIEW_VBLUR_2_ID, FShadowPipline::s_rtBlur);         //vblur
		bgfx::setViewFrameBuffer(RENDERVIEW_HBLUR_2_ID, FShadowPipline::s_rtShadowMap[2]); //hblur
		bgfx::setViewFrameBuffer(RENDERVIEW_VBLUR_3_ID, FShadowPipline::s_rtBlur);         //vblur
		bgfx::setViewFrameBuffer(RENDERVIEW_HBLUR_3_ID, FShadowPipline::s_rtShadowMap[3]); //hblur
	}


	// Clear backbuffer at beginning.
	bgfx::setViewClear(RENDERVIEW_SHADOW_BEGIN
		, BGFX_CLEAR_COLOR
		| BGFX_CLEAR_DEPTH
		, FShadowPipline::s_clearValues.m_clearRgba
		, FShadowPipline::s_clearValues.m_clearDepth
		, FShadowPipline::s_clearValues.m_clearStencil
	);
	//bgfx::touch(0);

	// Clear shadowmap rendertarget at beginning.
	const uint8_t flags0 = (ELightType::DirectionalLight == FShadowPipline::s_shadowSceneSettings.m_lightType)
		? 0
		: BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH | BGFX_CLEAR_STENCIL
		;

	bgfx::setViewClear(RENDERVIEW_SHADOWMAP_0_ID
		, flags0
		, 0xfefefefe //blur fails on completely white regions
		, FShadowPipline::s_clearValues.m_clearDepth
		, FShadowPipline::s_clearValues.m_clearStencil
	);
	bgfx::touch(RENDERVIEW_SHADOWMAP_0_ID);

	const uint8_t flags1 = (ELightType::DirectionalLight == FShadowPipline::s_shadowSceneSettings.m_lightType)
		? BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH
		: 0
		;

	for (uint8_t ii = 0; ii < 4; ++ii)
	{
		bgfx::setViewClear(RENDERVIEW_SHADOWMAP_1_ID + ii
			, flags1
			, 0xfefefefe //blur fails on completely white regions
			, FShadowPipline::s_clearValues.m_clearDepth
			, FShadowPipline::s_clearValues.m_clearStencil
		);
		bgfx::touch(RENDERVIEW_SHADOWMAP_1_ID + ii);
	}


	EPackDepth::Data depthType = (EShadowMapImpl::VSM == FShadowPipline::s_shadowSceneSettings.m_smImpl) ? EPackDepth::VSM : EPackDepth::RGBA;
	bool bVsmOrEsm = (EShadowMapImpl::VSM == FShadowPipline::s_shadowSceneSettings.m_smImpl) || (EShadowMapImpl::ESM == FShadowPipline::s_shadowSceneSettings.m_smImpl);

	// Blur shadow map.
	if (bVsmOrEsm
		&&  currentShadowMapSettings->m_doBlur)
	{
		bgfx::setTexture(4, FShadowPipline::us_shadowMap[0], bgfx::getTexture(FShadowPipline::s_rtShadowMap[0]));
		bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
		screenSpaceQuad(currentShadowMapSizef, currentShadowMapSizef, FRendererDef::s_flipV);
		bgfx::submit(RENDERVIEW_VBLUR_0_ID, _programs._vBlur[depthType].GetProgram());

		bgfx::setTexture(4, FShadowPipline::us_shadowMap[0], bgfx::getTexture(FShadowPipline::s_rtBlur));
		bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
		screenSpaceQuad(currentShadowMapSizef, currentShadowMapSizef, FRendererDef::s_flipV);
		bgfx::submit(RENDERVIEW_HBLUR_0_ID, _programs._hBlur[depthType].GetProgram());

		if (ELightType::DirectionalLight == FShadowPipline::s_shadowSceneSettings.m_lightType)
		{
			for (uint8_t ii = 1, jj = 2; ii < FShadowPipline::s_shadowSceneSettings.m_numSplits; ++ii, jj += 2)
			{
				const uint8_t viewId = RENDERVIEW_VBLUR_0_ID + jj;

				bgfx::setTexture(4, FShadowPipline::us_shadowMap[0], bgfx::getTexture(FShadowPipline::s_rtShadowMap[ii]));
				bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
				screenSpaceQuad(currentShadowMapSizef, currentShadowMapSizef, FRendererDef::s_flipV);
				bgfx::submit(viewId, _programs._vBlur[depthType].GetProgram());

				bgfx::setTexture(4, FShadowPipline::us_shadowMap[0], bgfx::getTexture(FShadowPipline::s_rtBlur));
				bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
				screenSpaceQuad(currentShadowMapSizef, currentShadowMapSizef, FRendererDef::s_flipV);
				bgfx::submit(viewId + 1, _programs._hBlur[depthType].GetProgram());
			}
		}
	}

	// Draw depth rect.
	if (FShadowPipline::s_shadowSceneSettings.m_drawDepthBuffer)
	{
		bgfx::setTexture(4, FShadowPipline::us_shadowMap[0], bgfx::getTexture(FShadowPipline::s_rtShadowMap[0]));
		bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
		screenSpaceQuad(currentShadowMapSizef, currentShadowMapSizef, FRendererDef::s_flipV);
		bgfx::submit(RENDERVIEW_DRAWDEPTH_0_ID, _programs._drawDepth[depthType].GetProgram());

		if (ELightType::DirectionalLight == FShadowPipline::s_shadowSceneSettings.m_lightType)
		{
			for (uint8_t ii = 1; ii < FShadowPipline::s_shadowSceneSettings.m_numSplits; ++ii)
			{
				bgfx::setTexture(4, FShadowPipline::us_shadowMap[0], bgfx::getTexture(FShadowPipline::s_rtShadowMap[ii]));
				bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
				screenSpaceQuad(currentShadowMapSizef, currentShadowMapSizef, FRendererDef::s_flipV);
				bgfx::submit(RENDERVIEW_DRAWDEPTH_0_ID + ii, _programs._drawDepth[depthType].GetProgram());
			}
		}
	}

	// Craft shadow map.
	{
		// Craft stencil mask for point light shadow map packing.
		if (ELightType::PointLight == FShadowPipline::s_shadowSceneSettings.m_lightType && FShadowPipline::s_shadowSceneSettings.m_stencilPack)
		{
			if (6 == bgfx::getAvailTransientVertexBuffer(6, _shadowPosLayout))
			{
				struct Pos
				{
					float m_x, m_y, m_z;
				};

				bgfx::TransientVertexBuffer vb;
				bgfx::allocTransientVertexBuffer(&vb, 6, _shadowPosLayout);
				Pos* vertex = (Pos*)vb.data;

				const float min = 0.0f;
				const float max = 1.0f;
				const float center = 0.5f;
				const float zz = 0.0f;

				vertex[0].m_x = min;
				vertex[0].m_y = min;
				vertex[0].m_z = zz;

				vertex[1].m_x = max;
				vertex[1].m_y = min;
				vertex[1].m_z = zz;

				vertex[2].m_x = center;
				vertex[2].m_y = center;
				vertex[2].m_z = zz;

				vertex[3].m_x = center;
				vertex[3].m_y = center;
				vertex[3].m_z = zz;

				vertex[4].m_x = max;
				vertex[4].m_y = max;
				vertex[4].m_z = zz;

				vertex[5].m_x = min;
				vertex[5].m_y = max;
				vertex[5].m_z = zz;

				bgfx::setState(0);
				bgfx::setStencil(BGFX_STENCIL_TEST_ALWAYS
					| BGFX_STENCIL_FUNC_REF(1)
					| BGFX_STENCIL_FUNC_RMASK(0xff)
					| BGFX_STENCIL_OP_FAIL_S_REPLACE
					| BGFX_STENCIL_OP_FAIL_Z_REPLACE
					| BGFX_STENCIL_OP_PASS_Z_REPLACE
				);
				bgfx::setVertexBuffer(0, &vb);
				bgfx::submit(RENDERVIEW_SHADOWMAP_0_ID, _programs._black.GetProgram());
			}
		}
	}
}

}