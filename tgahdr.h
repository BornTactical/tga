#ifndef _plugin_tga_tgahdr_h_
#define _plugin_tga_tgahdr_h_

#if defined(COMPILER_MSC) || defined(COMPILER_GCC)
#pragma pack(push, 1)
#endif

struct ImageSpec {
	Upp::int16  xOrigin    {};
	Upp::int16  yOrigin    {};
	Upp::int16  width      {};
	Upp::int16  height     {};
	byte        depth      {};
	byte        descriptor {};
}
#ifdef COMPILER_GCC
__attribute__((packed))
#endif
;

struct ColorMapSpec {
	Upp::word firstEntry {};
	Upp::word length     {};
	byte      entrySize  {};
}
#ifdef COMPILER_GCC
__attribute__((packed))
#endif
;

struct TGAHeader {
	byte         idLength     {};
	byte         colorMap     {};
	byte         imageType    {};
	ColorMapSpec colorMapSpec {};
	ImageSpec    imageSpec    {};
	
	String ToString() const;
}
#ifdef COMPILER_GCC
__attribute__((packed))
#endif
;

struct TGAFooter {
	Upp::dword extensionAreaOffset      {};
	Upp::dword developerDirectoryOffset {};
	char       signature[18]            {};
}
#ifdef COMPILER_GCC
__attribute__((packed))
#endif
;

#if defined(COMPILER_MSC) || defined(COMPILER_GCC)
#pragma pack(pop)
#endif

#endif
