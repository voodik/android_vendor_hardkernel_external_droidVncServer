#include "pf.h"
#include <ui/PixelFormat.h>


namespace android {

static const int COMPONENT_YUV = 0xFF;
struct Info {
    size_t      size;
    size_t      bitsPerPixel;
    struct {
        uint8_t     ah;
        uint8_t     al;
        uint8_t     rh;
        uint8_t     rl;
        uint8_t     gh;
        uint8_t     gl;
        uint8_t     bh;
        uint8_t     bl;
    };
    uint8_t     components;
};
static Info const sPixelFormatInfos[] = {
        { 0,  0, { 0, 0,   0, 0,   0, 0,   0, 0 }, 0 },
        { 4, 32, {32,24,   8, 0,  16, 8,  24,16 }, PixelFormatInfo::RGBA },
        { 4, 24, { 0, 0,   8, 0,  16, 8,  24,16 }, PixelFormatInfo::RGB  },
        { 3, 24, { 0, 0,   8, 0,  16, 8,  24,16 }, PixelFormatInfo::RGB  },
        { 2, 16, { 0, 0,  16,11,  11, 5,   5, 0 }, PixelFormatInfo::RGB  },
        { 4, 32, {32,24,  24,16,  16, 8,   8, 0 }, PixelFormatInfo::RGBA },
        { 2, 16, { 1, 0,  16,11,  11, 6,   6, 1 }, PixelFormatInfo::RGBA },
        { 2, 16, { 4, 0,  16,12,  12, 8,   8, 4 }, PixelFormatInfo::RGBA },
        { 1,  8, { 8, 0,   0, 0,   0, 0,   0, 0 }, PixelFormatInfo::ALPHA},
        { 1,  8, { 0, 0,   8, 0,   8, 0,   8, 0 }, PixelFormatInfo::L    },
        { 2, 16, {16, 8,   8, 0,   8, 0,   8, 0 }, PixelFormatInfo::LA   },
        { 1,  8, { 0, 0,   8, 5,   5, 2,   2, 0 }, PixelFormatInfo::RGB  },
};
static const Info* gGetPixelFormatTable(size_t* numEntries) {
    if (numEntries) {
        *numEntries = sizeof(sPixelFormatInfos)/sizeof(Info);
    }
    return sPixelFormatInfos;
}

status_t getPixelFormatInfo(PixelFormat format, PixelFormatInfo* info)
{
    if (format <= 0)
        return BAD_VALUE;
    if (info->version != sizeof(PixelFormatInfo))
        return INVALID_OPERATION;
    // YUV format from the HAL are handled here
    switch (format) {
    case HAL_PIXEL_FORMAT_YCbCr_422_SP:
    case HAL_PIXEL_FORMAT_YCbCr_422_I:
        info->bitsPerPixel = 16;
        goto done;
    case HAL_PIXEL_FORMAT_YCrCb_420_SP:
    case HAL_PIXEL_FORMAT_YV12:
        info->bitsPerPixel = 12;
     done:
        info->format = format;
        info->components = COMPONENT_YUV;
        info->bytesPerPixel = 1;
        info->h_alpha = 0;
        info->l_alpha = 0;
        info->h_red = info->h_green = info->h_blue = 8;
        info->l_red = info->l_green = info->l_blue = 0;
        return NO_ERROR;
    }
    size_t numEntries;
    const Info *i = gGetPixelFormatTable(&numEntries) + format;
    bool valid = uint32_t(format) < numEntries;
    if (!valid) {
        return BAD_INDEX;
    }
    info->format = format;
    info->bytesPerPixel = i->size;
    info->bitsPerPixel  = i->bitsPerPixel;
    info->h_alpha       = i->ah;
    info->l_alpha       = i->al;
    info->h_red         = i->rh;
    info->l_red         = i->rl;
    info->h_green       = i->gh;
    info->l_green       = i->gl;
    info->h_blue        = i->bh;
    info->l_blue        = i->bl;
    info->components    = i->components;
    return NO_ERROR;
}

}; // namespace android
