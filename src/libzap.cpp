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

static zap_malloc_proc mallocProc = malloc;
static zap_free_proc freeProc = free;

zap_error_t internal_zap_load(const char* filename, zap_uint_t colorFormat, zap_byte_t** pOut, zap_size_t* pOutSize, zap_int_t* pOutWidth, zap_int_t* pOutHeight, bool resize)
{
    FILE* pFile = fopen(filename, "rb");
    if (pFile)
    {
        fseek(pFile, 0, SEEK_END);
        size_t size = ftell(pFile);
        fseek(pFile, 0, SEEK_SET);
        auto* pData = (unsigned char*)mallocProc(size);

        if (!pData) {
            return ZAP_ERROR_OUT_OF_MEMORY;
        }

        fread(pData, 1, size, pFile);
        fclose(pFile);

        zap_error_t result = resize
            ? zap_resize_memory(pData, colorFormat, pOut, pOutSize, *pOutWidth, *pOutHeight)
            : zap_load_memory(pData, colorFormat, pOut, pOutSize, pOutWidth, pOutHeight);

        freeProc(pData);

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

    const char* extension = "jpg";

    if (pHeader->image1_format == INTERNAL_ZAP_IMAGE_FORMAT::PNG) {
        extension = "png";
    }

    int image1_size = (int)pHeader->image1_size;
    int image2_size = (int)pHeader->image2_size;

    int image1_offset = sizeof(ZAPFILE_HEADER);
    int image2_offset = image1_offset + image1_size;

    zap_int_t &image1_width = *pOutWidth;
    zap_int_t &image1_height = *pOutHeight;

    size_t image1_stride, image2_stride;

    *pOut = resize
    ? M4Image::load(extension, pData + image1_offset, image1_size, image1_width, image1_height, image1_stride, (M4Image::COLOR_FORMAT)colorFormat)
    : M4Image::resize(extension, pData + image1_offset, image1_size, image1_width, image1_height, image1_stride, (M4Image::COLOR_FORMAT)colorFormat);

    int width, height;

    if (resize) {
        width = image1_width;
        height = image1_height;
    } else {
        width = (int)pHeader->width;
        height = (int)pHeader->height;

        if (width != image1_width || height != image1_height) {
            return ZAP_ERROR_INVALID_FILE;
        }
    }

    zap_int_t image2_width = width;
    zap_int_t image2_height = height;

    unsigned char* pixelsRGB = *pOut;

    unsigned char* pixelsAlpha = resize
    ? M4Image::load(extension, pData + image2_offset, image2_size, image2_width, image2_height, image2_stride, M4Image::COLOR_FORMAT::L8)
    : M4Image::resize(extension, pData + image2_offset, image2_size, image2_width, image2_height, image2_stride, M4Image::COLOR_FORMAT::L8);

    if (image1_width != image2_width || image1_height != image2_height) {
        return ZAP_ERROR_INVALID_FILE;
    }

    if (height)
    {
        int bitsPerRow = 4 * width;
        int y = height;
        do
        {
            for (int i = 3; i < bitsPerRow; i += 4)
                pixelsRGB[i] = pixelsAlpha[i >> 2];
            pixelsAlpha += width;
            pixelsRGB += bitsPerRow;
            --y;
        } while (y);
    }

    *pOutSize = (size_t)width * (size_t)height * 4;

    freeProc(pixelsAlpha);

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

zap_error_t zap_set_allocator(zap_malloc_proc _mallocProc, zap_free_proc _freeProc)
{
    mallocProc = _mallocProc;
    freeProc = _freeProc;
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
