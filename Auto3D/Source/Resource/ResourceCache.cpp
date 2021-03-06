#include "Debug/Log.h"
#include "Debug/Profiler.h"
#include "IO/File.h"
#include "IO/FileSystem.h"
#include "IO/JSONFile.h"
#include "ResourceCache.h"

#include "Adapter/Ptr.h"

#include "Debug/DebugNew.h"

namespace Auto3D
{

IMPLEMENT_SINGLETON(GResourceModule)

GResourceModule::GResourceModule()
{
	AddResourceDir(ExecutableDir() + "Data");
}

GResourceModule::~GResourceModule()
{
    UnloadAllResources(true);
}

bool GResourceModule::AddResourceDir(const FString& pathName, bool addFirst)
{
    PROFILE(AddResourceDir);

    if (!DirExists(pathName))
    {
        ErrorString("Could not open directory " + pathName);
        return false;
    }

    FString fixedPath = SanitateResourceDirName(pathName);

    // Check that the same path does not already exist
    for (size_t i = 0; i < _resourceDirs.Size(); ++i)
    {
        if (!_resourceDirs[i].Compare(fixedPath, false))
            return true;
    }

    if (addFirst)
        _resourceDirs.Insert(0, fixedPath);
    else
        _resourceDirs.Push(fixedPath);

    InfoString("Added resource path " + fixedPath);
    return true;
}

bool GResourceModule::AddManualResource(OResource* resource)
{
    if (!resource)
    {
        ErrorString("Null manual resource");
        return false;
    }

    if (resource->GetPathName().IsEmpty())
    {
        ErrorString("Manual resource with empty name, can not add");
        return false;
    }

    _resources[MakePair(resource->GetTypeName(), resource->GetPathName())] = resource;
    return true;
}

void GResourceModule::RemoveResourceDir(const FString& pathName)
{
    // Convert path to absolute
    FString fixedPath = SanitateResourceDirName(pathName);

    for (size_t i = 0; i < _resourceDirs.Size(); ++i)
    {
        if (!_resourceDirs[i].Compare(fixedPath, false))
        {
            _resourceDirs.Erase(i);
            InfoString("Removed resource path " + fixedPath);
            return;
        }
    }
}

void GResourceModule::UnloadResource(FString type, const FString& name, bool force)
{
    auto key = MakePair(type, name);
    auto it = _resources.Find(key);
    if (it == _resources.End())
        return;

    OResource* resource = it->_second;
    if (force)
        _resources.Erase(key);
}

void GResourceModule::UnloadResources(FString type, bool force)
{
    // In case resources refer to other resources, _repeat until there are no further unloads
    for (;;)
    {
        size_t unloaded = 0;

        for (auto it = _resources.Begin(); it != _resources.End();)
        {
            auto current = it++;
            if (current->_first._first == type)
            {
                OResource* resource = current->_second;
                if (force)
                {
                    _resources.Erase(current);
                    ++unloaded;
                }
            }
        }

        if (!unloaded)
            break;
    }
}

void GResourceModule::UnloadResources(FString type, const FString& partialName, bool force)
{
    // In case resources refer to other resources, _repeat until there are no further unloads
    for (;;)
    {
        size_t unloaded = 0;

        for (auto it = _resources.Begin(); it != _resources.End();)
        {
            auto current  = it++;
            if (current->_first._first == type)
            {
                OResource* resource = current->_second;
                if (resource->GetPathName().StartsWith(partialName) && force)
                {
                    _resources.Erase(current);
                    ++unloaded;
                }
            }
        }

        if (!unloaded)
            break;
    }
}

void GResourceModule::UnloadResources(const FString& partialName, bool force)
{
    // In case resources refer to other resources, _repeat until there are no further unloads
    for (;;)
    {
        size_t unloaded = 0;

        for (auto it = _resources.Begin(); it != _resources.End();)
        {
            auto current = it++;
            OResource* resource = current->_second;
            if (resource->GetPathName().StartsWith(partialName) && (force))
            {
                _resources.Erase(current);
                ++unloaded;
            }
        }

        if (!unloaded)
            break;
    }
}

void GResourceModule::UnloadAllResources(bool force)
{
    // In case resources refer to other resources, _repeat until there are no further unloads
    for (;;)
    {
        size_t unloaded = 0;

        for (auto it = _resources.Begin(); it != _resources.End();)
        {
            auto current = it++;
            OResource* resource = current->_second;
            if (force)
            {
                _resources.Erase(current);
                ++unloaded;
            }
        }

        if (!unloaded)
            break;
    }
}

bool GResourceModule::ReloadResource(OResource* resource)
{
    if (!resource)
        return false;

    UPtr<FStream> stream(OpenResource(resource->GetPathName()));
    return stream ? resource->Load(stream->GetPathName()) : false;
}

OResource* GResourceModule::LoadResource(FString type, const FString& nameIn)
{
	FString name = SanitateResourceName(nameIn);

	// If empty name, return null pointer immediately without logging an error
	if (name.IsEmpty())
		return nullptr;

	// Check for existing resource
	auto key = MakePair(type, name);
	auto it = _resources.Find(key);
	if (it != _resources.End())
		return it->_second;

	OObject* newObject = NewObject(type);
	if (!newObject)
	{
		ErrorString("Could not load unknown resource type " + FString(type));
		return nullptr;
	}
	OResource* newResource = dynamic_cast<OResource*>(newObject);
	if (!newResource)
	{
		ErrorString("Type " + FString(type) + " is not a resource");
		return nullptr;
	}

	LogString("Loading resource " + name);
	newResource->SetName(name);

	FString fullPathName = ResourceFileName(name);
	if (!newResource->Load(fullPathName))
		return nullptr;

	// Store to cache
	_resources[key] = newResource;
	return newResource;
}

void GResourceModule::ResourcesByType(TVector<OResource*>& result, FString type) const
{
	result.Clear();

	for (auto it = _resources.Begin(); it != _resources.End(); ++it)
	{
		if (it->_second->GetTypeName() == type)
			result.Push(it->_second);
	}
}

FStream* GResourceModule::OpenResource(const FString& nameIn)
{
    FString name = SanitateResourceName(nameIn);
    FStream* ret;

    for (size_t i = 0; i < _resourceDirs.Size(); ++i)
    {
        if (FileExists(_resourceDirs[i] + name))
        {
            // Construct the file first with full path, then rename it to not contain the resource path,
            // so that the file's name can be used in further OpenResource() calls (for example over the network)
            ret = new FFile(_resourceDirs[i] + name);
            break;
        }
    }

    // Fallback using absolute path
    if (!ret)
        ret = new FFile(name);

    if (!ret->IsReadable())
    {
        ErrorString("Could not open resource file " + name);
        delete ret;
    }

    return ret;
}

bool GResourceModule::Exists(const FString& nameIn) const
{
    FString name = SanitateResourceName(nameIn);

    for (size_t i = 0; i < _resourceDirs.Size(); ++i)
    {
        if (FileExists(_resourceDirs[i] + name))
            return true;
    }

    // Fallback using absolute path
    return FileExists(name);
}

FString GResourceModule::ResourceFileName(const FString& name) const
{
    for (unsigned i = 0; i < _resourceDirs.Size(); ++i)
    {
        if (FileExists(_resourceDirs[i] + name))
            return _resourceDirs[i] + name;
    }

    return name;
}

FString GResourceModule::SanitateResourceName(const FString& nameIn) const
{
    // Sanitate unsupported constructs from the resource name
    FString name = NormalizePath(nameIn);
    name.Replace("../", "");
    name.Replace("./", "");

    // If the path refers to one of the resource directories, normalize the resource name
    if (_resourceDirs.Size())
    {
        FString namePath = Path(name);
        FString exePath = ExecutableDir();
        for (unsigned i = 0; i < _resourceDirs.Size(); ++i)
        {
            FString relativeResourcePath = _resourceDirs[i];
            if (relativeResourcePath.StartsWith(exePath))
                relativeResourcePath = relativeResourcePath.Substring(exePath.Length());

            if (namePath.StartsWith(_resourceDirs[i], false))
                namePath = namePath.Substring(_resourceDirs[i].Length());
            else if (namePath.StartsWith(relativeResourcePath, false))
                namePath = namePath.Substring(relativeResourcePath.Length());
        }

        name = namePath + FileNameAndExtension(name);
    }

    return name.Trimmed();
}

FString GResourceModule::SanitateResourceDirName(const FString& nameIn) const
{
    // Convert path to absolute
    FString fixedPath = AddTrailingSlash(nameIn);
    if (!IsAbsolutePath(fixedPath))
        fixedPath = FFileModule::Get().GetCurrentDir() + fixedPath;

    // Sanitate away /./ construct
    fixedPath.Replace("/./", "/");

    return fixedPath.Trimmed();
}

}
