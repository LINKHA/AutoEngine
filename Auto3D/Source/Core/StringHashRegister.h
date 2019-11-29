
#pragma once

#include "Container/HashMap.h"
#include "Container/Ptr.h"
#include "Container/StringHash.h"
#include "Container/Singleton.h"

namespace Auto3D
{

class Mutex;

template <typename _Ty1, typename _Ty2> class THashMap;
/// Map of strings.
using StringMap = THashMap<FStringHash, FString>;

/// Helper class used for FStringHash reversing.
class AUTO_API StringHashRegister
{
	friend class TSingleton<StringHashRegister>;
private:
	/// Construct. threadSafe controls whether the RegisterString and GetStringCopy are thread-safe.
	explicit StringHashRegister(bool threadSafe = true);
	/// Destruct.
	~StringHashRegister();
public:
	/// Gets the singleton instance of the module manager.
	static StringHashRegister& Get();

	/// Register string for hash reverse mapping. Could be used from FStringHash ctor.
	FStringHash RegisterString(const FStringHash& hash, const char* string);
	/// Register string for hash reverse mapping.
	FStringHash RegisterString(const char* string);
	/// Register string for hash reverse mapping. 
	bool RemoveString(const FStringHash& hash);
	/// Register string for hash reverse mapping. 
	bool RemoveString(const char* string);

	/// Return string for given FStringHash. Return empty string if not found.
	FString GetStringCopy(const FStringHash& hash) const;
	/// Return whether the string in contained in the register.
	bool Contains(const FStringHash& hash) const;

	/// Return FString for given FStringHash. Return value is unsafe to use if RegisterString is called from other threads.
	const FString& GetString(const FStringHash& hash) const;
	/// Return map of hashes. Return value is unsafe to use if RegisterString is called from other threads.
	const StringMap& GetInternalMap() const { return _map; }

private:
	/// Hash to string map.
	StringMap _map;
	/// Mutex.
	TUniquePtr<Mutex> _mutex;
};

}
