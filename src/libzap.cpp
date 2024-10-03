#define _CRT_SECURE_NO_WARNINGS
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

zap_error_t internal_zap_load(const char* filename, zap_uint_t colorFormat, zap_byte_t** pOut, zap_size_t* pOutSize, zap_int_t* pOutWidth, zap_int_t* pOutHeight, bool resize)
{
    FILE* pFile = fopen(filename, "rb");
    if (pFile)
    {
        fseek(pFile, 0, SEEK_END);
        size_t size = ftell(pFile);
        fseek(pFile, 0, SEEK_SET);
        auto* pData = (unsigned char*)M4Image::malloc(size);

        if (!pData) {
            return ZAP_ERROR_OUT_OF_MEMORY;
        }

        fread(pData, 1, size, pFile);
        fclose(pFile);

        zap_error_t result = resize
            ? zap_resize_memory(pData, colorFormat, pOut, pOutSize, *pOutWidth, *pOutHeight)
            : zap_load_memory(pData, colorFormat, pOut, pOutSize, pOutWidth, pOutHeight);

        M4Image::free(pData);

        return result;
    }
    else
    {
        *pOutSize = 0;

        return ZAP_ERROR_FILE_FAILED_TO_OPEN;
    }
}

zap_error_t zap_load(const char* filename, zap_uint_t colorFormat, zap_byte_t** pOut, zap_size_t* pOutSize, zap_int_t* pOutWidth, zap_int_t* pOutHeight)
{
    return internal_zap_load(filename, colorFormat, pOut, pOutSize, pOutWidth, pOutHeight, false);
}

zap_error_t zap_resize(const char* filename, zap_uint_t colorFormat, zap_byte_t** pOut, zap_size_t* pOutSize, zap_int_t width, zap_int_t height)
{
    return internal_zap_load(filename, colorFormat, pOut, pOutSize, &width, &height, true);
}

zap_error_t internal_zap_load_memory(const unsigned char* pData, zap_uint_t colorFormat, zap_byte_t** pOut, zap_size_t* pOutSize, zap_int_t* pOutWidth, zap_int_t* pOutHeight, bool resize)
{
    if (colorFormat < ZAP_COLOR_FORMAT_RGBA32 || colorFormat > ZAP_COLOR_FORMAT_BGRX32)
        return ZAP_ERROR_INVALID_ARGUMENT;

    auto* pHeader = (ZAPFILE_HEADER*)pData;

    if (pHeader->header_size != sizeof(ZAPFILE_HEADER))
        return ZAP_ERROR_INVALID_FILE;

    if (pHeader->file_version != 2)
        return ZAP_ERROR_INVALID_VERSION;

    if (pHeader->image1_format != INTERNAL_ZAP_IMAGE_FORMAT::PNG &&
        pHeader->image1_format != INTERNAL_ZAP_IMAGE_FORMAT::JPG &&
        pHeader->image1_format != INTERNAL_ZAP_IMAGE_FORMAT::JTIF)
        return ZAP_ERROR_INVALID_FORMAT;

    const char* extension;

    switch (pHeader->image1_format) {
        case INTERNAL_ZAP_IMAGE_FORMAT::PNG:
        extension = "png";
        break;
        case INTERNAL_ZAP_IMAGE_FORMAT::JPG:
        extension = "jpg";
        break;
        case INTERNAL_ZAP_IMAGE_FORMAT::JTIF:
        extension = "jtif";
    }

    int image1_offset = sizeof(ZAPFILE_HEADER);
    int image1_size = (int)pHeader->image1_size;

    zap_int_t &width = *pOutWidth;
    zap_int_t &height = *pOutHeight;

    if (!resize) {
        width = (zap_int_t)pHeader->width;
        height = (zap_int_t)pHeader->height;
    }

    size_t image1_stride;

    *pOut = M4Image::resize(extension, pData + image1_offset, image1_size, width, height, image1_stride, (M4Image::COLOR_FORMAT)colorFormat);

    unsigned char* pixelRGB = *pOut;

    if (!pixelRGB) {
        return ZAP_ERROR_OUT_OF_MEMORY;
    }

    if (colorFormat == ZAP_COLOR_FORMAT_RGBA32 ||
        colorFormat == ZAP_COLOR_FORMAT_BGRA32)
    {
        int image2_offset = image1_offset + image1_size;
        int image2_size = (int)pHeader->image2_size;
        size_t image2_stride;

        unsigned char* pixelsAlpha = M4Image::resize(extension, pData + image2_offset, image2_size, width, height, image2_stride, M4Image::COLOR_FORMAT::L8);

        if (!pixelsAlpha) {
            M4Image::free(pixelRGB);
            return ZAP_ERROR_OUT_OF_MEMORY;
        }

        if (height)
        {
            unsigned char* pixelAlpha = pixelsAlpha;

            zap_int_t stride = 4 * width;
            zap_int_t y = height;
            do
            {
                for (zap_int_t i = 3; i < stride; i += 4)
                    pixelRGB[i] = pixelAlpha[i >> 2];
                pixelAlpha += width;
                pixelRGB += stride;
                --y;
            } while (y);
        }

        M4Image::free(pixelsAlpha);
    }

    *pOutSize = (size_t)width * (size_t)height * 4;

    return ZAP_ERROR_NONE;
}

zap_error_t zap_load_memory(const unsigned char* pData, zap_uint_t colorFormat, zap_byte_t** pOut, zap_size_t* pOutSize, zap_int_t* pOutWidth, zap_int_t* pOutHeight)
{
    return internal_zap_load_memory(pData, colorFormat, pOut, pOutSize, pOutWidth, pOutHeight, false);
}

zap_error_t zap_resize_memory(const unsigned char* pData, zap_uint_t colorFormat, zap_byte_t** pOut, zap_size_t* pOutSize, zap_int_t width, zap_int_t height)
{
    return internal_zap_load_memory(pData, colorFormat, pOut, pOutSize, &width, &height, true);
}

zap_error_t zap_free(zap_byte_t* pData) {
    M4Image::free(pData);

    return ZAP_ERROR_NONE;
}

zap_error_t zap_set_allocator(zap_malloc_proc mallocProc, zap_free_proc freeProc)
{
    M4Image::setAllocator(mallocProc, freeProc);
    return ZAP_ERROR_NONE;
}

zap_error_t zap_save(const char* filename, const zap_byte_t* pData, zap_size_t dataSize, zap_int_t width, zap_int_t height, zap_uint_t colorFormat, zap_uint_t format)
{
    return ZAP_ERROR_NOT_IMPLEMENTED;
}

zap_error_t zap_save_memory(zap_byte_t** pOut, zap_size_t* pOutSize, zap_int_t width, zap_int_t height, zap_uint_t colorFormat, zap_uint_t format)
{
    return ZAP_ERROR_NOT_IMPLEMENTED;
}
