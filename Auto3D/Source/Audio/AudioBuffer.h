#pragma once
#include "../Resource/Resource.h"

namespace Auto3D 
{

class AudioBuffer : public Resource
{
	REGISTER_OBJECT_CLASS(AudioBuffer, Resource)
public:
	AudioBuffer();

	~AudioBuffer();
	/**
	* @brief : Register object factory
	*/
	static void RegisterObject();
	/*
	* @brief : Load resource from stream.May be called from a worker thread.Return true if successful
	*/
	bool BeginLoad(Stream& source)override;
	/**
	* @brief : Set data with void*
	*/
	void SetData(void* data) { _data = data; }
	/**
	* @brief : Set size with long
	*/
	void SetSize(long size) { _size = size; }
	/**
	* @brief : Get buffer data with void*
	*/
	void* GetData() { return _data; }
	/**
	* @brief : Get buffer size with long
	*/
	long GetSize() { return _size; }
private:
	/// buffer data
	void* _data;
	/// buffer size
	long _size;
};

}