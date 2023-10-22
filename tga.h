#ifndef _plugin_tga_tga_h_
#define _plugin_tga_tga_h_

#include <Draw/Draw.h>

namespace Upp {

INITIALIZE(TGARaster);

enum TgaImageType {
	TGA_NO_IMAGE_DATA            = 0,
	TGA_UNCOMPRESSED_COLOR_MAP   = 1,
	TGA_UNCOMPRESSED_TRUE_COLOR  = 2,
	TGA_UNCOMPRESSED_BLACK_WHITE = 3,
	TGA_RLE_COLOR_MAP            = 9,
	TGA_RLE_TRUE_COLOR           = 10,
	TGA_RLE_BLACK_WHITE          = 11
};

const uint8 TGA_REPEAT_COUNT_BIT  = 0b10000000;
const uint8 TGA_REPEAT_COUNT_MASK = 0b01111111;

enum Orientation {
	TGA_BOTTOM_LEFT,
	TGA_BOTTOM_RIGHT,
	TGA_TOP_LEFT,
	TGA_TOP_RIGHT
};

class TGARaster : public StreamRaster {
	Size             size     {};
	int              rowBytes {};
	Buffer<RGBA>     palette  {};
	Buffer<byte>     scanline {};
	Buffer<byte>     rleScan  {};
	bool             topdown  {};
	int64            soff     {};
	RasterFormat     fmt      {};
	Raster::Info     info     {};
	One<ImageRaster> rle      {};
	bool             file     {};
	
public:
	bool        Create()                              override;
	Size        GetSize()                             override;
	Info        GetInfo()                             override;
	Line        GetLine(int line)                     override;
	const RGBA* GetPalette()                          override;
	const       RasterFormat *GetFormat()             override;
	Value       GetMetaData(String id)                override;
	void        EnumMetaData(Vector<String>& id_list) override;
	String      ToString();
	
	TGARaster(bool file = true) : file(file) {}
};

bool IsTGA(StreamRaster *s);

class TGAEncoder : public StreamRasterEncoder {
	int          row_bytes {};
	int          linei     {};
	int64        soff      {};
	Buffer<byte> scanline  {};
	int          linebytes {};
	int          bpp       {};
	bool         grayscale {};
	bool         useRLE    {};
	bool         hasAlpha  {};

public:
	TGAEncoder& Bpp(int _bpp)        { bpp       = _bpp; return *this; }
	TGAEncoder& Mono(bool gs = true) { grayscale = gs;   return *this; }
	TGAEncoder& RLE(bool rle = true) { useRLE    = rle;  return *this; }

	TGAEncoder(int bpp = 32, bool gs = false, bool useRLE = false) : bpp(bpp), grayscale(gs), useRLE(useRLE) {}
	
	int  GetPaletteCount()           override;
	void Start(Size sz)              override;
	void WriteLineRaw(const byte *s) override;
};

}

#endif
