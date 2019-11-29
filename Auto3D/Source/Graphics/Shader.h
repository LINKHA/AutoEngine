#pragma once

#include "Resource/Resource.h"
#include "GraphicsDefs.h"

namespace Auto3D
{

class ShaderVariation;

/// %Shader resource. Defines either vertex or pixel shader source code, from which variations can be compiled by specifying defines.
class AUTO_API Shader : public AResource
{
    REGISTER_OBJECT_CLASS(Shader,AResource)

public:
    /// Construct.
    Shader();
    /// Destruct.
    ~Shader();

    /// Register object factory.
    static void RegisterObject();

    /// Load shader code from a stream. Return true on success.
    bool BeginLoad(Stream& source) override;
    /// Finish shader loading in the main thread. Return true on success.
    bool EndLoad() override;

    /// Define shader stage and source code. All existing variations are destroyed.
    void Define(ShaderStage::Type stage, const FString& code);
    /// Create and return a variation with defines, eg. "PERPIXEL NORMALMAP NUMLIGHTS=4". Existing variation is returned if possible. Variations should be cached to avoid repeated query.
    ShaderVariation* CreateVariation(const FString& defines = FString::EMPTY);
    
    /// Return shader stage.
    ShaderStage::Type GetStage() const { return _stage; }
    /// Return shader source code.
    const FString& GetSourceCode() const { return _sourceCode; }

    /// Sort the defines and strip extra spaces to prevent creation of unnecessary duplicate shader variations. When requesting variations, the defines should preferably be normalized already to save time.
    static FString NormalizeDefines(const FString& defines);

private:
    /// Process include statements in the shader source code recursively. Return true if successful.
    bool ProcessIncludes(FString& code, Stream& source);

    /// %Shader variations.
    THashMap<FStringHash, TSharedPtr<ShaderVariation> > _variations;
    /// %Shader stage.
    ShaderStage::Type _stage;
    /// %Shader source code.
    FString _sourceCode;
};

}
