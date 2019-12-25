#pragma once
#include "Scene/SpatialNode.h"

namespace Auto3D
{
/// AAudio fUsage 
namespace EAudioUsage
{
	enum Type
	{
		STATIC = 0,
		DYNAMIC,
	};
};

class FAudioModule;

class AUTO_API AAudioNode : public ASpatialNode
{
	DECLARE_CLASS(AAudioNode, ASpatialNode)
public:
	/// Constructor
	AAudioNode();
	/// Destructor
	~AAudioNode();
	/// Set audio Usage
	void SetUsage(EAudioUsage::Type fre) { _fre = fre; }
	/// Get audio Usage
	EAudioUsage::Type GetUsage() { return _fre; }
protected:
	/// AAudio weak ptr
	TWeakPtr<FAudioModule> _audio;
	/// AAudio Usage 
	EAudioUsage::Type _fre;
};

}