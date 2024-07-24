#include "zap.h"
#include <cstdio>
#include <stb_image.h>
#include <cstring>

enum class ZAP_IMAGE_FORMAT : unsigned int
{
    JPG = 10,
    JTIF = 11,
    PNG = 75,
};

struct ZAPFILE_HEADER
{
    unsigned int header_size;
    unsigned int file_version;
    ZAP_IMAGE_FORMAT image1_format;
    ZAP_IMAGE_FORMAT image2_format;
    unsigned int image1_size;
    unsigned int image2_size;
    unsigned int width;
    unsigned int height;
};

bool load_zap_file(const char* filename, unsigned char** pOut, int* pOutSize)
{
    FILE* pFile = fopen(filename, "rb");
    if (pFile)
    {
        fseek(pFile, 0, SEEK_END);
        int size = ftell(pFile);
        fseek(pFile, 0, SEEK_SET);
        auto* pData = new unsigned char[size];
        fread(pData, 1, size, pFile);
        fclose(pFile);

        bool result = load_zap(pData, pOut, pOutSize);

        delete[] pData;

        return result;
    }
    else
    {
        *pOutSize = 0;

        return false;
    }
}

bool load_zap(const unsigned char* pData, unsigned char** pOut, int* pOutSize)
{
    auto* pHeader = (ZAPFILE_HEADER*)pData;

    if (pHeader->header_size != sizeof(ZAPFILE_HEADER))
        return false;

    if (pHeader->file_version != 2)
        return false;

    if (pHeader->image1_format != ZAP_IMAGE_FORMAT::PNG &&
        pHeader->image1_format != ZAP_IMAGE_FORMAT::JPG &&
        pHeader->image1_format != ZAP_IMAGE_FORMAT::JTIF)
        return false;

    int width = pHeader->width;
    int height = pHeader->height;

    int image1_size = pHeader->image1_size;
    int image2_size = pHeader->image2_size;

    int image1_offset = sizeof(ZAPFILE_HEADER);
    int image2_offset = image1_offset + image1_size;

    int image1_width, image1_height, image1_channels;
    int image2_width, image2_height, image2_channels;

    unsigned char* pImage1 = stbi_load_from_memory(pData + image1_offset, image1_size, &image1_width, &image1_height, &image1_channels, 0);
    unsigned char* pImage2 = stbi_load_from_memory(pData + image2_offset, image2_size, &image2_width, &image2_height, &image2_channels, 0);

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
            pixelsRGB += width;
            --y;
        } while (y);
    }

    *pOutSize = width * height * 4;
    *pOut = new unsigned char[*pOutSize];

    memcpy(*pOut, pixelsRGB, *pOutSize);

    return true;
}