#pragma once

extern bool load_zap_file(const char* filename, unsigned char** pOut, size_t* pOutSize, int* pOutWidth, int* pOutHeight);
extern bool load_zap(const unsigned char* pData, unsigned char** pOut, size_t* pOutSize, int* pOutWidth, int* pOutHeight);
