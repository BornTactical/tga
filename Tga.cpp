#include "tga.h"

namespace Upp {
	#include "tgahdr.h"
	
	bool IsTGA(StreamRaster *s) {
		return false;
	}
	
	static Size GetDotSize(Size pixel_size, int xpm, int ypm) {
		if(!xpm || !ypm) {
			return Size(0, 0);
		}
		
		static const double DOTS_PER_METER = 60000 / 2.54;
		return Size(fround(pixel_size.cx * DOTS_PER_METER / xpm), fround(pixel_size.cy * DOTS_PER_METER / ypm));
	}
	
	String TGAHeader::ToString() const {
		String out;
		
		out << Format("Id:                   %s\n", idLength == 0 ? "no" : "yes");
		out << Format("Colormap:             %s\n", colorMap == 0 ? "no" : "yes");

		switch(imageType) {
			case TGA_NO_IMAGE_DATA:
				out << "Image Type:           No image data\n";
			break;
			case TGA_UNCOMPRESSED_COLOR_MAP:
				out << "Image Type:           Uncompressed color map\n";
			break;
			case TGA_UNCOMPRESSED_TRUE_COLOR:
				out << "Image Type:           Uncompressed true color\n";
			break;
			case TGA_UNCOMPRESSED_BLACK_WHITE:
				out << "Image Type:           Uncompressed black and white\n";
			break;
			case TGA_RLE_COLOR_MAP:
				out << "Image Type:           RLE color map\n";
			break;
			case TGA_RLE_TRUE_COLOR:
				out << "Image Type:           RLE true color\n";
			break;
			case TGA_RLE_BLACK_WHITE:
				out << "Image Type:           RLE black and white\n";
			break;
			default:
				out << "Error:                unknown image type!\n";
			break;
		}
		
		out << Format("First entry index:    %d\n", colorMapSpec.firstEntry);
		out << Format("Color map length:     %d\n", colorMapSpec.length);
		out << Format("Color Map entry size: %d\n", colorMapSpec.entrySize);
		out << Format("Image id length:      %d\n", idLength);
		out << Format("X-origin:             %d\n", imageSpec.xOrigin);
		out << Format("Y-origin:             %d\n", imageSpec.yOrigin);
		out << Format("width:                %d\n", imageSpec.width);
		out << Format("height:               %d\n", imageSpec.height);
		out << Format("depth:                %d\n", imageSpec.depth);
		
		String t;
		
		switch(imageSpec.descriptor >> 4) {
			case TGA_BOTTOM_LEFT:
				t = "bottom Left";
				break;
			case TGA_BOTTOM_RIGHT:
				t = "bottom Right";
				break;
			case TGA_TOP_LEFT:
				t = "top left";
				break;
			case TGA_TOP_RIGHT:
				t = "top right";
				break;
		}
		
		out << Format("first pixel origin:   %s\n", t);
		out << Format("alpha channel size:   %d\n", imageSpec.descriptor & 0b00001111);

		return out;
	}
	
	bool LoadTGAHeader(Stream& stream, TGAHeader& header, bool file) {
		if(!stream.IsOpen()) {
			return false;
		}
		
		ASSERT(stream.IsLoading());

		stream.Seek(0);
		
		if(!stream.GetAll(&header, sizeof(header))) {
			return false;
		}
		
		if(header.idLength) {
			Vector<byte> id;
			id.SetCount(header.idLength);
			
			if(!stream.GetAll(&id, header.idLength)) {
				return false;
			}
		}
		
		LOG(header);
		
		return true;
	}

	bool TGARaster::Create() {
		Stream& stream = GetStream();
		size = Size(0, 0);
		
		if(!stream.IsOpen()) {
			SetError();
			return false;
		}
		
		TGAHeader header;
		
		if(!LoadTGAHeader(stream, header, file)) {
			SetError();
			return false;
		}
		
		size.cx = header.imageSpec.width;
		size.cy = header.imageSpec.height;

		auto alphaChannelBits = header.imageSpec.descriptor & 0b00001111;
		
		if(header.colorMapSpec.length) {
			palette.Alloc(header.colorMapSpec.length);
		}
		
		switch(header.imageSpec.depth) {
			case 32:
				if(alphaChannelBits) {
					fmt.Set32le(0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000);
				}
				else {
					fmt.Set32le(0x00ff0000, 0x0000ff00, 0x000000ff);
				}
				break;
			case 24:
				fmt.Set24le(0xff0000, 0x00ff00, 0x0000ff);
				break;
			case 16:
				if(alphaChannelBits)
					fmt.Set8A();
				else
				    fmt.Set16le(0b0111110000000000, 0b0000001111100000, 0b0000000000011111);
				break;
			case 8:
				fmt.Set8();
				break;
		}
		
		if(header.colorMapSpec.length) {
			RGBA col;
			palette.Alloc(header.colorMapSpec.length);

			switch(header.colorMapSpec.entrySize) {
				case 32:
					stream.Get(palette.begin(), sizeof(RGBA) * header.colorMapSpec.length);
					break;
				case 24:
					for(int i = 0; i < header.colorMapSpec.length; i++) {
						col.b = stream.Get8();
						col.g = stream.Get8();
						col.r = stream.Get8();
						col.a = 255;
						palette[i] = col;
					}
					break;
				case 16:
					break;
				case 8:
					break;
			}
		}
		
		info.kind    = IMAGE_OPAQUE;
		info.bpp     = header.imageSpec.depth;
		info.dots    = GetDotSize(size, 72.0 , 72.0);
		info.hotspot = Point(0, 0);
		info.colors  = fmt.GetColorCount();

		switch(header.imageSpec.descriptor >> 4) {
			case TGA_BOTTOM_LEFT:
				info.orientation = FLIP_MIRROR_VERT;
				break;
			case TGA_BOTTOM_RIGHT:
				info.orientation = FLIP_MIRROR_HORZ;
				break;
			case TGA_TOP_LEFT:
				info.orientation = FLIP_NONE;
				break;
			case TGA_TOP_RIGHT:
				info.orientation = FLIP_TRANSVERSE;
				break;
		}
		
		topdown      = header.imageSpec.height < 0;
		rowBytes     = fmt.GetByteCount(size.cx);
		soff         = stream.GetPos();
		
		scanline.Alloc(rowBytes);
		ImageBuffer ib(size);
		RGBA *t = ib.Begin();
		
		int    y {};
		int    x {};
		uint8  c {};
		uint32 pos {};
		
		Buffer<uint8> pixel;
		const int bytesPerPixel = info.bpp / 8;
		pixel.Alloc(bytesPerPixel);
		
		switch(header.imageType) {
			case TGA_UNCOMPRESSED_BLACK_WHITE:
			case TGA_RLE_BLACK_WHITE:
				palette.Alloc(255);
				
				for(int i = 0; i < 255; i++) {
					palette[i].a = 255;
					palette[i].r = i;
					palette[i].g = i;
					palette[i].b = i;
				}
				break;
		}
		
		switch(header.imageType) {
			case TGA_UNCOMPRESSED_TRUE_COLOR:
			case TGA_UNCOMPRESSED_COLOR_MAP:
			case TGA_UNCOMPRESSED_BLACK_WHITE:
				while(y < size.cy) {
					stream.Get(scanline, rowBytes);
					fmt.Read(t, scanline, size.cx, palette);
					t += size.cx;
					y++;
				}
				break;
			case TGA_RLE_TRUE_COLOR:
			case TGA_RLE_COLOR_MAP:
			case TGA_RLE_BLACK_WHITE:
				while(y < size.cy) {
					c = stream.Get8();
					
					if(c & TGA_REPEAT_COUNT_BIT) {
						c &= TGA_REPEAT_COUNT_MASK;
						c++;
						x += c;
						
						stream.Get(pixel, bytesPerPixel);
						
						while(c--) {
							Copy(&scanline[pos], pixel, pixel + bytesPerPixel);
							pos += bytesPerPixel;
						}
					}
					else {
						c++;
						stream.Get(&scanline[pos], bytesPerPixel * c);
						pos += bytesPerPixel * c;
						x += c;
					}
					
					if(x == size.cx) {
						fmt.Read(t, scanline, size.cx, palette);
						t += size.cx;
						y++;
						x = 0;
						pos = 0;
					}
				}
				break;
			default:
				return false;
		}
		
		rle = new ImageRaster(ib);
		
		return true;
	}

	
	Size TGARaster::GetSize()  {
		return size;
	}
	
	ImageRaster::Info TGARaster::GetInfo()  {
		return info;
	}
	
	ImageRaster::Line TGARaster::GetLine(int line)  {
		if(rle) {
			return rle->GetLine(line);
		}
		
		return Line();
	}
	
	const RGBA* TGARaster::GetPalette()  {
		return palette;
	}
	
	const RasterFormat* TGARaster::GetFormat()  {
		return &fmt;
	}
	
	Value TGARaster::GetMetaData(String id)  {
		return Value(0);
	}
	
	void  TGARaster::EnumMetaData(Vector<String>& id_list)  {
	}
	
	INITIALIZER(TGARaster) {
		StreamRaster::Register<TGARaster>();
	}
}
