#pragma once
#include "RefCounted.h"
#include "Object.h"
#include "stl_use.h"
namespace Auto3D {
class Ambient : public RefCounted
{
	friend class Object;
	using SubSystems = AUTO_HASH_MAP(_String, SharedPtr<Object>);
	using Factories = AUTO_HASH_MAP(_String, SharedPtr<ObjectFactory>);
	using ObjectCategories = AUTO_HASH_MAP(_String, _VECTOR(_String));
public:
	Ambient();
	~Ambient()override;
	/**
	* @brief : Create an object by type. Return pointer to it or null if no factory found.
	*/
	template <class T> inline SharedPtr<T> CreateObject(){ return static_cast<T>(CreateObject(T::GetClassStringStatic())); }
	/**
	* @brief : Create object by type hash , Return pointer to it or null if no factory found.
	*/
	SharedPtr<Object> CreateObject(_String objectType);
	/**
	* @brief : Register sub system need extend object
	*/
	void RegisterSubSystem(Object* object);
	/**
	* @brief : Template version of registering subsystem.
	*/
	template <class T> T* RegisterSubsystem();
	/**
	* @brief : Register a factory for an object type.
	*/
	void RegisterFactory(ObjectFactory* factory);
	/**
	* @brief : Register a factory for an object type and specify the object category.
	*/
	void RegisterFactory(ObjectFactory* factory, const char* category);
	/**
	* @brief : Template version of registering an object factory.
	*/
	template <class T> void RegisterFactory();
	/**
	* @brief : Template version of registering an object factory with category.
	*/
	template <class T> void RegisterFactory(const char* category);
	/**
	* @brief : Get sub system according to the parameters
	*/
	Object* GetSubSystem(_String type) const;
	/**
	* @brief : Get sub systems
	* @return :AUTO_HASH_MAP(_String, SharedPtr<Object>)
	*/
	const SubSystems& GetSubSystems() const { return _subSystems; }
	/**
	* @brief : Get sub system by type
	*/
	template<typename T> T* GetSubSystem() const;
	/**
	* @brief : Return all subsystems.
	*/
	const SubSystems& GetSubsystems() const { return _subSystems; }
	/**
	* @brief : Return all object factories.
	*/
	const Factories& GetObjectFactories() const { return _factories; }
	/**
	* @brief : Remove a sub system from _subSystems
	*/
	void RemoveSubSystem(_String objectType);
	/**
	* @brief : Template version of removing a subsystem.
	*/
	template <class T> void RemoveSubsystem();
private:
	///sub systems hash map
	SubSystems _subSystems;
	///object factories hash map
	Factories _factories;
	///object categories hasm map
	ObjectCategories _objectCategories;
};

template <typename T>
inline T * Ambient::RegisterSubsystem()
{
	auto* subsystem = new T(this);
	RegisterSubsystem(subsystem);
	return subsystem;
}

template<class T> void Ambient::RemoveSubsystem(){ RemoveSubsystem(T::GetClassStringStatic()); }

template <typename T> T* Ambient::GetSubSystem() const { return static_cast<T*>(Ambient::GetSubSystem(T::GetClassStringStatic())); }

template <typename T> void Ambient::RegisterFactory() { RegisterFactory(new ObjectFactoryImpl<T>(this)); }

template<class T> void Ambient::RegisterFactory(const char * category) { RegisterFactory(new ObjectFactoryImpl<T>(this), category); }

}