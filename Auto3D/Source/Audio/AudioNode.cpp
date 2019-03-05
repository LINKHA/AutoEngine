#include "AudioNode.h"
#include "Audio.h"

namespace Auto3D
{

AudioNode::AudioNode() :
	_fre(AudioFrequency::Static)
{
	_audio = Subsystem<Audio>();

}
AudioNode::~AudioNode()
{

}

void AudioNode::RegisterObject()
{
	RegisterFactory<AudioNode>();
}

	



}