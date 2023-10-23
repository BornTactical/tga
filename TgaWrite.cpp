#include "tga.h"

namespace Upp {
#include "tgahdr.h"

void TGAEncoder::Start(Size sz) {
    LOG(sz);
    auto& stream = GetStream();
    
    switch(bpp) {
        case 32:
            if(hasAlpha) {
                format.Set32le(0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000);
            }
            else {
                format.Set32le(0x00ff0000, 0x0000ff00, 0x000000ff, 0);
            }
            break;
        case 24:
            format.Set24le(0xff0000, 0x00ff00, 0x0000ff);
            break;
        case 16:
            if(hasAlpha)
                format.Set8A();
            else
                format.Set16le(0b0111110000000000, 0b0000001111100000, 0b0000000000011111);
            break;
        case 8:
            format.Set8();
            break;
    }
    
    auto paletteCount = GetPaletteCount();
    
    auto InitImageType = [&]() -> byte {
        if(paletteCount) {
            if(useRLE) return TGA_RLE_COLOR_MAP;
            return TGA_UNCOMPRESSED_COLOR_MAP;
        }
        
        if(grayscale) {
            if(useRLE) return TGA_RLE_BLACK_WHITE;
            return TGA_UNCOMPRESSED_BLACK_WHITE;
        }
        
        if(useRLE) return TGA_RLE_TRUE_COLOR;
        
        return TGA_UNCOMPRESSED_TRUE_COLOR;
    };
    
    auto InitColorMapSpec = [&]() -> ColorMapSpec {
        if(paletteCount) {
            return {
                .firstEntry = word(sizeof(TGAHeader) + imageID.GetLength()),
                .length     = word(bpp * paletteCount),
                .entrySize  = byte(32)
            };
        }
        
        return ColorMapSpec {};
    };
    
    const TGAHeader header {
        .idLength     = byte(imageID.GetLength()),
        .colorMap     = byte(paletteCount),
        .imageType    = InitImageType(),
        .colorMapSpec = InitColorMapSpec(),
        .imageSpec = {
            .xOrigin    = 0,
            .yOrigin    = 0,
            .width      = int16(sz.cx),
            .height     = int16(sz.cy),
            .depth      = uint8(bpp),
            .descriptor = (TGA_TOP_LEFT << 4)
        }
    };
    
    row_bytes = format.GetByteCount(sz.cx);
    scanline.Alloc(row_bytes);
    
    stream.Put(&header, sizeof(header));
    
    if(!imageID.IsEmpty())
        stream.Put(imageID.Begin(), header.idLength);
    
    if(paletteCount) {
        stream.Put(GetPalette(), sizeof(RGBA) * paletteCount);
    }
    
    soff      = stream.GetPos();
    linei     = sz.cy;
    linebytes = format.GetByteCount(sz.cx);
}

struct RGB {
    uint8 b;
    uint8 g;
    uint8 r;
    
    bool operator!=(const RGB& second) const {
        return r != second.r || g != second.g || b != second.b;
    }
    
    bool operator==(const RGB& second) const {
        return r == second.r && g == second.g && b == second.b;
    }
};

template <typename T>
void WriteRLE(Stream& stream, const T* read, uint32 bytesRemaining) {
    auto remaining = bytesRemaining / sizeof(T);
    
    while(remaining > 0) {
        int run = 0;
        
        while(read[run] != read[0] && remaining) {
            run++;
            remaining--;
        }
        
        while(run >= 127) {
            stream.Put(127, sizeof(byte));
            stream.Put(read, sizeof(T) * 127);
            
            read += 127;
            run  -= 127;
        }
        
        if(run > 0) {
            stream.Put(run, sizeof(byte));
            stream.Put(read, sizeof(T) * run);
            read += run;
            run   = 0;
        }
        
        while(read[run] == read[0] && remaining) {
            run++;
            remaining--;
        }
        
        while(run >= 128) {
            stream.Put(255, sizeof(byte));
            stream.Put(read, sizeof(T));
            read += 128;
            run  -= 128;
        }
        
        if(run > 0) {
            stream.Put(run + 127, sizeof(byte));
            stream.Put(read, sizeof(T));
            read += run;
            run   = 0;
        }
    }
}

void TGAEncoder::WriteLineRaw(const byte *s) {
    auto& stream = GetStream();
    stream.Seek(soff);
    
    if(useRLE) {
        switch(bpp) {
            case 32:
                WriteRLE(stream, (RGBA*)s, row_bytes);
                break;
            case 24:
                WriteRLE(stream, (struct RGB*)s, row_bytes);
                break;
            case 16:
                WriteRLE(stream, (uint16*)s, row_bytes);
                break;
            case 8:
                WriteRLE(stream, (uint8*)s, row_bytes);
                break;
            default:
                return;
        }
        
        soff = stream.GetPos();
        return;
    }
    
    memcpy(scanline, s, linebytes);
    stream.Put(scanline, row_bytes);
    soff = stream.GetPos();
}
}

