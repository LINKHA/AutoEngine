#include "AudioBuffer.h"
#include "Audio.h"
#include "Sound.h"
#include "Debug/Log.h"
#include "Core/Modules/ModuleManager.h"

#include <AL/al.h>
#include <AL/alc.h>
#include <AL/alext.h>

namespace Auto3D
{

AAudioBuffer::AAudioBuffer()
{
	_audio = ModuleManager::Get().AudioModule();
}

AAudioBuffer::~AAudioBuffer()
{
	alSourceStop(_source);
	alDeleteSources(1, &_source);
	alDeleteBuffers(1, &_buffer);
}

bool AAudioBuffer::Create(ASound* sound)
{
	if (_audio && _audio->IsInitialized())
	{
		// Generate an AL Buffer
		alGenBuffers(1, &_buffer);
		alGenSources(1, &_source);
		if (!_buffer)
		{
			ErrorString("Failed to create vertex buffer");
			return false;
		}
		const ALvoid* _data = sound->GetStart();
		long dataSize = sound->GetDataSize();
		float frequency = sound->GetFrequency();
		/* for simplicity, assume raw file is signed-16b at frequency */
		alBufferData(_buffer, AL_FORMAT_MONO16, _data, dataSize, frequency * 2);
		alSourcei(_source, AL_BUFFER, _buffer);
	}

	return true;
}
}