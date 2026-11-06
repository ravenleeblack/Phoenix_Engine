#define PHOENIX_SYSTEM_LOADER_DLL
#include "system_phoenix_loader.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

PHOENIX_LOADER_API bool load_image_asset(const char* filepath, loader_image_data* out_image) {
    if (!filepath || !out_image) return false;

    FILE* file = fopen(filepath, "rb");
    if (!file) return false;

    // 1. Read standard 18-byte TGA Header layout
    uint8_t header[18];
    if (fread(header, 1, 18, file) != 18) {
        fclose(file);
        return false;
    }

    // header[2] contains the image type code. 2 = Uncompressed true-color
    if (header[2] != 2) {
        fclose(file); // Reject compressed TGA formats or color-mapped variations
        return false;
    }

    // Extract structural dimensional assets out of the standard byte offsets
    int w = header[12] | (header[13] << 8);
    int h = header[14] | (header[15] << 8);
    int bpp = header[16]; // Bits per pixel (Should be 24-bit BGR or 32-bit BGRA)

    if (bpp != 24 && bpp != 32) {
        fclose(file);
        return false;
    }

    int src_channels = bpp / 8;
    uint32_t src_pixels_bytes = w * h * src_channels;
    
    uint8_t* raw_file_pixels = (uint8_t*)malloc(src_pixels_bytes);
    if (!raw_file_pixels) {
        fclose(file);
        return false;
    }

    if (fread(raw_file_pixels, 1, src_pixels_bytes, file) != src_pixels_bytes) {
        free(raw_file_pixels);
        fclose(file);
        return false;
    }
    fclose(file);

    // 2. Normalize byte stream strictly to 4-channels (RGBA/BGRA) for direct Vulkan texture allocations
    uint8_t* final_buffer = (uint8_t*)malloc(w * h * 4);
    if (!final_buffer) {
        free(raw_file_pixels);
        return false;
    }

    // Check image layout orientation. If bit 5 of descriptor is set, it's top-to-bottom
    bool flip_y = !(header[17] & 0x20);

    for (int y = 0; y < h; y++) {
        int src_y = flip_y ? (h - 1 - y) : y;
        for (int x = 0; x < w; x++) {
            uint32_t src_idx = (src_y * w + x) * src_channels;
            uint32_t dest_idx = (y * w + x) * 4;

            // TGA naturally maps channels as Blue, Green, Red
            final_buffer[dest_idx + 0] = raw_file_pixels[src_idx + 2]; // R
            final_buffer[dest_idx + 1] = raw_file_pixels[src_idx + 1]; // G
            final_buffer[dest_idx + 2] = raw_file_pixels[src_idx + 0]; // B
            final_buffer[dest_idx + 3] = (src_channels == 4) ? raw_file_pixels[src_idx + 3] : 255; // A
        }
    }

    free(raw_file_pixels);

    out_image->pixels = final_buffer;
    out_image->width = w;
    out_image->height = h;
    out_image->channels = 4;
    return true;
}

PHOENIX_LOADER_API void remove_image_asset(loader_image_data* image) {
    if (image && image->pixels) {
        free(image->pixels);
        image->pixels = NULL;
    }
}

PHOENIX_LOADER_API bool load_3d_mesh_asset(const char* filepath, loader_mesh_data* out_mesh) {
    if (!filepath || !out_mesh) return false;

    FILE* file = fopen(filepath, "rb");
    if (!file) return false;

    // Read the component structural footprint face_type blocks down sequentially
    if (fread(&out_mesh->vertex_count, sizeof(uint32_t), 1, file) != 1 ||
        fread(&out_mesh->index_count, sizeof(uint32_t), 1, file) != 1) {
        fclose(file);
        return false;
    }

    out_mesh->vertices = (loader_vertex*)malloc(sizeof(loader_vertex) * out_mesh->vertex_count);
    out_mesh->indices  = (uint32_t*)malloc(sizeof(uint32_t) * out_mesh->index_count);

    if (!out_mesh->vertices || !out_mesh->indices) {
        if (out_mesh->vertices) free(out_mesh->vertices);
        if (out_mesh->indices) free(out_mesh->indices);
        fclose(file);
        return false;
    }

    if (fread(out_mesh->vertices, sizeof(loader_vertex), out_mesh->vertex_count, file) != out_mesh->vertex_count ||
        fread(out_mesh->indices, sizeof(uint32_t), out_mesh->index_count, file) != out_mesh->index_count) {
        free(out_mesh->vertices);
        free(out_mesh->indices);
        fclose(file);
        return false;
    }

    fclose(file);
    return true;
}

PHOENIX_LOADER_API void remove_3d_mesh_asset(loader_mesh_data* mesh) {
    if (mesh) {
        if (mesh->vertices) free(mesh->vertices);
        if (mesh->indices)  free(mesh->indices);
        mesh->vertices = NULL;
        mesh->indices = NULL;
    }
}
