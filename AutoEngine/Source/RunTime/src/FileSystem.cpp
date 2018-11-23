

#ifdef __ANDROID__
#	include <SDL2/SDL_rwops.h>
#endif

#ifndef MINI_AUTO
#	include <SDL2/SDL_filesystem.h>
#endif

#include <sys/stat.h>
#include <cstdio>

#ifdef _WIN32
#	ifndef _MSC_VER
#		define _WIN32_IE 0x501
#	endif
#	include <windows.h>
#	include <shellapi.h>
#	include <direct.h>
#	include <shlobj.h>
#	include <sys/types.h>
#	include <sys/utime.h>
#else
#	include <dirent.h>
#	include <cerrno>
#	include <unistd.h>
#	include <utime.h>
#	include <sys/wait.h>
#	define MAX_PATH 256
#endif

#if defined(__APPLE__)
#	include <mach-o/dyld.h>
#endif

#include "FileSystem.h"

extern "C"
{
#ifdef __ANDROID__
	const char* SDL_Android_GetFilesDir();
	char** SDL_Android_GetFileList(const char* path, int* count);
	void SDL_Android_FreeFileList(char*** array, int* count);
#elif defined(IOS) || defined(TVOS)
	const char* SDL_IOS_GetResourceDir();
	const char* SDL_IOS_GetDocumentsDir();
#endif
}

namespace Auto3D {

FileSystem::FileSystem(Ambient* ambient)
	:Super(ambient)
{

}

FileSystem::~FileSystem()
{
}

const WSTRING FileSystem::GetUserDocumentsDir()
{
#if defined(__ANDROID__)
	return AddTrailingSlash(SDL_Android_GetFilesDir());
#elif defined(IOS) || defined(TVOS)
	return AddTrailingSlash(SDL_IOS_GetDocumentsDir());
#elif defined(_WIN32)
	wchar_t pathName[MAX_PATH];
	pathName[0] = 0;
	SHGetSpecialFolderPathW(nullptr, pathName, CSIDL_PERSONAL, 0);
	return AddTrailingSlash(WSTRING(pathName));
#else
	char pathName[MAX_PATH];
	pathName[0] = 0;
	strcpy(pathName, getenv("HOME"));
	return AddTrailingSlash(STRING(pathName));
#endif
}

const WSTRING FileSystem::GetProgramDir()
{
#if defined(__ANDROID__)
	// This is an internal directory specifier pointing to the assets in the .apk
	// Files from this directory will be opened using special handling
	return APK;
#elif defined(IOS) || defined(TVOS)
	return AddTrailingSlash(SDL_IOS_GetResourceDir());
#elif defined(_WIN32)
	wchar_t exeName[MAX_PATH];
	exeName[0] = 0;
	GetModuleFileNameW(nullptr, exeName, MAX_PATH);
	OutPutString(exeName);
	return getPath(WSTRING(exeName));
#elif defined(__APPLE__)
	char exeName[MAX_PATH];
	memset(exeName, 0, MAX_PATH);
	unsigned size = MAX_PATH;
	_NSGetExecutablePath(exeName, &size);
	return getPath(String(exeName));
#elif defined(__linux__)
	char exeName[MAX_PATH];
	memset(exeName, 0, MAX_PATH);
	pid_t pid = getpid();
	String link = "/proc/" + String(pid) + "/exe";
	readlink(link.CString(), exeName, MAX_PATH);
	return GetPath(String(exeName));
#else
	return GetCurrentDir();
#endif
}

STRING FileSystem::AddTrailingSlash(const STRING& pathName)
{
	STRING ret = pathName;
	replace(ret.begin(), ret.end(), '\\', '/');
	if (!ret.empty() && ret.back() != '/')
		ret += '/';
	return ret;
}

WSTRING FileSystem::AddTrailingSlash(const WSTRING& pathName)
{
	WSTRING ret = pathName;
	replace(ret.begin(), ret.end(), '\\', '/');
	if (!ret.empty() && ret.back() != '/')
		ret += '/';
	return ret;
}

template<typename _Ty> void FileSystem::splitPath(const _Ty& fullPath, _Ty& pathName, _Ty& fileName, _Ty& extension, bool lowercaseExtension)
{
	_Ty fullPathCopy = getInternalPath(fullPath);
	unsigned extPos = fullPathCopy.find_last_of('.');
	unsigned pathPos = fullPathCopy.find_last_of('/');

	if (extPos != _Ty::npos && (pathPos == _Ty::npos || extPos > pathPos))
	{
		extension = fullPathCopy.substr(extPos);
		if (lowercaseExtension)
			for (int i = 0; i < extension.size(); i++) 
				tolower(extension[i]);
		fullPathCopy = fullPathCopy.substr(0, extPos);
	}
	else
		extension.clear();

	pathPos = fullPathCopy.find_last_of('/');
	if (pathPos != _Ty::npos)
	{
		fileName = fullPathCopy.substr(pathPos + 1);
		pathName = fullPathCopy.substr(0, pathPos + 1);
	}
	else
	{
		fileName = fullPathCopy;
		pathName.clear();
	}
}

STRING FileSystem::getPath(const STRING& fullPath)
{
	STRING path, file, extension;
	splitPath(fullPath, path, file, extension);
	return path;
}

WSTRING FileSystem::getPath(const WSTRING& fullPath)
{
	WSTRING path, file, extension;
	splitPath(fullPath, path, file, extension);
	return path;
	return fullPath;
}


STRING FileSystem::getInternalPath(const STRING& pathName)
{
	STRING ret = pathName;
	replace(ret.begin(), ret.end(), '\\', '/');
	return ret;
}

WSTRING FileSystem::getInternalPath(const WSTRING& pathName)
{
	WSTRING ret = pathName;
	replace(ret.begin(), ret.end(), '\\', '/');
	return ret;
}

STRING FileSystem::getNativePath(const STRING& pathName)
{
	STRING ret = pathName;
	replace(ret.begin(), ret.end(), '/', '\\');
	return ret;
}

WSTRING FileSystem::getNativePath(const WSTRING& pathName)
{
	WSTRING ret = pathName;
	replace(ret.begin(), ret.end(), '/', '\\');
	return ret;
}

}