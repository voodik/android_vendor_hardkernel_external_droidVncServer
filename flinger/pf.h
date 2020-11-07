#ifndef UI_PF_H
#define UI_PF_H

#include <stdint.h>
#include <sys/types.h>
#include <utils/Errors.h>

#include <ui/PixelFormat.h>

namespace android {

struct PixelFormatInfo {
    enum {
        INDEX_ALPHA   = 0,
        INDEX_RED     = 1,
        INDEX_GREEN   = 2,
        INDEX_BLUE    = 3
    };
    enum { // components
        ALPHA   = 1,
        RGB     = 2,
        RGBA    = 3,
        L       = 4,
        LA      = 5,
        OTHER   = 0xFF
    };
    struct szinfo {
        uint8_t h;
        uint8_t l;
    };
    inline PixelFormatInfo() : version(sizeof(PixelFormatInfo)) { }
    size_t getScanlineSize(unsigned int width) const;
    size_t getSize(size_t ci) const {
        return (ci <= 3) ? (cinfo[ci].h - cinfo[ci].l) : 0;
    }
    size_t      version;
    PixelFormat format;
    size_t      bytesPerPixel;
    size_t      bitsPerPixel;
    union {
        szinfo      cinfo[4];
        struct {
            uint8_t     h_alpha;
            uint8_t     l_alpha;
            uint8_t     h_red;
            uint8_t     l_red;
            uint8_t     h_green;
            uint8_t     l_green;
            uint8_t     h_blue;
            uint8_t     l_blue;
        };
    };
    uint8_t     components;
    uint8_t     reserved0[3];
    uint32_t    reserved1;
};


status_t    getPixelFormatInfo(PixelFormat format, PixelFormatInfo* info);

}; // namespace android

#endif // UI_PF_H
