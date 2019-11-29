#pragma once

#include "Container/HashMap.h"

namespace Auto3D
{

class FAttribute;
class Serializable;
struct ObjectRef;

/// Stored object ref attribute.
struct AUTO_API StoredObjectRef
{
    /// Construct undefined.
    StoredObjectRef() :
        _object(nullptr),
        _attr(nullptr),
        _oldId(0)
    {
    }

    /// Construct with values.
    StoredObjectRef(Serializable* object, FAttribute* attr, unsigned oldId) :
        _object(object),
        _attr(attr),
        _oldId(oldId)
    {
    }

    /// %AObject that contains the attribute.
    Serializable* _object;
    /// Description of the object ref attribute.
    FAttribute* _attr;
    /// Old _id from the serialized data.
    unsigned _oldId;
};

/// Helper class for resolving object ref attributes when loading a scene.
class AUTO_API ObjectResolver
{
public:
    /// Store an object along with its old _id from the serialized data.
    void StoreObject(unsigned oldId, Serializable* object);
    /// Store an object ref attribute that needs to be resolved later.
    void StoreObjectRef(Serializable* object, FAttribute* attr, const ObjectRef& value);
    /// Resolve the object ref attributes.
    void Resolve();

private:
    /// Mapping of old _id's to objects.
    THashMap<unsigned, Serializable*> _objects;
    /// Stored object ref attributes.
    TVector<StoredObjectRef> _objectRefs;
};

}
