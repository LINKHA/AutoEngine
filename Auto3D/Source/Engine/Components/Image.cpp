#include "Debug/Log.h"
#include "Debug/Profiler.h"
#include "IO/Stream.h"
#include "Math/MathDef.h"
#include "Resource/Decompress.h"

#include <cstdlib>
#include <cstring>
#include <SDL_surface.h>

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION

#include <stb_image.h>
#include <stb_image_write.h>

#include "Debug/DebugNew.h"

#ifndef MAKEFOURCC
#define MAKEFOURCC(ch0, ch1, ch2, ch3) ((unsigned)(ch0) | ((unsigned)(ch1) << 8) | ((unsigned)(ch2) << 16) | ((unsigned)(ch3) << 24))
#endif

#define FOURCC_DXT1 (MAKEFOURCC('D','X','T','1'))
#define FOURCC_DXT2 (MAKEFOURCC('D','X','T','2'))
#define FOURCC_DXT3 (MAKEFOURCC('D','X','T','3'))
#define FOURCC_DXT4 (MAKEFOURCC('D','X','T','4'))
#define FOURCC_DXT5 (MAKEFOURCC('D','X','T','5'))

namespace Auto3D
{

const int AImage::components[] =
{
    0,      // EImageFormat::NONE
    1,      // EImageFormat::R8
    2,      // EImageFormat::RG8
    4,      // EImageFormat::RGBA8
    1,      // EImageFormat::A8
    0,      // EImageFormat::R16
    0,      // EImageFormat::RG16
    0,      // EImageFormat::RGBA16
    0,      // EImageFormat::R16F
    0,      // EImageFormat::RG16F
    0,      // EImageFormat::RGBA16F
    0,      // EImageFormat::R32F
    0,      // EImageFormat::RG32F
    0,      // EImageFormat::RGB32F
    0,      // EImageFormat::RGBA32F
    0,      // EImageFormat::D16
    0,      // EImageFormat::D32
    0,      // EImageFormat::D24S8
    0,      // EImageFormat::DXT1
    0,      // EImageFormat::DXT3
    0,      // EImageFormat::DXT5
    0,      // EImageFormat::ETC1
    0,      // EImageFormat::PVRTC_RGB_2BPP
    0,      // EImageFormat::PVRTC_RGBA_2BPP
    0,      // EImageFormat::PVRTC_RGB_4BPP
    0       // EImageFormat::PVRTC_RGBA_4BPP
};

const size_t AImage::pixelByteSizes[] =
{
    0,      // EImageFormat::NONE
    1,      // EImageFormat::R8
    2,      // EImageFormat::RG8
    4,      // EImageFormat::RGBA8
    1,      // EImageFormat::A8
    2,      // EImageFormat::R16
    4,      // EImageFormat::RG16
    8,      // EImageFormat::RGBA16
    2,      // EImageFormat::R16F
    4,      // EImageFormat::RG16F
    8,      // EImageFormat::RGBA16F
    4,      // EImageFormat::R32F
    8,      // EImageFormat::RG32F
    12,     // EImageFormat::RGB32F
    16,     // EImageFormat::RGBA32F
    2,      // EImageFormat::D16
    4,      // EImageFormat::D32
    4,      // EImageFormat::D24S8
    0,      // EImageFormat::DXT1
    0,      // EImageFormat::DXT3
    0,      // EImageFormat::DXT5
    0,      // EImageFormat::ETC1
    0,      // EImageFormat::PVRTC_RGB_2BPP
    0,      // EImageFormat::PVRTC_RGBA_2BPP
    0,      // EImageFormat::PVRTC_RGB_4BPP
    0       // EImageFormat::PVRTC_RGBA_4BPP
};

static const EImageFormat::Type componentsToFormat[] =
{
    EImageFormat::NONE,
    EImageFormat::R8,
    EImageFormat::RG8,
    EImageFormat::RGBA8,
    EImageFormat::RGBA8
};

/// \cond PRIVATE
struct FDDColorKey
{
    unsigned dwColorSpaceLowValue;
    unsigned dwColorSpaceHighValue;
};
/// \endcond

/// \cond PRIVATE
struct FDDPixelFormat
{
    unsigned dwSize;
    unsigned dwFlags;
    unsigned dwFourCC;
    union
    {
        unsigned dwRGBBitCount;
        unsigned dwYUVBitCount;
        unsigned dwZBufferBitDepth;
        unsigned dwAlphaBitDepth;
        unsigned dwLuminanceBitCount;
        unsigned dwBumpBitCount;
        unsigned dwPrivateFormatBitCount;
    };
    union
    {
        unsigned dwRBitMask;
        unsigned dwYBitMask;
        unsigned dwStencilBitDepth;
        unsigned dwLuminanceBitMask;
        unsigned dwBumpDuBitMask;
        unsigned dwOperations;
    };
    union
    {
        unsigned dwGBitMask;
        unsigned dwUBitMask;
        unsigned dwZBitMask;
        unsigned dwBumpDvBitMask;
        struct
        {
            unsigned short wFlipMSTypes;
            unsigned short wBltMSTypes;
        } multiSampleCaps;
    };
    union
    {
        unsigned dwBBitMask;
        unsigned dwVBitMask;
        unsigned dwStencilBitMask;
        unsigned dwBumpLuminanceBitMask;
    };
    union
    {
        unsigned dwRGBAlphaBitMask;
        unsigned dwYUVAlphaBitMask;
        unsigned dwLuminanceAlphaBitMask;
        unsigned dwRGBZBitMask;
        unsigned dwYUVZBitMask;
    };
};
/// \endcond

/// \cond PRIVATE
struct FDDSCaps2
{
    unsigned dwCaps;
    unsigned dwCaps2;
    unsigned dwCaps3;
    union
    {
        unsigned dwCaps4;
        unsigned dwVolumeDepth;
    };
};
/// \endcond

/// \cond PRIVATE
struct FDDSurfaceDesc2
{
    unsigned dwSize;
    unsigned dwFlags;
    unsigned dwHeight;
    unsigned dwWidth;
    union
    {
        unsigned lPitch;
        unsigned dwLinearSize;
    };
    union
    {
        unsigned dwBackBufferCount;
        unsigned dwDepth;
    };
    union
    {
        unsigned dwMipMapCount;
        unsigned dwRefreshRate;
        unsigned dwSrcVBHandle;
    };
    unsigned dwAlphaBitDepth;
    unsigned dwReserved;
    unsigned lpSurface; // Do not define as a void pointer, as it is 8 bytes in a 64bit build
    union
    {
        FDDColorKey ddckCKDestOverlay;
        unsigned dwEmptyFaceColor;
    };
    FDDColorKey ddckCKDestBlt;
    FDDColorKey ddckCKSrcOverlay;
    FDDColorKey ddckCKSrcBlt;
    union
    {
        FDDPixelFormat ddpfPixelFormat;
        unsigned dwFVF;
    };
    FDDSCaps2 ddsCaps;
    unsigned dwTextureStage;
};
/// \endcond

AImage::AImage() :
    _size(TVector2I::ZERO),
    _format(EImageFormat::NONE),
    _numLevels(1)
{
}

AImage::~AImage()
{
}

void AImage::RegisterObject()
{
    RegisterFactory<AImage>();
}

bool AImage::BeginLoad(FStream& source)
{
    PROFILE(LoadImage);

    // Check for DDS, KTX or PVR compressed format
    FString fileID = source.ReadFileID();

    if (fileID == "DDS ")
    {
        // DDS compressed format
        FDDSurfaceDesc2 ddsd;
        source.Read(&ddsd, sizeof(ddsd));

#ifndef AUTO_OPENGL_ES
        switch (ddsd.ddpfPixelFormat.dwFourCC)
        {
        case FOURCC_DXT1:
            _format = EImageFormat::DXT1;
            break;

        case FOURCC_DXT3:
            _format = EImageFormat::DXT3;
            break;

        case FOURCC_DXT5:
            _format = EImageFormat::DXT5;
            break;

        default:
            ErrorString("Unsupported DDS format");
            return false;
        }
#endif
        size_t dataSize = source.Size() - source.Position();
        _data = new unsigned char[dataSize];
        _size = TVector2I(ddsd.dwWidth, ddsd.dwHeight);
        _numLevels = ddsd.dwMipMapCount ? ddsd.dwMipMapCount : 1;
        source.Read(_data.Get(), dataSize);
    }
    else if (fileID == "\253KTX")
    {
        source.Seek(12);

        unsigned endianness = source.Read<unsigned>();
        unsigned type = source.Read<unsigned>();
        /* unsigned typeSize = */ source.Read<unsigned>();
        unsigned imageFormat = source.Read<unsigned>();
        unsigned internalFormat = source.Read<unsigned>();
        /* unsigned baseInternalFormat = */ source.Read<unsigned>();
        unsigned imageWidth = source.Read<unsigned>();
        unsigned imageHeight = source.Read<unsigned>();
        unsigned depth = source.Read<unsigned>();
        /* unsigned arrayElements = */ source.Read<unsigned>();
        unsigned faces = source.Read<unsigned>();
        unsigned mipmaps = source.Read<unsigned>();
        unsigned keyValueBytes = source.Read<unsigned>();

        if (endianness != 0x04030201)
        {
            ErrorString("Big-endian KTX files not supported");
            return false;
        }

        if (type != 0 || imageFormat != 0)
        {
            ErrorString("Uncompressed KTX files not supported");
            return false;
        }

        if (faces > 1 || depth > 1)
        {
            ErrorString("3D or cube KTX files not supported");
            return false;
        }

        if (mipmaps == 0)
        {
            ErrorString("KTX files without explicitly specified mipmap count not supported");
            return false;
        }

        _format = EImageFormat::NONE;
        switch (internalFormat)
        {
#ifndef AUTO_OPENGL_ES
        case 0x83f1:
            _format = EImageFormat::DXT1;
            break;

        case 0x83f2:
            _format = EImageFormat::DXT3;
            break;

        case 0x83f3:
            _format = EImageFormat::DXT5;
            break;
#endif
        case 0x8d64:
            _format = EImageFormat::ETC1;
            break;

        case 0x8c00:
            _format = EImageFormat::PVRTC_RGB_4BPP;
            break;

        case 0x8c01:
            _format = EImageFormat::PVRTC_RGB_2BPP;
            break;

        case 0x8c02:
            _format = EImageFormat::PVRTC_RGBA_4BPP;
            break;

        case 0x8c03:
            _format = EImageFormat::PVRTC_RGBA_2BPP;
            break;
        }

        if (_format == EImageFormat::NONE)
        {
            ErrorString("Unsupported texture format in KTX file");
            return false;
        }

        source.Seek(source.Position() + keyValueBytes);
        size_t dataSize = source.Size() - source.Position() - mipmaps * sizeof(unsigned);

        _data = new unsigned char[dataSize];
        _size = TVector2I(imageWidth, imageHeight);
        _numLevels = mipmaps;

        size_t dataOffset = 0;
        for (size_t i = 0; i < mipmaps; ++i)
        {
            size_t levelSize = source.Read<unsigned>();
            if (levelSize + dataOffset > dataSize)
            {
                ErrorString("KTX mipmap level data size exceeds file size");
                return false;
            }

            source.Read(&_data[dataOffset], levelSize);
            dataOffset += levelSize;
            if (source.Position() & 3)
                source.Seek((source.Position() + 3) & 0xfffffffc);
        }
    }
    else if (fileID == "PVR\3")
    {
        /* unsigned flags = */ source.Read<unsigned>();
        unsigned pixelFormatLo = source.Read<unsigned>();
        /* unsigned pixelFormatHi = */ source.Read<unsigned>();
        /* unsigned colourSpace = */ source.Read<unsigned>();
        /* unsigned channelType = */ source.Read<unsigned>();
        unsigned imageHeight = source.Read<unsigned>();
        unsigned imageWidth = source.Read<unsigned>();
        unsigned depth = source.Read<unsigned>();
        /* unsigned numSurfaces = */ source.Read<unsigned>();
        unsigned numFaces = source.Read<unsigned>();
        unsigned mipmapCount = source.Read<unsigned>();
        unsigned metaDataSize = source.Read<unsigned>();

        if (depth > 1 || numFaces > 1)
        {
            ErrorString("3D or cube PVR files not supported");
            return false;
        }

        if (mipmapCount == 0)
        {
            ErrorString("PVR files without explicitly specified mipmap count not supported");
            return false;
        }

        _format = EImageFormat::NONE;
        switch (pixelFormatLo)
        {
        case 0:
            _format = EImageFormat::PVRTC_RGB_2BPP;
            break;

        case 1:
            _format = EImageFormat::PVRTC_RGBA_2BPP;
            break;

        case 2:
            _format = EImageFormat::PVRTC_RGB_4BPP;
            break;

        case 3:
            _format = EImageFormat::PVRTC_RGBA_4BPP;
            break;

        case 6:
            _format = EImageFormat::ETC1;
            break;
#ifndef AUTO_OPENGL_ES
        case 7:
            _format = EImageFormat::DXT1;
            break;

        case 9:
            _format = EImageFormat::DXT3;
            break;

        case 11:
            _format = EImageFormat::DXT5;
            break;
#endif
        }

        if (_format == EImageFormat::NONE)
        {
            ErrorString("Unsupported texture format in PVR file");
            return false;
        }

        source.Seek(source.Position() + metaDataSize);
        size_t dataSize = source.Size() - source.Position();

        _data = new unsigned char[dataSize];
        _size = TVector2I(imageWidth, imageHeight);
        _numLevels = mipmapCount;

        source.Read(_data.Get(), dataSize);
    }
    else
    {
        // Not DDS, KTX or PVR, use STBImage to load other image formats as uncompressed
        source.Seek(0);
        int imageWidth, imageHeight;
        unsigned imageComponents;
        unsigned char* pixelData = DecodePixelData(source, imageWidth, imageHeight, imageComponents);
        if (!pixelData)
        {
            ErrorString("Could not load image " + source.GetName() + ": " + FString(stbi_failure_reason()));
            return false;
        }
        
        SetSize(TVector2I(imageWidth, imageHeight), componentsToFormat[imageComponents]);

        if (imageComponents != 3)
            SetData(pixelData);
        else
        {
            // Convert RGB to RGBA as for example Direct3D 11 does not support 24-bit formats
            TAutoArrayPtr<unsigned char> rgbaData(new unsigned char[4 * imageWidth * imageHeight]);
            unsigned char* src = pixelData;
            unsigned char* dest = rgbaData.Get();
            for (int i = 0; i < imageWidth * imageHeight; ++i)
            {
                *dest++ = *src++;
                *dest++ = *src++;
                *dest++ = *src++;
                *dest++ = 0xff;
            }

            SetData(rgbaData.Get());
        }

        FreePixelData(pixelData);
    }

    return true;
}

bool AImage::Save(FStream& dest)
{
    PROFILE(SaveImage);

    if (IsCompressed())
    {
        ErrorString("Can not save compressed image " + GetName());
        return false;
    }

    if (!_data)
    {
        ErrorString("Can not save zero-sized image " + GetName());
        return false;
    }

    int components = (int)PixelByteSize();
    if (components < 1 || components > 4)
    {
        ErrorString("Unsupported pixel format for PNG save on image " + GetName());
        return false;
    }

    int len;
    unsigned char *png = stbi_write_png_to_mem(_data.Get(), 0, _size._x, _size._y, components, &len);
    bool success = dest.Write(png, len) == (size_t)len;
    free(png);
    return success;
}

void AImage::SetSize(const TVector2I& newSize, EImageFormat::Type newFormat)
{
    if (newSize == _size && newFormat == _format)
        return;

    if (newSize._x <= 0 || newSize._y <= 0)
    {
        ErrorString("Can not set zero or negative image size");
        return;
    }
    if (pixelByteSizes[newFormat] == 0)
    {
        ErrorString("Can not set image size with unspecified pixel byte size (including compressed formats)");
        return;
    }

    _data = new unsigned char[newSize._x * newSize._y * pixelByteSizes[newFormat]];
    _size = newSize;
    _format = newFormat;
    _numLevels = 1;
}

void AImage::SetData(const unsigned char* pixelData)
{
    if (!IsCompressed())
        memcpy(_data.Get(), pixelData, _size._x * _size._y * PixelByteSize());
    else
        ErrorString("Can not set pixel data of a compressed image");
}

unsigned char* AImage::DecodePixelData(FStream& source, int& width, int& height, unsigned& components)
{
    size_t dataSize = source.Size();

    TAutoArrayPtr<unsigned char> buffer(new unsigned char[dataSize]);
    source.Read(buffer.Get(), dataSize);
    return stbi_load_from_memory(buffer.Get(), (int)dataSize, &width, &height, (int *)&components, 0);
}

void AImage::FreePixelData(unsigned char* pixelData)
{
    if (!pixelData)
        return;

    stbi_image_free(pixelData);
}

bool AImage::GenerateMipImage(AImage& dest) const
{
    PROFILE(GenerateMipImage);

    int components = GetComponents();
    if (components < 1 || components > 4)
    {
        ErrorString("Unsupported format for calculating the next mip level");
        return false;
    }

    TVector2I sizeOut(Max(_size._x / 2, 1), Max(_size._y / 2, 1));
    dest.SetSize(sizeOut, _format);

    const unsigned char* pixelDataIn = _data.Get();
    unsigned char* pixelDataOut = dest._data.Get();

    switch (components)
    {
    case 1:
        for (int y = 0; y < sizeOut._y; ++y)
        {
            const unsigned char* inUpper = &pixelDataIn[(y * 2) * _size._x];
            const unsigned char* inLower = &pixelDataIn[(y * 2 + 1) * _size._x];
            unsigned char* out = &pixelDataOut[y * sizeOut._x];

            for (int x = 0; x < sizeOut._x; ++x)
                out[x] = ((unsigned)inUpper[x * 2] + inUpper[x * 2 + 1] + inLower[x * 2] + inLower[x * 2 + 1]) >> 2;
        }
        break;

    case 2:
        for (int y = 0; y < sizeOut._y; ++y)
        {
            const unsigned char* inUpper = &pixelDataIn[(y * 2) * _size._x * 2];
            const unsigned char* inLower = &pixelDataIn[(y * 2 + 1) * _size._x * 2];
            unsigned char* out = &pixelDataOut[y * sizeOut._x * 2];

            for (int x = 0; x < sizeOut._x * 2; x += 2)
            {
                out[x] = ((unsigned)inUpper[x * 2] + inUpper[x * 2 + 2] + inLower[x * 2] + inLower[x * 2 + 2]) >> 2;
                out[x + 1] = ((unsigned)inUpper[x * 2 + 1] + inUpper[x * 2 + 3] + inLower[x * 2 + 1] + inLower[x * 2 + 3]) >> 2;
            }
        }
        break;

    case 4:
        for (int y = 0; y < sizeOut._y; ++y)
        {
            const unsigned char* inUpper = &pixelDataIn[(y * 2) * _size._x * 4];
            const unsigned char* inLower = &pixelDataIn[(y * 2 + 1) * _size._x * 4];
            unsigned char* out = &pixelDataOut[y * sizeOut._x * 4];

            for (int x = 0; x < sizeOut._x * 4; x += 4)
            {
                out[x] = ((unsigned)inUpper[x * 2] + inUpper[x * 2 + 4] + inLower[x * 2] + inLower[x * 2 + 4]) >> 2;
                out[x + 1] = ((unsigned)inUpper[x * 2 + 1] + inUpper[x * 2 + 5] + inLower[x * 2 + 1] + inLower[x * 2 + 5]) >> 2;
                out[x + 2] = ((unsigned)inUpper[x * 2 + 2] + inUpper[x * 2 + 6] + inLower[x * 2 + 2] + inLower[x * 2 + 6]) >> 2;
                out[x + 3] = ((unsigned)inUpper[x * 2 + 3] + inUpper[x * 2 + 7] + inLower[x * 2 + 3] + inLower[x * 2 + 7]) >> 2;
            }
        }
        break;
    }

    return true;
}

FImageLevel AImage::GetLevel(size_t index) const
{
    FImageLevel level;

    if (index >= _numLevels)
        return level;

    size_t i = 0;
    size_t offset = 0;

    for (;;)
    {
        level._size = TVector2I(Max(_size._x >> i, 1), Max(_size._y >> i, 1));
        level._data = _data.Get() + offset;

        size_t dataSize = CalculateDataSize(level._size, _format, &level._rows, &level._rowSize);
        if (i == index)
            return level;

        offset += dataSize;
        ++i;
    }
}


SDL_Surface* AImage::GetSDLSurface(const TRectI& rect) const
{
	if (!_data)
		return nullptr;

	if (IsCompressed())
	{
		ErrorString("Can not get SDL surface from compressed image " + GetName());
		return nullptr;
	}

	if (GetComponents() < 3)
	{
		ErrorString("Can not get SDL surface from image " + GetName() + " with less than 3 components");
		return nullptr;
	}

	TRectI imageRect = rect;
	// Use full image if illegal rect
	if (imageRect.Left() < 0 || imageRect.Top() < 0 || imageRect.Right() > _size._x || imageRect.Bottom() > _size._y ||
		imageRect.Left() >= imageRect.Right() || imageRect.Top() >= imageRect.Bottom())
	{
		imageRect.Left() = 0;
		imageRect.Top() = 0;
		imageRect.Right() = _size._x;
		imageRect.Bottom() = _size._y;
	}

	int imageWidth = _size._x;
	int width = imageRect.Width();
	int height = imageRect.Height();

	// Assume little-endian for all the supported platforms
	unsigned rMask = 0x000000ff;
	unsigned gMask = 0x0000ff00;
	unsigned bMask = 0x00ff0000;
	unsigned aMask = 0xff000000;

	SDL_Surface* surface = SDL_CreateRGBSurface(0, width, height, GetComponents() * 8, rMask, gMask, bMask, aMask);
	if (surface)
	{
		SDL_LockSurface(surface);

		auto* destination = reinterpret_cast<unsigned char*>(surface->pixels);
		unsigned char* source = _data.Get() + GetComponents() * (imageWidth * imageRect.Top() + imageRect.Left());
		for (int i = 0; i < height; ++i)
		{
			memcpy(destination, source, (size_t)GetComponents() * width);
			destination += surface->pitch;
			source += GetComponents() * imageWidth;
		}

		SDL_UnlockSurface(surface);
	}
	else
		ErrorString("Failed to create SDL surface from image " + GetName());

	return surface;
}

#ifndef AUTO_OPENGL_ES
bool AImage::DecompressLevel(unsigned char* dest, size_t index) const
{
    PROFILE(DecompressImageLevel);

    if (!dest)
    {
        ErrorString("Null destination data for DecompressLevel");
        return false;
    }

    if (index >= _numLevels)
    {
        ErrorString("Mip level index out of bounds for DecompressLevel");
        return false;
    }

    FImageLevel level = GetLevel(index);

    switch (_format)
    {
    case EImageFormat::DXT1:
    case EImageFormat::DXT3:
    case EImageFormat::DXT5:
        DecompressImageDXT(dest, level._data, level._size._x, level._size._y, _format);
        break;

    case EImageFormat::ETC1:
        DecompressImageETC(dest, level._data, level._size._x, level._size._y);
        break;

    case EImageFormat::PVRTC_RGB_2BPP:
    case EImageFormat::PVRTC_RGBA_2BPP:
    case EImageFormat::PVRTC_RGB_4BPP:
    case EImageFormat::PVRTC_RGBA_4BPP:
        DecompressImagePVRTC(dest, level._data, level._size._x, level._size._y, _format);
        break;

    default:
        ErrorString("Unsupported format for DecompressLevel");
        return false;
    }

    return true;
}
#endif

size_t AImage::CalculateDataSize(const TVector2I& _size, EImageFormat::Type _format, size_t* dstRows, size_t* dstRowSize)
{
    size_t rows, rowSize, dataSize;
#ifndef AUTO_OPENGL_ES
    if (_format < EImageFormat::DXT1)
#else
	if (_format < EImageFormat::ETC1)
#endif
    {
        rows = _size._y;
        rowSize = _size._x * pixelByteSizes[_format];
        dataSize = rows * rowSize;
    }
#ifndef AUTO_OPENGL_ES
    else if (_format < EImageFormat::PVRTC_RGB_2BPP)
    {
        size_t blockSize = (_format == EImageFormat::DXT1 || _format == EImageFormat::ETC1) ? 8 : 16;
        rows = (_size._y + 3) / 4;
        rowSize = ((_size._x + 3) / 4) * blockSize;
        dataSize = rows * rowSize;
    }
#endif
    else
    {
        size_t blockSize = _format < EImageFormat::PVRTC_RGB_4BPP ? 2 : 4;
        size_t dataWidth = Max(_size._x, blockSize == 2 ? 16 : 8);
        rows = Max(_size._y, 8);
        dataSize = (dataWidth * rows * blockSize + 7) >> 3;
        rowSize = dataSize / rows;
    }

    if (dstRows)
        *dstRows = rows;
    if (dstRowSize)
        *dstRowSize = rowSize;
    return dataSize;
}

}