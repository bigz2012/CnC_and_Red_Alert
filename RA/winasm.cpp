#include <stdint.h>
#include <string.h>
#include "gbuffer.h"

#ifndef LORES

#define SIZE_OF_PALETTE	256
extern "C"{
	extern unsigned char PaletteInterpolationTable[SIZE_OF_PALETTE][SIZE_OF_PALETTE];
}

extern "C" void ModeX_Blit(GraphicBufferClass * source)
{
	printf("%s\n", __func__);
}

extern "C" void Asm_Interpolate(unsigned char* src_ptr, unsigned char* dest_ptr, int lines, int src_width, int dest_width)
{
	// this one line doubles by skipping every other line
	auto old_dest = dest_ptr;

	auto interp_table_flat = (uint8_t *)PaletteInterpolationTable;

	do
	{
		int width_counter = 0;
		int pixel_count = (src_width - 2) / 2; // we copy two at a time, so this isn't actually a pixel count

		auto out_ptr = old_dest;

		// convert 2 pixels of source into 4 pixels of destination
		do
		{
			// read four pixels (we use three)
			auto in_pixels = *(uint32_t *)src_ptr;
			src_ptr += 2;

			auto interped = interp_table_flat[in_pixels & 0xFFFF];
			auto interped2 = interp_table_flat[(in_pixels >> 8) & 0xFFFF];
			
			auto out_pixels = (in_pixels & 0xFF) | interped << 8 | (in_pixels & 0xFF00) << 8 | interped2 << 24;

			*(uint32_t *)out_ptr = out_pixels;
			out_ptr += 4;

		}
		while(--pixel_count);

		// do the last three pixels and a blank
		auto in_pixels = *(uint16_t *)src_ptr;
		src_ptr += 2;
		*out_ptr++ = in_pixels & 0xFF;
		*out_ptr++ = interp_table_flat[in_pixels];
		*out_ptr++ = in_pixels >> 8;
		*out_ptr++ = 0;

		old_dest += dest_width; // skip every other line
	}
	while(--lines);
}

extern "C" void Asm_Interpolate_Line_Double(unsigned char* src_ptr, unsigned char* dest_ptr, int lines, int src_width, int dest_width)
{
	auto old_dest = dest_ptr;

	auto interp_table_flat = (uint8_t *)PaletteInterpolationTable;

	do
	{
		int width_counter = 0;
		int pixel_count = (src_width - 2) / 2; // we copy two at a time, so this isn't actually a pixel count

		auto out_ptr = old_dest;
		auto out2_ptr = old_dest + dest_width / 2; // dest width is 2x the actual width...

		// convert 2 pixels of source into 4 pixels of destination
		do
		{
			// read four pixels (we use three)
			auto in_pixels = *(uint32_t *)src_ptr;
			src_ptr += 2;

			auto interped = interp_table_flat[in_pixels & 0xFFFF];
			auto interped2 = interp_table_flat[(in_pixels >> 8) & 0xFFFF];
			
			auto out_pixels = (in_pixels & 0xFF) | interped << 8 | (in_pixels & 0xFF00) << 8 | interped2 << 24;

			*(uint32_t *)out_ptr = out_pixels;
			out_ptr += 4;
			*(uint32_t *)out2_ptr = out_pixels;
			out2_ptr += 4;
		}
		while(--pixel_count);

		// do the last three pixels and a blank
		auto in_pixels = *(uint16_t *)src_ptr;
		src_ptr += 2;
		*out_ptr++ = *out2_ptr++ = in_pixels & 0xFF;
		*out_ptr++ = *out2_ptr++ = interp_table_flat[in_pixels];
		*out_ptr++ = *out2_ptr++ = in_pixels >> 8;
		*out_ptr++ = *out2_ptr++ = 0;

		old_dest += dest_width;
	}
	while(--lines);
}

extern "C" void Asm_Interpolate_Line_Interpolate(unsigned char* src_ptr, unsigned char* dest_ptr, int lines, int src_width, int dest_width)
{
	// This function interpolates a source buffer to double resolution
	// with interpolation between both horizontal pixels AND between lines.
	// Uses two temporary line buffers, alternating between them.
	// Algorithm from winasm.asm: Interpolate_Single_Line + Interpolate_Between_Lines

	auto interp_table_flat = (uint8_t *)PaletteInterpolationTable;

	// Allocate temporary line buffers (doubled width = src_width * 2)
	int doubled_width = src_width * 2;
	static uint32_t TopLine[640];
	static uint32_t BottomLine[640];
	static uint8_t LineBuffer[2560]; // interpolated between-line buffer

	uint32_t *next_line = TopLine;
	uint32_t *last_line = BottomLine;

	auto old_dest = dest_ptr;

	// Helper: interpolate a single source line horizontally into a temp buffer (as dwords)
	auto Interpolate_Single_Line = [&](unsigned char *source, uint32_t *dest_buf) {
		int pixel_count = (src_width - 2) / 2;
		auto src = source;
		auto out = (uint8_t *)dest_buf;

		// Convert 2 source pixels into 4 destination pixels
		for(int i = 0; i < pixel_count; i++)
		{
			auto in_pixels = *(uint32_t *)src;
			src += 2;

			auto interped = interp_table_flat[in_pixels & 0xFFFF];
			auto interped2 = interp_table_flat[(in_pixels >> 8) & 0xFFFF];

			auto out_pixels = (in_pixels & 0xFF) | interped << 8 | (in_pixels & 0xFF00) << 8 | interped2 << 24;
			*(uint32_t *)out = out_pixels;
			out += 4;
		}

		// Last three pixels and a blank
		auto in_pixels = *(uint16_t *)src;
		*out++ = in_pixels & 0xFF;
		*out++ = interp_table_flat[in_pixels];
		*out++ = in_pixels >> 8;
		*out++ = 0;
	};

	// Helper: interpolate between two already-doubled lines
	auto Interpolate_Between_Lines = [&](uint32_t *source1, uint32_t *source2, uint8_t *destination, int width) {
		auto s1 = (uint8_t *)source1;
		auto s2 = (uint8_t *)source2;
		int count = width * 2; // doubled width in bytes
		for(int i = 0; i < count; i++)
		{
			destination[i] = interp_table_flat[s1[i] | (s2[i] << 8)];
		}
	};

	int half_dest_width = dest_width / 2;
	int pixel_dwords = src_width / 2; // number of dwords to copy per line

	// Interpolate first source line
	Interpolate_Single_Line(src_ptr, next_line);
	// Swap next_line and last_line
	auto temp = next_line; next_line = last_line; last_line = temp;
	src_ptr += src_width;
	lines--;

	// Process each pair of lines
	while(lines > 0)
	{
		Interpolate_Single_Line(src_ptr, next_line);
		Interpolate_Between_Lines(last_line, next_line, LineBuffer, src_width);

		// Copy last_line to dest
		memcpy(old_dest, last_line, pixel_dwords * 4);
		old_dest += half_dest_width;

		// Copy interpolated line to dest
		memcpy(old_dest, LineBuffer, pixel_dwords * 4);
		old_dest += half_dest_width;

		src_ptr += src_width;
		temp = next_line; next_line = last_line; last_line = temp;
		lines--;
	}

	// Copy final interpolated line
	Interpolate_Single_Line(src_ptr, next_line);
	memcpy(old_dest, next_line, pixel_dwords * 4);
}

#endif