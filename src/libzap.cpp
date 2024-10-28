#include "libzap.h"
#include <cstdio>
#include <M4Image/M4Image.h>
#include <cstring>
#include <cstdlib>

enum class INTERNAL_ZAP_IMAGE_FORMAT : unsigned int
{
    JPG = 10,
    JTIF = 11,
    PNG = 75,
};

struct ZAPFILE_HEADER
{
    unsigned int header_size;
    unsigned int file_version;
    INTERNAL_ZAP_IMAGE_FORMAT image1_format;
    INTERNAL_ZAP_IMAGE_FORMAT image2_format;
    unsigned int image1_size;
    unsigned int image2_size;
    unsigned int width;
    unsigned int height;
};

zap_error_t internal_zap_load(const char* filename, zap_uint_t colorFormat, zap_byte_t** pRefImage, zap_size_t* pOutSize, zap_int_t* pOutWidth, zap_int_t* pOutHeight, zap_size_t* pRefStride, bool resize)
{
    FILE* pFile = fopen(filename, "rb");
    if (pFile)
    {
        fseek(pFile, 0, SEEK_END);
        size_t size = ftell(pFile);
        fseek(pFile, 0, SEEK_SET);

        unsigned char* pData = 0;

        try {
            pData = (unsigned char*)M4Image::allocator.mallocSafe(size);
        } catch (std::bad_alloc) {
            return ZAP_ERROR_OUT_OF_MEMORY;
        }

        fread(pData, 1, size, pFile);
        fclose(pFile);

        zap_error_t result = resize
            ? zap_resize_memory(pData, colorFormat, pRefImage, pOutSize, *pOutWidth, *pOutHeight, pRefStride)
            : zap_load_memory(pData, colorFormat, pRefImage, pOutSize, pOutWidth, pOutHeight, pRefStride);

        M4Image::allocator.freeSafe(pData);

        return result;
    }
    else
    {
        *pOutSize = 0;

        return ZAP_ERROR_FILE_FAILED_TO_OPEN;
    }
}

zap_error_t zap_load(const char* filename, zap_uint_t colorFormat, zap_byte_t** pRefImage, zap_size_t* pOutSize, zap_int_t* pOutWidth, zap_int_t* pOutHeight, zap_size_t* pRefStride)
{
    return internal_zap_load(filename, colorFormat, pRefImage, pOutSize, pOutWidth, pOutHeight, pRefStride, false);
}

zap_error_t zap_resize(const char* filename, zap_uint_t colorFormat, zap_byte_t** pRefImage, zap_size_t* pOutSize, zap_int_t width, zap_int_t height, zap_size_t* pRefStride)
{
    return internal_zap_load(filename, colorFormat, pRefImage, pOutSize, &width, &height, pRefStride, true);
}

zap_error_t internal_zap_get_extension(INTERNAL_ZAP_IMAGE_FORMAT imageFormat, const char* &pExtension) {
    switch (imageFormat) {
        case INTERNAL_ZAP_IMAGE_FORMAT::PNG:
        pExtension = ".png";
        return ZAP_ERROR_NONE;
        case INTERNAL_ZAP_IMAGE_FORMAT::JPG:
        pExtension = ".jpg";
        return ZAP_ERROR_NONE;
        case INTERNAL_ZAP_IMAGE_FORMAT::JTIF:
        pExtension = ".jtif";
        return ZAP_ERROR_NONE;
    }
    return ZAP_ERROR_INVALID_FORMAT;
}

zap_error_t internal_zap_validate_header(ZAPFILE_HEADER* pHeader, const char* &pExtension1, const char* &pExtension2) {
    if (pHeader->header_size != sizeof(ZAPFILE_HEADER))
        return ZAP_ERROR_INVALID_FILE;

    if (pHeader->file_version != 2)
        return ZAP_ERROR_INVALID_VERSION;

    zap_error_t err = internal_zap_get_extension(pHeader->image1_format, pExtension1);

    if (err != ZAP_ERROR_NONE)
        return err;
    
    return internal_zap_get_extension(pHeader->image2_format, pExtension2);
}

zap_error_t internal_zap_acquire_image(const unsigned char* pData, size_t size, const char* pExtension, int width, int height, size_t &stride, M4Image::COLOR_FORMAT colorFormat, zap_byte_t** pRefImage) {
    try {
        M4Image m4Image(width, height, stride, colorFormat, *pRefImage);
        m4Image.load(pData, size, pExtension);
        *pRefImage = m4Image.acquire();
    } catch (M4Image::Invalid) {
        return ZAP_ERROR_INVALID_FILE;
    } catch (std::runtime_error) {
        return ZAP_ERROR_INVALID_FORMAT;
    } catch (std::invalid_argument) {
        return ZAP_ERROR_INVALID_ARGUMENT;
    } catch (std::bad_alloc) {
        return ZAP_ERROR_OUT_OF_MEMORY;
    } catch (...) {
        return ZAP_ERROR_INTERNAL_ERROR;
    }
    return ZAP_ERROR_NONE;
}

zap_error_t internal_zap_load_memory(const unsigned char* pData, zap_uint_t colorFormat, zap_byte_t** pRefImage, zap_size_t* pOutSize, zap_int_t* pRefWidth, zap_int_t* pRefHeight, zap_size_t* pRefStride, bool resize)
{
    if (colorFormat < ZAP_COLOR_FORMAT_RGBA32 || colorFormat > ZAP_COLOR_FORMAT_BGRX32)
        return ZAP_ERROR_INVALID_ARGUMENT;

    auto* pHeader = (ZAPFILE_HEADER*)pData;
    const char* pExtension1;
    const char* pExtension2;

    zap_error_t err = internal_zap_validate_header(pHeader, pExtension1, pExtension2);

    if (err != ZAP_ERROR_NONE)
        return err;

    int image1_offset = sizeof(ZAPFILE_HEADER);
    int image1_size = (int)pHeader->image1_size;

    zap_int_t &width = *pRefWidth;
    zap_int_t &height = *pRefHeight;

    if (!resize)
    {
        width = (zap_int_t)pHeader->width;
        height = (zap_int_t)pHeader->height;
    }

    zap_size_t &image1_stride = *pRefStride;

    err = internal_zap_acquire_image(pData + image1_offset, image1_size, pExtension1, width, height, image1_stride, (M4Image::COLOR_FORMAT)colorFormat, pRefImage);

    if (err != ZAP_ERROR_NONE)
        return err;

    unsigned char* pixelRGB = *pRefImage;

    // if colour format has alpha
    if (!(colorFormat & 1))
    {
        int image2_offset = image1_offset + image1_size;
        int image2_size = (int)pHeader->image2_size;
        size_t image2_stride = 0;

        unsigned char* pixelsAlpha = 0;

        err = internal_zap_acquire_image(pData + image2_offset, image2_size, pExtension2, width, height, image2_stride, M4Image::COLOR_FORMAT::L8, &pixelsAlpha);

        if (err != ZAP_ERROR_NONE)
        {
            M4Image::allocator.freeSafe(pixelRGB);
            return err;
        }

        if (height)
        {
            unsigned char* pixelAlpha = pixelsAlpha;

            zap_int_t bytesPerRow = 4 * width;
            zap_int_t y = height;
            do
            {
                for (zap_int_t i = 3; i < bytesPerRow; i += 4)
                    pixelRGB[i] = pixelAlpha[i >> 2];
                pixelRGB += image1_stride;
                pixelAlpha += image2_stride;
                --y;
            } while (y);
        }

        M4Image::allocator.freeSafe(pixelsAlpha);
    }

    *pOutSize = (size_t)width * (size_t)height * 4;

    return ZAP_ERROR_NONE;
}

zap_error_t zap_load_memory(const unsigned char* pData, zap_uint_t colorFormat, zap_byte_t** pRefImage, zap_size_t* pOutSize, zap_int_t* pOutWidth, zap_int_t* pOutHeight, zap_size_t* pRefStride)
{
    return internal_zap_load_memory(pData, colorFormat, pRefImage, pOutSize, pOutWidth, pOutHeight, pRefStride, false);
}

zap_error_t zap_resize_memory(const unsigned char* pData, zap_uint_t colorFormat, zap_byte_t** pRefImage, zap_size_t* pOutSize, zap_int_t width, zap_int_t height, zap_size_t* pRefStride)
{
    return internal_zap_load_memory(pData, colorFormat, pRefImage, pOutSize, &width, &height, pRefStride, true);
}

zap_error_t zap_free(zap_byte_t* pImage) {
    M4Image::allocator.freeSafe(pImage);

    return ZAP_ERROR_NONE;
}

zap_error_t zap_get_info(const unsigned char* pData, zap_int_t* pOutWidth, zap_int_t* pOutHeight) {
    auto* pHeader = (ZAPFILE_HEADER*)pData;
    const char* pExtension1;
    const char* pExtension2;

    zap_error_t err = internal_zap_validate_header(pHeader, pExtension1, pExtension2);

    if (pOutWidth)
        *pOutWidth = (zap_int_t)pHeader->width;

    if (pOutHeight)
        *pOutHeight = (zap_int_t)pHeader->height;

    return ZAP_ERROR_NONE;
}

zap_error_t zap_set_allocator(zap_malloc_proc mallocProc, zap_free_proc freeProc, zap_realloc_proc reallocProc)
{
    M4Image::allocator = M4Image::Allocator(mallocProc, freeProc, reallocProc);
    return ZAP_ERROR_NONE;
}

zap_error_t zap_save(const char* filename, const zap_byte_t* pData, zap_size_t dataSize, zap_int_t width, zap_int_t height, zap_size_t stride, zap_uint_t colorFormat, zap_uint_t format)
{
    return ZAP_ERROR_NOT_IMPLEMENTED;
}

zap_error_t zap_save_memory(zap_byte_t** pOut, zap_size_t* pOutSize, zap_int_t width, zap_int_t height, zap_size_t stride, zap_uint_t colorFormat, zap_uint_t format)
{
    return ZAP_ERROR_NOT_IMPLEMENTED;
}
