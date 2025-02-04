#include "CodecBase.h"

#include <math.h>

namespace Engine
{
	namespace Codec
	{
		bool IsPalettePixel(PixelFormat format)
		{
			return (format == PixelFormat::P8 || format == PixelFormat::P4 || format == PixelFormat::P2 || format == PixelFormat::P1);
		}
		uint32 GetPaletteVolume(PixelFormat format)
		{
			uint32 v = 1;
			for (int i = 0; i < int(GetBitsPerPixel(format)); i++) v <<= 1;
			return v;
		}
		uint32 GetBitsPerPixel(PixelFormat format)
		{
			if (format == PixelFormat::B8G8R8A8) return 32;
			else if (format == PixelFormat::B8G8R8X8) return 32;
			else if (format == PixelFormat::R8G8B8A8) return 32;
			else if (format == PixelFormat::R8G8B8X8) return 32;
			else if (format == PixelFormat::B8G8R8) return 24;
			else if (format == PixelFormat::R8G8B8) return 24;
			else if (format == PixelFormat::B5G5R5A1) return 16;
			else if (format == PixelFormat::B5G5R5X1) return 16;
			else if (format == PixelFormat::B5G6R5) return 16;
			else if (format == PixelFormat::R5G5B5A1) return 16;
			else if (format == PixelFormat::R5G5B5X1) return 16;
			else if (format == PixelFormat::R5G6B5) return 16;
			else if (format == PixelFormat::B4G4R4A4) return 16;
			else if (format == PixelFormat::B4G4R4X4) return 16;
			else if (format == PixelFormat::R4G4B4A4) return 16;
			else if (format == PixelFormat::R4G4B4X4) return 16;
			else if (format == PixelFormat::R8A8) return 16;
			else if (format == PixelFormat::B2G3R2A1) return 8;
			else if (format == PixelFormat::B2G3R2X1) return 8;
			else if (format == PixelFormat::B2G3R3) return 8;
			else if (format == PixelFormat::R2G3B2A1) return 8;
			else if (format == PixelFormat::R2G3B2X1) return 8;
			else if (format == PixelFormat::R3G3B2) return 8;
			else if (format == PixelFormat::B2G2R2A2) return 8;
			else if (format == PixelFormat::B2G2R2X2) return 8;
			else if (format == PixelFormat::R2G2B2A2) return 8;
			else if (format == PixelFormat::R2G2B2X2) return 8;
			else if (format == PixelFormat::R4A4) return 8;
			else if (format == PixelFormat::A8) return 8;
			else if (format == PixelFormat::R8) return 8;
			else if (format == PixelFormat::P8) return 8;
			else if (format == PixelFormat::R2A2) return 4;
			else if (format == PixelFormat::A4) return 4;
			else if (format == PixelFormat::R4) return 4;
			else if (format == PixelFormat::P4) return 4;
			else if (format == PixelFormat::R1A1) return 2;
			else if (format == PixelFormat::A2) return 2;
			else if (format == PixelFormat::R2) return 2;
			else if (format == PixelFormat::P2) return 2;
			else if (format == PixelFormat::A1) return 1;
			else if (format == PixelFormat::R1) return 1;
			else if (format == PixelFormat::P1) return 1;
			else return 0;
		}
		uint32 GetRedChannel(uint32 source, PixelFormat source_format, AlphaMode source_alpha)
		{
			uint32 ev = 0;
			if (source_format == PixelFormat::A8 || source_format == PixelFormat::A4) return 255;
			else if (source_format == PixelFormat::A2 || source_format == PixelFormat::A1) return 255;
			if (source_format == PixelFormat::B8G8R8A8 || source_format == PixelFormat::B8G8R8) ev = (source & 0x00FF0000) >> 16;
			else if (source_format == PixelFormat::R8G8B8A8 || source_format == PixelFormat::R8G8B8) ev = (source & 0x000000FF);
			else if (source_format == PixelFormat::B8G8R8X8) ev = (source & 0x00FF0000) >> 16;
			else if (source_format == PixelFormat::R8G8B8X8) ev = (source & 0x000000FF);
			else if (source_format == PixelFormat::B5G5R5A1 || source_format == PixelFormat::B5G5R5X1) ev = ((source & 0x7C00) >> 10) * 255 / 31;
			else if (source_format == PixelFormat::B5G6R5) ev = ((source & 0xF800) >> 11) * 255 / 31;
			else if (source_format == PixelFormat::R5G5B5A1 || source_format == PixelFormat::R5G5B5X1 || source_format == PixelFormat::R5G6B5) ev = (source & 0x001F) * 255 / 31;
			else if (source_format == PixelFormat::B4G4R4A4 || source_format == PixelFormat::B4G4R4X4) ev = ((source & 0x0F00) >> 8) * 255 / 15;
			else if (source_format == PixelFormat::R4G4B4A4 || source_format == PixelFormat::R4G4B4X4) ev = (source & 0x000F) * 255 / 15;
			else if (source_format == PixelFormat::R8 || source_format == PixelFormat::R8A8) ev = source & 0xFF;
			else if (source_format == PixelFormat::R4 || source_format == PixelFormat::R4A4) ev = (source & 0x0F) * 255 / 15;
			else if (source_format == PixelFormat::R2 || source_format == PixelFormat::R2A2) ev = (source & 0x3) * 255 / 3;
			else if (source_format == PixelFormat::R1 || source_format == PixelFormat::R1A1) ev = (source & 0x1) * 255;
			else if (source_format == PixelFormat::B2G3R2A1 || source_format == PixelFormat::B2G3R2X1) ev = ((source & 0x60) >> 5) * 255 / 3;
			else if (source_format == PixelFormat::R2G3B2A1 || source_format == PixelFormat::R2G3B2X1) ev = (source & 0x03) * 255 / 3;
			else if (source_format == PixelFormat::R2G2B2A2 || source_format == PixelFormat::R2G2B2X2) ev = (source & 0x03) * 255 / 3;
			else if (source_format == PixelFormat::B2G2R2A2 || source_format == PixelFormat::B2G2R2X2) ev = ((source & 0x30) >> 4) * 255 / 3;
			else if (source_format == PixelFormat::B2G3R3) ev = ((source & 0xE0) >> 5) * 255 / 7;
			else if (source_format == PixelFormat::R3G3B2) ev = (source & 0x07) * 255 / 7;
			if (source_alpha == AlphaMode::Premultiplied) {
				uint32 ac = GetAlphaChannel(source, source_format, source_alpha);
				if (ac) { ev *= 255; ev /= ac; }
			}
			return ev;
		}
		uint32 GetGreenChannel(uint32 source, PixelFormat source_format, AlphaMode source_alpha)
		{
			uint32 ev = 0;
			if (source_format == PixelFormat::A8 || source_format == PixelFormat::A4) return 255;
			else if (source_format == PixelFormat::A2 || source_format == PixelFormat::A1) return 255;
			if (source_format == PixelFormat::B8G8R8A8 || source_format == PixelFormat::B8G8R8) ev = (source & 0x0000FF00) >> 8;
			else if (source_format == PixelFormat::R8G8B8A8 || source_format == PixelFormat::R8G8B8) ev = (source & 0x0000FF00) >> 8;
			else if (source_format == PixelFormat::B8G8R8X8) ev = (source & 0x0000FF00) >> 8;
			else if (source_format == PixelFormat::R8G8B8X8) ev = (source & 0x0000FF00) >> 8;
			else if (source_format == PixelFormat::B5G5R5A1 || source_format == PixelFormat::B5G5R5X1) ev = ((source & 0x03E0) >> 5) * 255 / 31;
			else if (source_format == PixelFormat::R5G5B5A1 || source_format == PixelFormat::R5G5B5X1) ev = ((source & 0x03E0) >> 5) * 255 / 31;
			else if (source_format == PixelFormat::R5G6B5 || source_format == PixelFormat::B5G6R5) ev = ((source & 0x07E0) >> 5) * 255 / 63;
			else if (source_format == PixelFormat::B4G4R4A4 || source_format == PixelFormat::B4G4R4X4) ev = ((source & 0x00F0) >> 4) * 255 / 15;
			else if (source_format == PixelFormat::R4G4B4A4 || source_format == PixelFormat::R4G4B4X4) ev = ((source & 0x00F0) >> 4) * 255 / 15;
			else if (source_format == PixelFormat::R8 || source_format == PixelFormat::R8A8) ev = source & 0xFF;
			else if (source_format == PixelFormat::R4 || source_format == PixelFormat::R4A4) ev = (source & 0x0F) * 255 / 15;
			else if (source_format == PixelFormat::R2 || source_format == PixelFormat::R2A2) ev = (source & 0x3) * 255 / 3;
			else if (source_format == PixelFormat::R1 || source_format == PixelFormat::R1A1) ev = (source & 0x1) * 255;
			else if (source_format == PixelFormat::B2G3R2A1 || source_format == PixelFormat::R2G3B2A1) ev = ((source & 0x1C) >> 2) * 255 / 7;
			else if (source_format == PixelFormat::B2G3R2X1 || source_format == PixelFormat::R2G3B2X1) ev = ((source & 0x1C) >> 2) * 255 / 7;
			else if (source_format == PixelFormat::B2G3R3) ev = ((source & 0x1C) >> 2) * 255 / 7;
			else if (source_format == PixelFormat::R3G3B2) ev = ((source & 0x38) >> 3) * 255 / 7;
			else if (source_format == PixelFormat::B2G2R2A2 || source_format == PixelFormat::B2G2R2X2) ev = ((source & 0x0C) >> 2) * 255 / 3;
			else if (source_format == PixelFormat::R2G2B2A2 || source_format == PixelFormat::R2G2B2X2) ev = ((source & 0x0C) >> 2) * 255 / 3;
			if (source_alpha == AlphaMode::Premultiplied) {
				uint32 ac = GetAlphaChannel(source, source_format, source_alpha);
				if (ac) { ev *= 255; ev /= ac; }
			}
			return ev;
		}
		uint32 GetBlueChannel(uint32 source, PixelFormat source_format, AlphaMode source_alpha)
		{
			uint32 ev = 0;
			if (source_format == PixelFormat::A8 || source_format == PixelFormat::A4) return 255;
			else if (source_format == PixelFormat::A2 || source_format == PixelFormat::A1) return 255;
			if (source_format == PixelFormat::B8G8R8A8 || source_format == PixelFormat::B8G8R8) ev = (source & 0x000000FF);
			else if (source_format == PixelFormat::R8G8B8A8 || source_format == PixelFormat::R8G8B8) ev = (source & 0x00FF0000) >> 16;
			else if (source_format == PixelFormat::B8G8R8X8) ev = (source & 0x000000FF);
			else if (source_format == PixelFormat::R8G8B8X8) ev = (source & 0x00FF0000) >> 16;
			else if (source_format == PixelFormat::R5G5B5A1 || source_format == PixelFormat::R5G5B5X1) ev = ((source & 0x7C00) >> 10) * 255 / 31;
			else if (source_format == PixelFormat::R5G6B5) ev = ((source & 0xF800) >> 11) * 255 / 31;
			else if (source_format == PixelFormat::B5G5R5A1 || source_format == PixelFormat::B5G5R5X1 || source_format == PixelFormat::B5G6R5) ev = (source & 0x001F) * 255 / 31;
			else if (source_format == PixelFormat::R4G4B4A4 || source_format == PixelFormat::R4G4B4X4) ev = ((source & 0x0F00) >> 8) * 255 / 15;
			else if (source_format == PixelFormat::B4G4R4A4 || source_format == PixelFormat::B4G4R4X4) ev = (source & 0x000F) * 255 / 15;
			else if (source_format == PixelFormat::R8 || source_format == PixelFormat::R8A8) ev = source & 0xFF;
			else if (source_format == PixelFormat::R4 || source_format == PixelFormat::R4A4) ev = (source & 0x0F) * 255 / 15;
			else if (source_format == PixelFormat::R2 || source_format == PixelFormat::R2A2) ev = (source & 0x3) * 255 / 3;
			else if (source_format == PixelFormat::R1 || source_format == PixelFormat::R1A1) ev = (source & 0x1) * 255;
			else if (source_format == PixelFormat::R2G3B2A1 || source_format == PixelFormat::R2G3B2X1) ev = ((source & 0x60) >> 5) * 255 / 3;
			else if (source_format == PixelFormat::B2G3R2A1 || source_format == PixelFormat::B2G3R2X1) ev = (source & 0x03) * 255 / 3;
			else if (source_format == PixelFormat::B2G2R2A2 || source_format == PixelFormat::B2G2R2X2) ev = (source & 0x03) * 255 / 3;
			else if (source_format == PixelFormat::B2G3R3) ev = (source & 0x03) * 255 / 3;
			else if (source_format == PixelFormat::R2G2B2A2 || source_format == PixelFormat::R2G2B2X2) ev = ((source & 0x30) >> 4) * 255 / 3;
			else if (source_format == PixelFormat::R3G3B2) ev = ((source & 0xC0) >> 6) * 255 / 3;
			if (source_alpha == AlphaMode::Premultiplied) {
				uint32 ac = GetAlphaChannel(source, source_format, source_alpha);
				if (ac) { ev *= 255; ev /= ac; }
			}
			return ev;
		}
		uint32 GetAlphaChannel(uint32 source, PixelFormat source_format, AlphaMode source_alpha)
		{
			uint32 ev = 0;
			if (source_format == PixelFormat::B8G8R8A8) ev = (source & 0xFF000000) >> 24;
			else if (source_format == PixelFormat::R8G8B8A8) ev = (source & 0xFF000000) >> 24;
			else if (source_format == PixelFormat::B8G8R8X8 || source_format == PixelFormat::B8G8R8) ev = 0xFF;
			else if (source_format == PixelFormat::R8G8B8X8 || source_format == PixelFormat::R8G8B8) ev = 0xFF;
			else if (source_format == PixelFormat::R5G5B5A1 || source_format == PixelFormat::B5G5R5A1) ev = ((source & 0x8000) >> 15) * 255;
			else if (source_format == PixelFormat::R4G4B4A4 || source_format == PixelFormat::B4G4R4A4) ev = ((source & 0xF000) >> 12) * 255 / 15;
			else if (source_format == PixelFormat::R5G5B5X1 || source_format == PixelFormat::B5G5R5X1) ev = 0xFF;
			else if (source_format == PixelFormat::R4G4B4X4 || source_format == PixelFormat::B4G4R4X4) ev = 0xFF;
			else if (source_format == PixelFormat::R5G6B5 || source_format == PixelFormat::B5G6R5) ev = 0xFF;
			else if (source_format == PixelFormat::B2G3R2A1 || source_format == PixelFormat::R2G3B2A1) ev = ((source & 0x80) >> 7) * 255;
			else if (source_format == PixelFormat::B2G3R2X1 || source_format == PixelFormat::R2G3B2X1) ev = 0xFF;
			else if (source_format == PixelFormat::B2G3R3 || source_format == PixelFormat::R3G3B2) ev = 0xFF;
			else if (source_format == PixelFormat::B2G2R2A2 || source_format == PixelFormat::R2G2B2A2) ev = ((source & 0xC0) >> 6) * 255 / 3;
			else if (source_format == PixelFormat::B2G2R2X2 || source_format == PixelFormat::R2G2B2X2) ev = 0xFF;
			else if (source_format == PixelFormat::A8) ev = (source & 0xFF);
			else if (source_format == PixelFormat::R8) ev = 0xFF;
			else if (source_format == PixelFormat::R8A8) ev = (source & 0xFF00) >> 8;
			else if (source_format == PixelFormat::A4) ev = (source & 0x0F) * 255 / 15;
			else if (source_format == PixelFormat::R4) ev = 0xFF;
			else if (source_format == PixelFormat::R4A4) ev = ((source & 0xF0) >> 4) * 255 / 15;
			else if (source_format == PixelFormat::A2) ev = (source & 0x3) * 255 / 3;
			else if (source_format == PixelFormat::R2) ev = 0xFF;
			else if (source_format == PixelFormat::R2A2) ev = ((source & 0xC) >> 2) * 255 / 3;
			else if (source_format == PixelFormat::A1) ev = (source & 0x1) * 255;
			else if (source_format == PixelFormat::R1) ev = 0xFF;
			else if (source_format == PixelFormat::R1A1) ev = ((source & 0x2) >> 1) * 255;
			return ev;
		}
		uint32 MakePixel(uint32 r, uint32 g, uint32 b, uint32 a, PixelFormat format, AlphaMode alpha)
		{
			if (alpha == AlphaMode::Premultiplied) {
				r *= a; r /= 255;
				g *= a; g /= 255;
				b *= a; b /= 255;
			}
			if (format == PixelFormat::B8G8R8A8) {
				return b | (g << 8) | (r << 16) | (a << 24);
			} else if (format == PixelFormat::R8G8B8A8) {
				return r | (g << 8) | (b << 16) | (a << 24);
			} else if (format == PixelFormat::B8G8R8X8 || format == PixelFormat::B8G8R8) {
				return b | (g << 8) | (r << 16);
			} else if (format == PixelFormat::R8G8B8X8 || format == PixelFormat::R8G8B8) {
				return r | (g << 8) | (b << 16);
			} else if (format == PixelFormat::B5G5R5A1) {
				return ((b + 4) * 31 / 255) | (((g + 4) * 31 / 255) << 5) | (((r + 4) * 31 / 255) << 10) | (((a + 128) / 255) << 15);
			} else if (format == PixelFormat::B5G5R5X1) {
				return ((b + 4) * 31 / 255) | (((g + 4) * 31 / 255) << 5) | (((r + 4) * 31 / 255) << 10);
			} else if (format == PixelFormat::B5G6R5) {
				return ((b + 4) * 31 / 255) | (((g + 2) * 63 / 255) << 5) | (((r + 4) * 31 / 255) << 11);
			} else if (format == PixelFormat::R5G5B5A1) {
				return ((r + 4) * 31 / 255) | (((g + 4) * 31 / 255) << 5) | (((b + 4) * 31 / 255) << 10) | (((a + 128) / 255) << 15);
			} else if (format == PixelFormat::R5G5B5X1) {
				return ((r + 4) * 31 / 255) | (((g + 4) * 31 / 255) << 5) | (((b + 4) * 31 / 255) << 10);
			} else if (format == PixelFormat::R5G6B5) {
				return ((r + 4) * 31 / 255) | (((g + 2) * 63 / 255) << 5) | (((b + 4) * 31 / 255) << 11);
			} else if (format == PixelFormat::B4G4R4A4) {
				return ((b + 8) * 15 / 255) | (((g + 8) * 15 / 255) << 4) | (((r + 8) * 15 / 255) << 8) | (((a + 8) * 15 / 255) << 12);
			} else if (format == PixelFormat::B4G4R4X4) {
				return ((b + 8) * 15 / 255) | (((g + 8) * 15 / 255) << 4) | (((r + 8) * 15 / 255) << 8);
			} else if (format == PixelFormat::R4G4B4A4) {
				return ((r + 8) * 15 / 255) | (((g + 8) * 15 / 255) << 4) | (((b + 8) * 15 / 255) << 8) | (((a + 8) * 15 / 255) << 12);
			} else if (format == PixelFormat::R4G4B4X4) {
				return ((r + 8) * 15 / 255) | (((g + 8) * 15 / 255) << 4) | (((b + 8) * 15 / 255) << 8);
			} else if (format == PixelFormat::R8A8) {
				return ((r + g + b + 1) / 3) | (a << 8);
			} else if (format == PixelFormat::R4A4) {
				return ((r + g + b + 25) * 5 / 255) | (((a + 8) * 15 / 255) << 4);
			} else if (format == PixelFormat::R2A2) {
				return ((r + g + b + 128) * 3 / 765) | (((a + 42) * 3 / 255) << 2);
			} else if (format == PixelFormat::R1A1) {
				return ((r + g + b + 383) / 765) | (((a + 128) / 255) << 1);
			} else if (format == PixelFormat::B2G3R2A1) {
				return ((b + 42) * 3 / 255) | (((g + 18) * 7 / 255) << 2) | (((r + 42) * 3 / 255) << 5) | (((a + 128) / 255) << 7);
			} else if (format == PixelFormat::B2G3R2X1) {
				return ((b + 42) * 3 / 255) | (((g + 18) * 7 / 255) << 2) | (((r + 42) * 3 / 255) << 5);
			} else if (format == PixelFormat::B2G3R3) {
				return ((b + 42) * 3 / 255) | (((g + 18) * 7 / 255) << 2) | (((r + 18) * 7 / 255) << 5);
			} else if (format == PixelFormat::R2G3B2A1) {
				return ((r + 42) * 3 / 255) | (((g + 18) * 7 / 255) << 2) | (((b + 42) * 3 / 255) << 5) | (((a + 128) / 255) << 7);
			} else if (format == PixelFormat::R2G3B2X1) {
				return ((r + 42) * 3 / 255) | (((g + 18) * 7 / 255) << 2) | (((b + 42) * 3 / 255) << 5);
			} else if (format == PixelFormat::R3G3B2) {
				return ((r + 18) * 7 / 255) | (((g + 18) * 7 / 255) << 3) | (((b + 42) * 3 / 255) << 6);
			} else if (format == PixelFormat::B2G2R2A2) {
				return ((b + 42) * 3 / 255) | (((g + 42) * 3 / 255) << 2) | (((r + 42) * 3 / 255) << 4) | (((a + 42) * 3 / 255) << 6);
			} else if (format == PixelFormat::B2G2R2X2) {
				return ((b + 42) * 3 / 255) | (((g + 42) * 3 / 255) << 2) | (((r + 42) * 3 / 255) << 4);
			} else if (format == PixelFormat::R2G2B2A2) {
				return ((r + 42) * 3 / 255) | (((g + 42) * 3 / 255) << 2) | (((b + 42) * 3 / 255) << 4) | (((a + 42) * 3 / 255) << 6);
			} else if (format == PixelFormat::R2G2B2X2) {
				return ((r + 42) * 3 / 255) | (((g + 42) * 3 / 255) << 2) | (((b + 42) * 3 / 255) << 4);
			} else if (format == PixelFormat::A8) {
				return a;
			} else if (format == PixelFormat::R8) {
				return (r + g + b + 1) / 3;
			} else if (format == PixelFormat::A4) {
				return (a + 8) * 15 / 255;
			} else if (format == PixelFormat::R4) {
				return (r + g + b + 25) * 5 / 255;
			} else if (format == PixelFormat::A2) {
				return ((a + 42) * 3 / 255);
			} else if (format == PixelFormat::R2) {
				return ((r + g + b + 128) * 3 / 765);
			} else if (format == PixelFormat::A1) {
				return ((a + 128) / 255);
			} else if (format == PixelFormat::R1) {
				return ((r + g + b + 383) / 765);
			} else return 0;
		}
		uint32 ConvertPixel(uint32 source, PixelFormat source_format, AlphaMode source_alpha, PixelFormat format, AlphaMode alpha)
		{
			return MakePixel(
				GetRedChannel(source, source_format, source_alpha),
				GetGreenChannel(source, source_format, source_alpha),
				GetBlueChannel(source, source_format, source_alpha),
				GetAlphaChannel(source, source_format, source_alpha),
				format, alpha);
		}

		Frame::Frame(const Frame & src)
		{
			Width = src.Width; Height = src.Height; ScanLineLength = src.ScanLineLength;
			Format = src.Format; Alpha = src.Alpha; Origin = src.Origin;
			Palette = src.Palette;
			auto size = ScanLineLength * Height;
			RawData = reinterpret_cast<uint8 *>(malloc(size));
			if (!RawData) throw OutOfMemoryException();
			MemoryCopy(RawData, src.RawData, size);
			Usage = src.Usage; HotPointX = src.HotPointX; HotPointY = src.HotPointY;
			Duration = src.Duration; DpiUsage = src.DpiUsage;
		}
		Frame::Frame(const Frame * src) : Frame(*src) {}
		Frame::Frame(int32 width, int32 height, PixelFormat format) :
			Frame(width, height, -1, format, AlphaMode::Normal, ScanOrigin::TopDown) {}
		Frame::Frame(int32 width, int32 height, PixelFormat format, AlphaMode alpha) :
			Frame(width, height, -1, format, alpha, ScanOrigin::TopDown) {}
		Frame::Frame(int32 width, int32 height, PixelFormat format, ScanOrigin origin) :
			Frame(width, height, -1, format, AlphaMode::Normal, origin) {}
		Frame::Frame(int32 width, int32 height, PixelFormat format, AlphaMode alpha, ScanOrigin origin) :
			Frame(width, height, -1, format, alpha, origin) {}
		Frame::Frame(int32 width, int32 height, int32 scan_line_length, PixelFormat format) :
			Frame(width, height, scan_line_length, format, AlphaMode::Normal, ScanOrigin::TopDown) {}
		Frame::Frame(int32 width, int32 height, int32 scan_line_length, PixelFormat format, AlphaMode alpha) :
			Frame(width, height, scan_line_length, format, alpha, ScanOrigin::TopDown) {}
		Frame::Frame(int32 width, int32 height, int32 scan_line_length, PixelFormat format, ScanOrigin origin) :
			Frame(width, height, scan_line_length, format, AlphaMode::Normal, origin) {}
		Frame::Frame(int32 width, int32 height, int32 scan_line_length, PixelFormat format, AlphaMode alpha, ScanOrigin origin) :
			Width(width), Height(height), ScanLineLength(scan_line_length), Format(format), Alpha(alpha), Origin(origin), Palette(0x10)
		{
			if (width <= 0 || height <= 0 || scan_line_length < -1 || scan_line_length == 0) throw InvalidArgumentException();
			if (ScanLineLength == -1) ScanLineLength = ((Width * GetBitsPerPixel(format) + 31) / 32) * 4;
			RawData = reinterpret_cast<uint8 *>(malloc(ScanLineLength * Height));
			if (!RawData) throw OutOfMemoryException();
			ZeroMemory(RawData, ScanLineLength * Height);
			if (IsPalettePixel(Format)) {
				Palette.SetLength(Engine::Codec::GetPaletteVolume(Format));
				ZeroMemory(Palette.GetBuffer(), 4 * Palette.Length());
			}
		}
		Frame::~Frame(void) { free(RawData); }
		int32 Frame::GetWidth(void) const { return Width; }
		int32 Frame::GetHeight(void) const { return Height; }
		int32 Frame::GetScanLineLength(void) const { return ScanLineLength; }
		PixelFormat Frame::GetPixelFormat(void) const { return Format; }
		AlphaMode Frame::GetAlphaMode(void) const { return Alpha; }
		ScanOrigin Frame::GetScanOrigin(void) const { return Origin; }
		const uint32 * Frame::GetPalette(void) const { return Palette.GetBuffer(); }
		uint32 * Frame::GetPalette(void) { return Palette.GetBuffer(); }
		int Frame::GetPaletteVolume(void) const { return Palette.Length(); }
		void Frame::SetPaletteVolume(int volume)
		{
			if (!IsPalettePixel(Format)) throw InvalidArgumentException();
			if (volume < 1 || volume > 0x100) throw InvalidArgumentException();
			for (int y = 0; y < Height; y++) for (int x = 0; x < Width; x++) {
				if (GetPixel(x, y) >= uint32(volume)) throw InvalidArgumentException();
			}
			Palette.SetLength(volume);
		}
		uint32 Frame::GetPixel(int x, int y) const
		{
			if (Origin == ScanOrigin::BottomUp) y = Height - y - 1;
			uint32 bpp = GetBitsPerPixel(Format);
			if (bpp == 32) {
				return *reinterpret_cast<const uint32 *>(RawData + ScanLineLength * y + 4 * x);
			} else if (bpp == 24) {
				int base = ScanLineLength * y + 3 * x;
				return RawData[base] | (uint32(RawData[base + 1]) << 8) | (uint32(RawData[base + 2]) << 16);
			} else if (bpp == 16) {
				int base = ScanLineLength * y + 2 * x;
				return RawData[base] | (uint32(RawData[base + 1]) << 8);
			} else if (bpp <= 8) {
				uint64 base = uint64(ScanLineLength) * 8 * y + uint64(bpp) * x;
				int byte = int(base / 8);
				int bit = 8 - base % 8 - bpp;
				uint8 b = RawData[byte];
				if (bpp == 8) return b;
				else if (bpp == 4) return (b >> bit) & 0xF;
				else if (bpp == 2) return (b >> bit) & 0x3;
				else if (bpp == 1) return (b >> bit) & 0x1;
				else return 0;
			} else return 0;
		}
		void Frame::SetPixel(int x, int y, uint32 v)
		{
			if (Origin == ScanOrigin::BottomUp) y = Height - y - 1;
			uint32 bpp = GetBitsPerPixel(Format);
			if (bpp == 32) {
				*reinterpret_cast<uint32 *>(RawData + ScanLineLength * y + 4 * x) = v;
			} else if (bpp == 24) {
				int base = ScanLineLength * y + 3 * x;
				RawData[base] = v & 0xFF;
				RawData[base + 1] = (v & 0xFF00) >> 8;
				RawData[base + 2] = (v & 0xFF0000) >> 16;
			} else if (bpp == 16) {
				int base = ScanLineLength * y + 2 * x;
				RawData[base] = v & 0xFF;
				RawData[base + 1] = (v & 0xFF00) >> 8;
			} else if (bpp <= 8) {
				uint64 base = uint64(ScanLineLength) * 8 * y + uint64(bpp) * x;
				int byte = int(base / 8);
				int bit = 8 - base % 8 - bpp;
				if (bpp == 8) RawData[byte] = v;
				else if (bpp == 4) {
					RawData[byte] &= 0xFF ^ (0xF << bit);
					RawData[byte] |= v << bit;
				} else if (bpp == 2) {
					RawData[byte] &= 0xFF ^ (0x3 << bit);
					RawData[byte] |= v << bit;
				} else if (bpp == 1) {
					RawData[byte] &= 0xFF ^ (0x1 << bit);
					RawData[byte] |= v << bit;
				}
			}
		}
		uint8 * Frame::GetData(void) { return RawData; }
		const uint8 * Frame::GetData(void) const { return RawData; }
		uint32 Frame::GetBestPaletteIndex(uint32 color) const
		{
			int32 b = color & 0xFF;
			int32 g = (color >> 8) & 0xFF;
			int32 r = (color >> 16) & 0xFF;
			int32 a = (color >> 24) & 0xFF;
			int32 min_dist = -1;
			int32 min_index = -1;
			for (int i = 0; i < Palette.Length(); i++) {
				int32 pb = Palette[i] & 0xFF;
				int32 pg = (Palette[i] >> 8) & 0xFF;
				int32 pr = (Palette[i] >> 16) & 0xFF;
				int32 pa = (Palette[i] >> 24) & 0xFF;
				int32 dist = (pr - r) * (pr - r) + (pg - g) * (pg - g) + (pb - b) * (pb - b) + (pa - a) * (pa - a);
				if (dist < min_dist || min_index == -1) {
					min_dist = dist;
					min_index = i;
				}
			}
			return min_index;
		}
		uint32 Frame::ReadPixel(int x, int y, PixelFormat format, AlphaMode alpha) const
		{
			auto val = GetPixel(x, y);
			PixelFormat lf = Format; AlphaMode la = Alpha;
			if (IsPalettePixel(Format)) { val = Palette[val]; lf = PixelFormat::B8G8R8A8; la = AlphaMode::Normal; }
			return ConvertPixel(val, lf, la, format, alpha);
		}
		void Frame::WritePixel(int x, int y, uint32 v, PixelFormat format, AlphaMode alpha)
		{
			PixelFormat lf = Format; AlphaMode la = Alpha;
			auto is_palette = IsPalettePixel(Format);
			if (is_palette) { lf = PixelFormat::B8G8R8A8; la = AlphaMode::Normal; }
			auto val = ConvertPixel(v, format, alpha, lf, la);
			if (is_palette) val = GetBestPaletteIndex(val);
			SetPixel(x, y, val);
		}
		uint32 Frame::ReadPalette(int index, PixelFormat format, AlphaMode alpha) const
		{
			if (index < 0 || index >= Palette.Length()) throw InvalidArgumentException();
			return ConvertPixel(Palette[index], PixelFormat::B8G8R8A8, AlphaMode::Normal, format, alpha);
		}
		void Frame::WritePalette(int index, uint32 v, PixelFormat format, AlphaMode alpha)
		{
			if (index < 0 || index >= Palette.Length()) throw InvalidArgumentException();
			Palette[index] = ConvertPixel(v, format, alpha, PixelFormat::B8G8R8A8, AlphaMode::Normal);
		}
		Frame * Frame::ConvertFormat(PixelFormat format, int scan_line) const { return ConvertFormat(format, Alpha, Origin, scan_line); }
		Frame * Frame::ConvertFormat(AlphaMode alpha, int scan_line) const { return ConvertFormat(Format, alpha, Origin, scan_line); }
		Frame * Frame::ConvertFormat(ScanOrigin origin, int scan_line) const { return ConvertFormat(Format, Alpha, origin, scan_line); }
		Frame * Frame::ConvertFormat(PixelFormat format, AlphaMode alpha, int scan_line) const { return ConvertFormat(format, alpha, Origin, scan_line); }
		Frame * Frame::ConvertFormat(PixelFormat format, ScanOrigin origin, int scan_line) const { return ConvertFormat(format, Alpha, origin, scan_line); }
		Frame * Frame::ConvertFormat(AlphaMode alpha, ScanOrigin origin, int scan_line) const { return ConvertFormat(Format, alpha, origin, scan_line); }
		Frame * Frame::ConvertFormat(PixelFormat format, AlphaMode alpha, ScanOrigin origin, int scan_line) const
		{
			SafePointer<Frame> New = new Frame(Width, Height, scan_line, format, alpha, origin);
			if (!IsPalettePixel(format) && !IsPalettePixel(Format)) {
				if (format == Format && alpha == Alpha) {
					for (int y = 0; y < Height; y++) for (int x = 0; x < Width; x++) {
						New->SetPixel(x, y, GetPixel(x, y));
					}
				} else {
					for (int y = 0; y < Height; y++) for (int x = 0; x < Width; x++) {
						New->SetPixel(x, y, ConvertPixel(GetPixel(x, y), Format, Alpha, format, alpha));
					}
				}
			} else if (!IsPalettePixel(format)){
				for (int y = 0; y < Height; y++) for (int x = 0; x < Width; x++) {
					New->SetPixel(x, y, ConvertPixel(Palette[GetPixel(x, y)], PixelFormat::B8G8R8A8, AlphaMode::Normal, format, alpha));
				}
			} else throw Exception();
			New->DpiUsage = DpiUsage;
			New->Duration = Duration;
			New->HotPointX = HotPointX;
			New->HotPointY = HotPointY;
			New->Usage = Usage;
			New->Retain();
			return New;
		}

		Image::Image(void) : Frames(0x10) {}
		Image::~Image(void) {}
		Frame * Image::GetFrameBestSizeFit(int32 w, int32 h) const
		{
			if (!Frames.Length()) return 0;
			int best_i = -1;
			double min_dist = 0.0;
			for (int i = 0; i < Frames.Length(); i++) {
				double dist = sqrt(pow(double(w - Frames[i].GetWidth()), 2.0) + pow(double(h - Frames[i].GetHeight()), 2.0));
				if (dist < min_dist || best_i == -1) {
					best_i = i;
					min_dist = dist;
				}
			}
			return (best_i >= 0) ? Frames.ElementAt(best_i) : 0;
		}
		Frame * Image::GetFrameBestDpiFit(double dpi) const
		{
			if (!Frames.Length()) return 0;
			int best_i = -1;
			double min_dist = 0.0;
			for (int i = 0; i < Frames.Length(); i++) {
				double dist = fabs(dpi - Frames[i].DpiUsage);
				if (dist < min_dist || best_i == -1) {
					best_i = i;
					min_dist = dist;
				}
			}
			return (best_i >= 0) ? Frames.ElementAt(best_i) : 0;
		}
		Frame * Image::GetFrameByUsage(FrameUsage usage) const
		{
			for (int i = 0; i < Frames.Length(); i++) if (Frames[i].Usage == usage) return Frames.ElementAt(i);
			return 0;
		}
		Frame * Image::GetFramePreciseSize(int32 w, int32 h) const
		{
			for (int i = 0; i < Frames.Length(); i++) if (Frames[i].GetWidth() == w && Frames[i].GetHeight() == h) return Frames.ElementAt(i);
			return 0;
		}

		ObjectArray<ICodec> Codecs(0x10);

		ICodec * FindEncoder(const string & format)
		{
			for (int i = 0; i < Codecs.Length(); i++) if (Codecs[i].CanEncode(format)) return Codecs.ElementAt(i);
			return 0;
		}
		ICodec * FindDecoder(const string & format)
		{
			for (int i = 0; i < Codecs.Length(); i++) if (Codecs[i].CanDecode(format)) return Codecs.ElementAt(i);
			return 0;
		}
		ICodec::ICodec(void) { Codecs.Append(this); }
		ICodec::~ICodec(void) {}
		void EncodeFrame(Streaming::Stream * stream, Frame * frame, const string & format)
		{
			for (int i = 0; i < Codecs.Length(); i++) {
				if (Codecs[i].CanEncode(format) && Codecs[i].IsFrameCodec()) {
					Codecs[i].EncodeFrame(stream, frame, format);
					return;
				}
			}
			throw InvalidFormatException();
		}
		void EncodeImage(Streaming::Stream * stream, Image * image, const string & format)
		{
			for (int i = 0; i < Codecs.Length(); i++) {
				if (Codecs[i].CanEncode(format) && Codecs[i].IsImageCodec()) {
					Codecs[i].EncodeImage(stream, image, format);
					return;
				}
			}
			throw InvalidFormatException();
		}
		Frame * DecodeFrame(Streaming::Stream * stream) { return DecodeFrame(stream, 0, 0); }
		Frame * DecodeFrame(Streaming::Stream * stream, string * format, ICodec ** decoder)
		{
			for (int i = 0; i < Codecs.Length(); i++) if (Codecs[i].IsFrameCodec()) {
				auto fmt = Codecs[i].ExamineData(stream);
				if (fmt.Length()) {
					if (format) *format = fmt;
					if (decoder) *decoder = Codecs.ElementAt(i);
					return Codecs[i].DecodeFrame(stream);
				}
			}
			return 0;
		}
		Image * DecodeImage(Streaming::Stream * stream) { return DecodeImage(stream, 0, 0); }
		Image * DecodeImage(Streaming::Stream * stream, string * format, ICodec ** decoder)
		{
			for (int i = 0; i < Codecs.Length(); i++) if (Codecs[i].IsImageCodec()) {
				auto fmt = Codecs[i].ExamineData(stream);
				if (fmt.Length()) {
					if (format) *format = fmt;
					if (decoder) *decoder = Codecs.ElementAt(i);
					return Codecs[i].DecodeImage(stream);
				}
			}
			return 0;
		}
		string GetEncodedImageFormat(Streaming::Stream * stream)
		{
			for (int i = 0; i < Codecs.Length(); i++) {
				if (Codecs[i].IsImageCodec()) {
					string format = Codecs[i].ExamineData(stream);
					if (format.Length()) return format;
				}
			}
			return L"";
		}
	}
}