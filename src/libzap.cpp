#include "libzap.h"
#include <cstdio>
#include <stb_image.h>
#include <cstring>

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

zap_error_t zap_load(const char* filename, zap_uint_t colorFormat, zap_byte_t** pOut, zap_size_t* pOutSize, zap_int_t* pOutWidth, zap_int_t* pOutHeight)
{
    FILE* pFile = fopen(filename, "rb");
    if (pFile)
    {
        fseek(pFile, 0, SEEK_END);
        size_t size = ftell(pFile);
        fseek(pFile, 0, SEEK_SET);
        auto* pData = new unsigned char[size];
        fread(pData, 1, size, pFile);
        fclose(pFile);

        zap_error_t result = zap_load_memory(pData, colorFormat, pOut, pOutSize, pOutWidth, pOutHeight);

        delete[] pData;

        return result;
    }
    else
    {
        *pOutSize = 0;

        return ZAP_ERROR_FILE_FAILED_TO_OPEN;
    }
}

zap_error_t zap_load_memory(const unsigned char* pData, zap_uint_t colorFormat, zap_byte_t** pOut, zap_size_t* pOutSize, zap_int_t* pOutWidth, zap_int_t* pOutHeight)
{
    if (colorFormat != ZAP_COLOR_FORMAT_RGBA32)
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

    int width = (int)pHeader->width;
    int height = (int)pHeader->height;

    int image1_size = (int)pHeader->image1_size;
    int image2_size = (int)pHeader->image2_size;

    int image1_offset = sizeof(ZAPFILE_HEADER);
    int image2_offset = image1_offset + image1_size;

    int image1_width, image1_height, image1_channels;
    int image2_width, image2_height, image2_channels;

    unsigned char* pImage1 = stbi_load_from_memory(pData + image1_offset, image1_size, &image1_width, &image1_height, &image1_channels, 4);
    unsigned char* pImage2 = stbi_load_from_memory(pData + image2_offset, image2_size, &image2_width, &image2_height, &image2_channels, 1);

    unsigned char* pixelsAlpha = pImage2;
    unsigned char* pixelsRGB = pImage1;

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

    *pOutSize = width * height * 4;
    *pOut = new zap_byte_t[*pOutSize];
    *pOutWidth = width;
    *pOutHeight = height;

    memcpy(*pOut, pImage1, *pOutSize);

    stbi_image_free(pImage1);
    stbi_image_free(pImage2);

    return ZAP_ERROR_NONE;
}

zap_error_t zap_free(const zap_byte_t* pData)
{
    delete[] pData;

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
