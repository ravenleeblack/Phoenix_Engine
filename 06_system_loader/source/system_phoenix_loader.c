#define PHOENIX_SYSTEM_LOADER_DLL
#include "system_phoenix_loader.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>

/* ========================================================================= */
/* the phoenix loader: png + tga images, fbx meshes (binary and ascii).       */
/* no dependencies: inflate (zlib), png and fbx are all in this file.         */
/* ========================================================================= */

/* ------------------------------------------------------------------------- */
/* small helpers                                                              */
/* ------------------------------------------------------------------------- */
uint8_t* loader_read_file(const char* path, size_t* out_size)
{
    FILE* file = fopen(path, "rb");
    if (!file) {
        return NULL;
    }
    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    fseek(file, 0, SEEK_SET);
    if (length < 0) {
        fclose(file);
        return NULL;
    }
    uint8_t* data = (uint8_t*)malloc((size_t)length + 1);
    if (!data) {
        fclose(file);
        return NULL;
    }
    size_t got = fread(data, 1, (size_t)length, file);
    fclose(file);
    if (got != (size_t)length) {
        free(data);
        return NULL;
    }
    data[length] = 0;
    *out_size = (size_t)length;
    return data;
}

bool loader_write_file(const char* path, const uint8_t* data, size_t size)
{
    FILE* file = fopen(path, "wb");
    if (!file) {
        return false;
    }
    size_t wrote = fwrite(data, 1, size, file);
    fclose(file);
    return wrote == size;
}

uint32_t loader_read_be32(const uint8_t* p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

uint32_t loader_read_le32(const uint8_t* p)
{
    return ((uint32_t)p[3] << 24) | ((uint32_t)p[2] << 16) | ((uint32_t)p[1] << 8) | (uint32_t)p[0];
}

uint64_t loader_read_le64(const uint8_t* p)
{
    return (uint64_t)loader_read_le32(p) | ((uint64_t)loader_read_le32(p + 4) << 32);
}

bool loader_has_extension(const char* path, const char* extension)
{
    size_t path_length = strlen(path);
    size_t extension_length = strlen(extension);
    if (path_length < extension_length) {
        return false;
    }
    const char* tail = path + (path_length - extension_length);
    for (size_t i = 0; i < extension_length; i++) {
        char a = tail[i];
        char b = extension[i];
        if (a >= 'A' && a <= 'Z') {
            a = (char)(a - 'A' + 'a');
        }
        if (a != b) {
            return false;
        }
    }
    return true;
}

/* growable byte buffer: used by the writers */
typedef struct {
    uint8_t* data;
    size_t   size;
    size_t   capacity;
    bool     failed;
} loader_buffer;

void loader_buffer_append(loader_buffer* buffer, const void* source, size_t count)
{
    if (buffer->failed || count == 0) {
        return;
    }
    if (buffer->size + count > buffer->capacity) {
        size_t new_capacity = buffer->capacity;
        if (new_capacity < 4096) {
            new_capacity = 4096;
        }
        while (new_capacity < buffer->size + count) {
            new_capacity *= 2;
        }
        uint8_t* grown = (uint8_t*)realloc(buffer->data, new_capacity);
        if (!grown) {
            buffer->failed = true;
            return;
        }
        buffer->data = grown;
        buffer->capacity = new_capacity;
    }
    if (source) {
        memcpy(buffer->data + buffer->size, source, count);
    } else {
        memset(buffer->data + buffer->size, 0, count);
    }
    buffer->size += count;
}

void loader_buffer_text(loader_buffer* buffer, const char* format, ...)
{
    char line[1024];
    va_list args;
    va_start(args, format);
    int length = vsnprintf(line, sizeof(line), format, args);
    va_end(args);
    if (length > 0 && length < (int)sizeof(line)) {
        loader_buffer_append(buffer, line, (size_t)length);
    }
}

/* ------------------------------------------------------------------------- */
/* inflate (rfc 1951) + zlib wrapper (rfc 1950)                               */
/* ------------------------------------------------------------------------- */
typedef struct {
    const uint8_t* source;
    size_t         source_length;
    size_t         source_position;
    uint32_t       bit_buffer;
    int            bit_count;
    uint8_t*       out;
    size_t         out_length;
    size_t         out_capacity;
    bool           failed;
} loader_inflate_state;

typedef struct {
    uint16_t count[16];
    uint16_t symbol[288];
} loader_huffman;

const uint16_t loader_length_base[29]   = { 3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,35,43,51,59,67,83,99,115,131,163,195,227,258 };
const uint16_t loader_length_extra[29]  = { 0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0 };
const uint16_t loader_distance_base[30] = { 1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,257,385,513,769,1025,1537,2049,3073,4097,6145,8193,12289,16385,24577 };
const uint16_t loader_distance_extra[30] = { 0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13 };
const uint8_t  loader_code_length_order[19] = { 16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15 };

int loader_inflate_bits(loader_inflate_state* state, int need)
{
    uint32_t value = state->bit_buffer;
    while (state->bit_count < need) {
        if (state->source_position >= state->source_length) {
            state->failed = true;
            return 0;
        }
        value |= (uint32_t)state->source[state->source_position] << state->bit_count;
        state->source_position++;
        state->bit_count += 8;
    }
    state->bit_buffer = value >> need;
    state->bit_count -= need;
    return (int)(value & ((1u << need) - 1u));
}

int loader_inflate_decode(loader_inflate_state* state, const loader_huffman* huffman)
{
    int code = 0;
    int first = 0;
    int index = 0;
    for (int length = 1; length <= 15; length++) {
        code |= loader_inflate_bits(state, 1);
        if (state->failed) {
            return -1;
        }
        int count = huffman->count[length];
        if (code - count < first) {
            return huffman->symbol[index + (code - first)];
        }
        index += count;
        first += count;
        first <<= 1;
        code <<= 1;
    }
    return -1;
}

int loader_huffman_build(loader_huffman* huffman, const uint16_t* lengths, int symbol_count)
{
    for (int length = 0; length <= 15; length++) {
        huffman->count[length] = 0;
    }
    for (int symbol = 0; symbol < symbol_count; symbol++) {
        huffman->count[lengths[symbol]]++;
    }
    if (huffman->count[0] == symbol_count) {
        return 0;
    }
    int left = 1;
    for (int length = 1; length <= 15; length++) {
        left <<= 1;
        left -= huffman->count[length];
        if (left < 0) {
            return left;
        }
    }
    uint16_t offsets[16];
    offsets[1] = 0;
    for (int length = 1; length < 15; length++) {
        offsets[length + 1] = (uint16_t)(offsets[length] + huffman->count[length]);
    }
    for (int symbol = 0; symbol < symbol_count; symbol++) {
        if (lengths[symbol] != 0) {
            huffman->symbol[offsets[lengths[symbol]]] = (uint16_t)symbol;
            offsets[lengths[symbol]]++;
        }
    }
    return left;
}

bool loader_inflate_put(loader_inflate_state* state, uint8_t value)
{
    if (state->out_length >= state->out_capacity) {
        size_t new_capacity = state->out_capacity * 2;
        if (new_capacity < 4096) {
            new_capacity = 4096;
        }
        uint8_t* grown = (uint8_t*)realloc(state->out, new_capacity);
        if (!grown) {
            state->failed = true;
            return false;
        }
        state->out = grown;
        state->out_capacity = new_capacity;
    }
    state->out[state->out_length] = value;
    state->out_length++;
    return true;
}

bool loader_inflate_codes(loader_inflate_state* state, const loader_huffman* length_code, const loader_huffman* distance_code)
{
    for (;;) {
        int symbol = loader_inflate_decode(state, length_code);
        if (symbol < 0 || state->failed) {
            return false;
        }
        if (symbol < 256) {
            if (!loader_inflate_put(state, (uint8_t)symbol)) {
                return false;
            }
        } else if (symbol == 256) {
            return true;
        } else {
            symbol -= 257;
            if (symbol >= 29) {
                return false;
            }
            int length = loader_length_base[symbol] + loader_inflate_bits(state, loader_length_extra[symbol]);
            int distance_symbol = loader_inflate_decode(state, distance_code);
            if (distance_symbol < 0 || distance_symbol >= 30 || state->failed) {
                return false;
            }
            size_t distance = (size_t)loader_distance_base[distance_symbol] + (size_t)loader_inflate_bits(state, loader_distance_extra[distance_symbol]);
            if (state->failed || distance > state->out_length) {
                return false;
            }
            for (int i = 0; i < length; i++) {
                uint8_t copied = state->out[state->out_length - distance];
                if (!loader_inflate_put(state, copied)) {
                    return false;
                }
            }
        }
    }
}

bool loader_inflate_stored(loader_inflate_state* state)
{
    state->bit_buffer = 0;
    state->bit_count = 0;
    if (state->source_position + 4 > state->source_length) {
        return false;
    }
    uint32_t length = (uint32_t)state->source[state->source_position] | ((uint32_t)state->source[state->source_position + 1] << 8);
    uint32_t inverse = (uint32_t)state->source[state->source_position + 2] | ((uint32_t)state->source[state->source_position + 3] << 8);
    state->source_position += 4;
    if (length != (~inverse & 0xFFFFu)) {
        return false;
    }
    if (state->source_position + length > state->source_length) {
        return false;
    }
    for (uint32_t i = 0; i < length; i++) {
        if (!loader_inflate_put(state, state->source[state->source_position + i])) {
            return false;
        }
    }
    state->source_position += length;
    return true;
}

bool loader_inflate_fixed(loader_inflate_state* state)
{
    loader_huffman length_code;
    loader_huffman distance_code;
    uint16_t lengths[288];
    int symbol = 0;
    for (; symbol < 144; symbol++) {
        lengths[symbol] = 8;
    }
    for (; symbol < 256; symbol++) {
        lengths[symbol] = 9;
    }
    for (; symbol < 280; symbol++) {
        lengths[symbol] = 7;
    }
    for (; symbol < 288; symbol++) {
        lengths[symbol] = 8;
    }
    loader_huffman_build(&length_code, lengths, 288);
    for (symbol = 0; symbol < 30; symbol++) {
        lengths[symbol] = 5;
    }
    loader_huffman_build(&distance_code, lengths, 30);
    return loader_inflate_codes(state, &length_code, &distance_code);
}

bool loader_inflate_dynamic(loader_inflate_state* state)
{
    uint16_t lengths[320];
    loader_huffman length_code;
    loader_huffman distance_code;

    int literal_count = loader_inflate_bits(state, 5) + 257;
    int distance_count = loader_inflate_bits(state, 5) + 1;
    int code_count = loader_inflate_bits(state, 4) + 4;
    if (state->failed || literal_count > 286 || distance_count > 30) {
        return false;
    }

    int index = 0;
    for (; index < code_count; index++) {
        lengths[loader_code_length_order[index]] = (uint16_t)loader_inflate_bits(state, 3);
    }
    for (; index < 19; index++) {
        lengths[loader_code_length_order[index]] = 0;
    }
    if (loader_huffman_build(&length_code, lengths, 19) != 0) {
        return false;
    }

    index = 0;
    while (index < literal_count + distance_count) {
        int symbol = loader_inflate_decode(state, &length_code);
        if (symbol < 0 || state->failed) {
            return false;
        }
        if (symbol < 16) {
            lengths[index] = (uint16_t)symbol;
            index++;
        } else {
            uint16_t previous = 0;
            int repeat = 0;
            if (symbol == 16) {
                if (index == 0) {
                    return false;
                }
                previous = lengths[index - 1];
                repeat = 3 + loader_inflate_bits(state, 2);
            } else if (symbol == 17) {
                repeat = 3 + loader_inflate_bits(state, 3);
            } else {
                repeat = 11 + loader_inflate_bits(state, 7);
            }
            if (index + repeat > literal_count + distance_count) {
                return false;
            }
            while (repeat > 0) {
                lengths[index] = previous;
                index++;
                repeat--;
            }
        }
    }
    if (lengths[256] == 0) {
        return false;
    }

    int error = loader_huffman_build(&length_code, lengths, literal_count);
    if (error < 0 || (error > 0 && literal_count - length_code.count[0] != 1)) {
        return false;
    }
    error = loader_huffman_build(&distance_code, lengths + literal_count, distance_count);
    if (error < 0 || (error > 0 && distance_count - distance_code.count[0] != 1)) {
        return false;
    }
    return loader_inflate_codes(state, &length_code, &distance_code);
}

/* raw deflate stream -> newly allocated bytes. size_hint pre-sizes the output (0 = unknown). */
bool loader_inflate(const uint8_t* source, size_t source_length, uint8_t** out_data, size_t* out_length, size_t size_hint)
{
    loader_inflate_state state;
    memset(&state, 0, sizeof(state));
    state.source = source;
    state.source_length = source_length;
    if (size_hint > 0) {
        state.out = (uint8_t*)malloc(size_hint);
        if (!state.out) {
            return false;
        }
        state.out_capacity = size_hint;
    }

    int last = 0;
    do {
        last = loader_inflate_bits(&state, 1);
        int type = loader_inflate_bits(&state, 2);
        bool ok = false;
        if (state.failed) {
            ok = false;
        } else if (type == 0) {
            ok = loader_inflate_stored(&state);
        } else if (type == 1) {
            ok = loader_inflate_fixed(&state);
        } else if (type == 2) {
            ok = loader_inflate_dynamic(&state);
        }
        if (!ok || state.failed) {
            free(state.out);
            return false;
        }
    } while (!last);

    *out_data = state.out;
    *out_length = state.out_length;
    return true;
}

bool loader_zlib_decompress(const uint8_t* source, size_t source_length, uint8_t** out_data, size_t* out_length, size_t size_hint)
{
    if (source_length < 2 || (source[0] & 0x0F) != 8) {
        return false;
    }
    return loader_inflate(source + 2, source_length - 2, out_data, out_length, size_hint);
}

/* ------------------------------------------------------------------------- */
/* png                                                                        */
/* ------------------------------------------------------------------------- */
const uint8_t loader_png_signature[8] = { 137, 80, 78, 71, 13, 10, 26, 10 };

uint32_t loader_crc_table[256];
bool     loader_crc_ready = false;

uint32_t loader_crc32(uint32_t crc, const uint8_t* data, size_t length)
{
    if (!loader_crc_ready) {
        for (uint32_t n = 0; n < 256; n++) {
            uint32_t c = n;
            for (int k = 0; k < 8; k++) {
                if (c & 1u) {
                    c = 0xEDB88320u ^ (c >> 1);
                } else {
                    c = c >> 1;
                }
            }
            loader_crc_table[n] = c;
        }
        loader_crc_ready = true;
    }
    uint32_t c = crc ^ 0xFFFFFFFFu;
    for (size_t i = 0; i < length; i++) {
        c = loader_crc_table[(c ^ data[i]) & 0xFFu] ^ (c >> 8);
    }
    return c ^ 0xFFFFFFFFu;
}

uint32_t loader_adler32(const uint8_t* data, size_t length)
{
    uint32_t a = 1;
    uint32_t b = 0;
    for (size_t i = 0; i < length; i++) {
        a = (a + data[i]) % 65521u;
        b = (b + a) % 65521u;
    }
    return (b << 16) | a;
}

int loader_png_paeth(int a, int b, int c)
{
    int p = a + b - c;
    int pa = p - a;
    int pb = p - b;
    int pc = p - c;
    if (pa < 0) {
        pa = -pa;
    }
    if (pb < 0) {
        pb = -pb;
    }
    if (pc < 0) {
        pc = -pc;
    }
    if (pa <= pb && pa <= pc) {
        return a;
    }
    if (pb <= pc) {
        return b;
    }
    return c;
}

/* one sample (0..255) out of a decoded scanline */
int loader_png_sample(const uint8_t* line, int depth, int channels, int x, int channel)
{
    if (depth == 8) {
        return line[x * channels + channel];
    }
    if (depth == 16) {
        return line[(x * channels + channel) * 2];
    }
    int bit = x * depth;
    int shift = 8 - depth - (bit & 7);
    int mask = (1 << depth) - 1;
    int value = (line[bit >> 3] >> shift) & mask;
    return (value * 255) / mask;
}

bool loader_decode_png(const uint8_t* data, size_t size, loader_image_data* out_image)
{
    if (size < 8 || memcmp(data, loader_png_signature, 8) != 0) {
        return false;
    }

    uint32_t width = 0;
    uint32_t height = 0;
    int depth = 0;
    int color_type = 0;
    int interlace = 0;
    uint8_t palette[256][3];
    uint8_t palette_alpha[256];
    int palette_count = 0;
    memset(palette, 0, sizeof(palette));
    memset(palette_alpha, 255, sizeof(palette_alpha));

    uint8_t* compressed = NULL;
    size_t compressed_length = 0;
    bool header_seen = false;

    size_t position = 8;
    while (position + 12 <= size) {
        uint32_t chunk_length = loader_read_be32(data + position);
        const uint8_t* chunk_type = data + position + 4;
        const uint8_t* chunk_data = data + position + 8;
        if ((size_t)chunk_length > size - position - 12) {
            free(compressed);
            return false;
        }

        if (memcmp(chunk_type, "IHDR", 4) == 0 && chunk_length >= 13) {
            width = loader_read_be32(chunk_data);
            height = loader_read_be32(chunk_data + 4);
            depth = chunk_data[8];
            color_type = chunk_data[9];
            interlace = chunk_data[12];
            header_seen = true;
        } else if (memcmp(chunk_type, "PLTE", 4) == 0) {
            palette_count = (int)(chunk_length / 3);
            if (palette_count > 256) {
                palette_count = 256;
            }
            for (int i = 0; i < palette_count; i++) {
                palette[i][0] = chunk_data[i * 3];
                palette[i][1] = chunk_data[i * 3 + 1];
                palette[i][2] = chunk_data[i * 3 + 2];
            }
        } else if (memcmp(chunk_type, "tRNS", 4) == 0 && color_type == 3) {
            for (uint32_t i = 0; i < chunk_length && i < 256; i++) {
                palette_alpha[i] = chunk_data[i];
            }
        } else if (memcmp(chunk_type, "IDAT", 4) == 0) {
            uint8_t* grown = (uint8_t*)realloc(compressed, compressed_length + chunk_length);
            if (!grown) {
                free(compressed);
                return false;
            }
            compressed = grown;
            memcpy(compressed + compressed_length, chunk_data, chunk_length);
            compressed_length += chunk_length;
        } else if (memcmp(chunk_type, "IEND", 4) == 0) {
            break;
        }
        position += 12 + (size_t)chunk_length;
    }

    if (!header_seen || width == 0 || height == 0 || width > 16384 || height > 16384 || !compressed || interlace != 0) {
        free(compressed);
        return false;
    }

    int channels = 0;
    if (color_type == 0) {
        channels = 1;
    } else if (color_type == 2) {
        channels = 3;
    } else if (color_type == 3) {
        channels = 1;
    } else if (color_type == 4) {
        channels = 2;
    } else if (color_type == 6) {
        channels = 4;
    } else {
        free(compressed);
        return false;
    }
    if (depth != 1 && depth != 2 && depth != 4 && depth != 8 && depth != 16) {
        free(compressed);
        return false;
    }

    size_t bits_per_pixel = (size_t)channels * (size_t)depth;
    size_t stride = (((size_t)width * bits_per_pixel) + 7) / 8;
    size_t bytes_per_pixel = bits_per_pixel / 8;
    if (bytes_per_pixel < 1) {
        bytes_per_pixel = 1;
    }
    size_t expected = (stride + 1) * (size_t)height;

    uint8_t* raw = NULL;
    size_t raw_length = 0;
    bool inflated = loader_zlib_decompress(compressed, compressed_length, &raw, &raw_length, expected);
    free(compressed);
    if (!inflated || raw_length < expected) {
        free(raw);
        return false;
    }

    /* undo the per scanline filters in place */
    uint8_t* zero_line = (uint8_t*)calloc(stride, 1);
    if (!zero_line) {
        free(raw);
        return false;
    }
    for (uint32_t y = 0; y < height; y++) {
        uint8_t* row = raw + (size_t)y * (stride + 1);
        int filter = row[0];
        uint8_t* line = row + 1;
        const uint8_t* previous = zero_line;
        if (y > 0) {
            previous = raw + (size_t)(y - 1) * (stride + 1) + 1;
        }
        for (size_t i = 0; i < stride; i++) {
            int left = 0;
            int up = previous[i];
            int up_left = 0;
            if (i >= bytes_per_pixel) {
                left = line[i - bytes_per_pixel];
                up_left = previous[i - bytes_per_pixel];
            }
            int add = 0;
            if (filter == 1) {
                add = left;
            } else if (filter == 2) {
                add = up;
            } else if (filter == 3) {
                add = (left + up) / 2;
            } else if (filter == 4) {
                add = loader_png_paeth(left, up, up_left);
            } else if (filter != 0) {
                free(zero_line);
                free(raw);
                return false;
            }
            line[i] = (uint8_t)(line[i] + add);
        }
    }
    free(zero_line);

    uint8_t* pixels = (uint8_t*)malloc((size_t)width * (size_t)height * 4);
    if (!pixels) {
        free(raw);
        return false;
    }
    for (uint32_t y = 0; y < height; y++) {
        const uint8_t* line = raw + (size_t)y * (stride + 1) + 1;
        for (uint32_t x = 0; x < width; x++) {
            uint8_t* out = pixels + ((size_t)y * width + x) * 4;
            if (color_type == 0) {
                int g = loader_png_sample(line, depth, channels, (int)x, 0);
                out[0] = (uint8_t)g;
                out[1] = (uint8_t)g;
                out[2] = (uint8_t)g;
                out[3] = 255;
            } else if (color_type == 2) {
                out[0] = (uint8_t)loader_png_sample(line, depth, channels, (int)x, 0);
                out[1] = (uint8_t)loader_png_sample(line, depth, channels, (int)x, 1);
                out[2] = (uint8_t)loader_png_sample(line, depth, channels, (int)x, 2);
                out[3] = 255;
            } else if (color_type == 3) {
                int index = 0;
                if (depth == 8) {
                    index = line[x];
                } else {
                    int bit = (int)x * depth;
                    int shift = 8 - depth - (bit & 7);
                    index = (line[bit >> 3] >> shift) & ((1 << depth) - 1);
                }
                out[0] = palette[index & 255][0];
                out[1] = palette[index & 255][1];
                out[2] = palette[index & 255][2];
                out[3] = palette_alpha[index & 255];
            } else if (color_type == 4) {
                int g = loader_png_sample(line, depth, channels, (int)x, 0);
                out[0] = (uint8_t)g;
                out[1] = (uint8_t)g;
                out[2] = (uint8_t)g;
                out[3] = (uint8_t)loader_png_sample(line, depth, channels, (int)x, 1);
            } else {
                out[0] = (uint8_t)loader_png_sample(line, depth, channels, (int)x, 0);
                out[1] = (uint8_t)loader_png_sample(line, depth, channels, (int)x, 1);
                out[2] = (uint8_t)loader_png_sample(line, depth, channels, (int)x, 2);
                out[3] = (uint8_t)loader_png_sample(line, depth, channels, (int)x, 3);
            }
        }
    }
    free(raw);

    out_image->pixels = pixels;
    out_image->width = (int)width;
    out_image->height = (int)height;
    out_image->channels = 4;
    return true;
}

void loader_png_chunk(loader_buffer* buffer, const char* type, const uint8_t* data, size_t length)
{
    uint8_t header[8];
    header[0] = (uint8_t)(length >> 24);
    header[1] = (uint8_t)(length >> 16);
    header[2] = (uint8_t)(length >> 8);
    header[3] = (uint8_t)length;
    memcpy(header + 4, type, 4);
    loader_buffer_append(buffer, header, 8);
    loader_buffer_append(buffer, data, length);

    uint32_t crc = loader_crc32(0, header + 4, 4);
    crc = loader_crc32(crc, data, length);
    uint8_t tail[4] = { (uint8_t)(crc >> 24), (uint8_t)(crc >> 16), (uint8_t)(crc >> 8), (uint8_t)crc };
    loader_buffer_append(buffer, tail, 4);
}

/* png writer: filter 0 on every row, deflate "stored" blocks (no compression, always valid) */
bool loader_encode_png(const char* filepath, const loader_image_data* image)
{
    if (!image || !image->pixels || image->width <= 0 || image->height <= 0) {
        return false;
    }
    if (image->channels != 3 && image->channels != 4) {
        return false;
    }

    size_t width = (size_t)image->width;
    size_t height = (size_t)image->height;
    size_t channels = (size_t)image->channels;
    size_t row_bytes = width * channels;
    size_t raw_length = (row_bytes + 1) * height;
    uint8_t* raw = (uint8_t*)malloc(raw_length);
    if (!raw) {
        return false;
    }
    for (size_t y = 0; y < height; y++) {
        raw[y * (row_bytes + 1)] = 0;
        memcpy(raw + y * (row_bytes + 1) + 1, image->pixels + y * row_bytes, row_bytes);
    }

    loader_buffer zlib;
    memset(&zlib, 0, sizeof(zlib));
    uint8_t zlib_header[2] = { 0x78, 0x01 };
    loader_buffer_append(&zlib, zlib_header, 2);
    size_t offset = 0;
    while (offset < raw_length) {
        size_t block = raw_length - offset;
        if (block > 65535) {
            block = 65535;
        }
        uint8_t is_final = 0;
        if (offset + block >= raw_length) {
            is_final = 1;
        }
        uint8_t block_header[5] = { is_final, (uint8_t)(block & 0xFF), (uint8_t)(block >> 8), (uint8_t)(~block & 0xFF), (uint8_t)((~block >> 8) & 0xFF) };
        loader_buffer_append(&zlib, block_header, 5);
        loader_buffer_append(&zlib, raw + offset, block);
        offset += block;
    }
    uint32_t adler = loader_adler32(raw, raw_length);
    uint8_t adler_bytes[4] = { (uint8_t)(adler >> 24), (uint8_t)(adler >> 16), (uint8_t)(adler >> 8), (uint8_t)adler };
    loader_buffer_append(&zlib, adler_bytes, 4);
    free(raw);

    loader_buffer file;
    memset(&file, 0, sizeof(file));
    loader_buffer_append(&file, loader_png_signature, 8);
    uint8_t ihdr[13];
    ihdr[0] = (uint8_t)(width >> 24);  ihdr[1] = (uint8_t)(width >> 16);  ihdr[2] = (uint8_t)(width >> 8);  ihdr[3] = (uint8_t)width;
    ihdr[4] = (uint8_t)(height >> 24); ihdr[5] = (uint8_t)(height >> 16); ihdr[6] = (uint8_t)(height >> 8); ihdr[7] = (uint8_t)height;
    ihdr[8] = 8;
    if (channels == 4) {
        ihdr[9] = 6;
    } else {
        ihdr[9] = 2;
    }
    ihdr[10] = 0;
    ihdr[11] = 0;
    ihdr[12] = 0;
    loader_png_chunk(&file, "IHDR", ihdr, 13);
    loader_png_chunk(&file, "IDAT", zlib.data, zlib.size);
    loader_png_chunk(&file, "IEND", NULL, 0);

    bool ok = !zlib.failed && !file.failed && loader_write_file(filepath, file.data, file.size);
    free(zlib.data);
    free(file.data);
    return ok;
}

/* ------------------------------------------------------------------------- */
/* tga (uncompressed true color, kept from the first loader)                  */
/* ------------------------------------------------------------------------- */
bool loader_load_tga_file(const char* filepath, loader_image_data* out_image)
{
    FILE* file = fopen(filepath, "rb");
    if (!file) {
        return false;
    }

    uint8_t header[18];
    if (fread(header, 1, 18, file) != 18) {
        fclose(file);
        return false;
    }
    if (header[2] != 2) {
        fclose(file);
        return false;
    }

    int w = header[12] | (header[13] << 8);
    int h = header[14] | (header[15] << 8);
    int bpp = header[16];
    if ((bpp != 24 && bpp != 32) || w <= 0 || h <= 0) {
        fclose(file);
        return false;
    }

    int src_channels = bpp / 8;
    uint32_t src_pixels_bytes = (uint32_t)(w * h * src_channels);
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

    uint8_t* final_buffer = (uint8_t*)malloc((size_t)w * (size_t)h * 4);
    if (!final_buffer) {
        free(raw_file_pixels);
        return false;
    }

    bool flip_y = !(header[17] & 0x20);
    for (int y = 0; y < h; y++) {
        int src_y = y;
        if (flip_y) {
            src_y = h - 1 - y;
        }
        for (int x = 0; x < w; x++) {
            uint32_t src_idx = (uint32_t)((src_y * w + x) * src_channels);
            uint32_t dest_idx = (uint32_t)((y * w + x) * 4);
            final_buffer[dest_idx + 0] = raw_file_pixels[src_idx + 2];
            final_buffer[dest_idx + 1] = raw_file_pixels[src_idx + 1];
            final_buffer[dest_idx + 2] = raw_file_pixels[src_idx + 0];
            if (src_channels == 4) {
                final_buffer[dest_idx + 3] = raw_file_pixels[src_idx + 3];
            } else {
                final_buffer[dest_idx + 3] = 255;
            }
        }
    }
    free(raw_file_pixels);

    out_image->pixels = final_buffer;
    out_image->width = w;
    out_image->height = h;
    out_image->channels = 4;
    return true;
}

PHOENIX_LOADER_API bool load_image_asset(const char* filepath, loader_image_data* out_image)
{
    if (!filepath || !out_image) {
        return false;
    }
    memset(out_image, 0, sizeof(*out_image));

    if (loader_has_extension(filepath, ".png")) {
        size_t size = 0;
        uint8_t* data = loader_read_file(filepath, &size);
        if (!data) {
            return false;
        }
        bool ok = loader_decode_png(data, size, out_image);
        free(data);
        return ok;
    }
    return loader_load_tga_file(filepath, out_image);
}

PHOENIX_LOADER_API void remove_image_asset(loader_image_data* image)
{
    if (image && image->pixels) {
        free(image->pixels);
        image->pixels = NULL;
    }
}

PHOENIX_LOADER_API bool save_image_asset(const char* filepath, const loader_image_data* image)
{
    if (!filepath) {
        return false;
    }
    return loader_encode_png(filepath, image);
}

/* ------------------------------------------------------------------------- */
/* fbx loading: mesh geometry only (vertices, polygons, normals, uvs)         */
/*   binary (7.x) and ascii. every Geometry node with polygons is merged into  */
/*   one mesh. polygons are fanned into triangles, every polygon corner gets   */
/*   its own vertex so normals and uvs never smear.                           */
/* ------------------------------------------------------------------------- */
typedef enum {
    fbx_slot_none = 0,
    fbx_slot_vertices,
    fbx_slot_polygon_index,
    fbx_slot_normals,
    fbx_slot_normal_index,
    fbx_slot_uvs,
    fbx_slot_uv_index
} loader_fbx_slot;

typedef enum {
    fbx_map_polygon_vertex = 0,
    fbx_map_vertex,
    fbx_map_all_same,
    fbx_map_polygon
} loader_fbx_mapping;

typedef struct {
    double*  vertices;      size_t vertex_count;
    double*  polygon_index; size_t polygon_index_count;
    double*  normals;       size_t normal_count;
    double*  normal_index;  size_t normal_index_count;
    double*  uvs;           size_t uv_count;
    double*  uv_index;      size_t uv_index_count;
    int      normal_mapping;
    int      normal_indexed;
    int      uv_mapping;
    int      uv_indexed;
    int      normal_layers_seen;
    int      uv_layers_seen;
    int      layer;          /* 0 none, 1 inside LayerElementNormal, 2 inside LayerElementUV (first set of each only) */
    bool     in_geometry;
    double   unit_scale;     /* UnitScaleFactor: 1 = centimeters, 100 = meters */
    char     first_string[128];
    double   last_number;
    loader_vertex* out_vertices;
    uint32_t       out_vertex_count;
    uint32_t       out_vertex_capacity;
    uint32_t*      out_indices;
    uint32_t       out_index_count;
    uint32_t       out_index_capacity;
    bool     failed;
} loader_fbx_context;

void loader_fbx_free_arrays(loader_fbx_context* context)
{
    free(context->vertices);      context->vertices = NULL;      context->vertex_count = 0;
    free(context->polygon_index); context->polygon_index = NULL; context->polygon_index_count = 0;
    free(context->normals);       context->normals = NULL;       context->normal_count = 0;
    free(context->normal_index);  context->normal_index = NULL;  context->normal_index_count = 0;
    free(context->uvs);           context->uvs = NULL;           context->uv_count = 0;
    free(context->uv_index);      context->uv_index = NULL;      context->uv_index_count = 0;
    context->normal_mapping = fbx_map_polygon_vertex;
    context->normal_indexed = 0;
    context->uv_mapping = fbx_map_polygon_vertex;
    context->uv_indexed = 0;
    context->normal_layers_seen = 0;
    context->uv_layers_seen = 0;
    context->layer = 0;
}

int loader_fbx_slot_for(const loader_fbx_context* context, const char* name)
{
    if (context->in_geometry) {
        if (strcmp(name, "Vertices") == 0) {
            return fbx_slot_vertices;
        }
        if (strcmp(name, "PolygonVertexIndex") == 0) {
            return fbx_slot_polygon_index;
        }
    }
    if (context->layer == 1) {
        if (strcmp(name, "Normals") == 0) {
            return fbx_slot_normals;
        }
        if (strcmp(name, "NormalsIndex") == 0) {
            return fbx_slot_normal_index;
        }
    }
    if (context->layer == 2) {
        if (strcmp(name, "UV") == 0) {
            return fbx_slot_uvs;
        }
        if (strcmp(name, "UVIndex") == 0) {
            return fbx_slot_uv_index;
        }
    }
    return fbx_slot_none;
}

/* takes ownership of values */
void loader_fbx_store_array(loader_fbx_context* context, int slot, double* values, size_t count)
{
    double** target = NULL;
    size_t*  target_count = NULL;
    if (slot == fbx_slot_vertices) {
        target = &context->vertices;           target_count = &context->vertex_count;
    } else if (slot == fbx_slot_polygon_index) {
        target = &context->polygon_index;      target_count = &context->polygon_index_count;
    } else if (slot == fbx_slot_normals) {
        target = &context->normals;            target_count = &context->normal_count;
    } else if (slot == fbx_slot_normal_index) {
        target = &context->normal_index;       target_count = &context->normal_index_count;
    } else if (slot == fbx_slot_uvs) {
        target = &context->uvs;                target_count = &context->uv_count;
    } else if (slot == fbx_slot_uv_index) {
        target = &context->uv_index;           target_count = &context->uv_index_count;
    }
    if (!target) {
        free(values);
        return;
    }
    free(*target);
    *target = values;
    *target_count = count;
}

void loader_fbx_read_mapping(loader_fbx_context* context, const char* text, bool is_reference)
{
    if (is_reference) {
        int indexed = 0;
        if (strstr(text, "Index") != NULL) {
            indexed = 1;
        }
        if (context->layer == 1) {
            context->normal_indexed = indexed;
        } else if (context->layer == 2) {
            context->uv_indexed = indexed;
        }
        return;
    }
    int mapping = fbx_map_polygon_vertex;
    if (strstr(text, "ByPolygonVertex") != NULL) {
        mapping = fbx_map_polygon_vertex;
    } else if (strstr(text, "ByVertice") != NULL || strstr(text, "ByVertex") != NULL) {
        mapping = fbx_map_vertex;
    } else if (strstr(text, "AllSame") != NULL) {
        mapping = fbx_map_all_same;
    } else if (strstr(text, "ByPolygon") != NULL) {
        mapping = fbx_map_polygon;
    }
    if (context->layer == 1) {
        context->normal_mapping = mapping;
    } else if (context->layer == 2) {
        context->uv_mapping = mapping;
    }
}

bool loader_fbx_grow_output(loader_fbx_context* context, uint32_t extra_vertices, uint32_t extra_indices)
{
    if (context->out_vertex_count + extra_vertices > context->out_vertex_capacity) {
        uint32_t capacity = context->out_vertex_capacity;
        if (capacity < 1024) {
            capacity = 1024;
        }
        while (capacity < context->out_vertex_count + extra_vertices) {
            capacity *= 2;
        }
        loader_vertex* grown = (loader_vertex*)realloc(context->out_vertices, sizeof(loader_vertex) * capacity);
        if (!grown) {
            return false;
        }
        context->out_vertices = grown;
        context->out_vertex_capacity = capacity;
    }
    if (context->out_index_count + extra_indices > context->out_index_capacity) {
        uint32_t capacity = context->out_index_capacity;
        if (capacity < 3072) {
            capacity = 3072;
        }
        while (capacity < context->out_index_count + extra_indices) {
            capacity *= 2;
        }
        uint32_t* grown = (uint32_t*)realloc(context->out_indices, sizeof(uint32_t) * capacity);
        if (!grown) {
            return false;
        }
        context->out_indices = grown;
        context->out_index_capacity = capacity;
    }
    return true;
}

/* one layer element value for a polygon corner. key = what the mapping indexes by. */
bool loader_fbx_lookup(const double* data, size_t data_count, const double* index_data, size_t index_count,
                       int indexed, int mapping, size_t polygon_vertex, size_t vertex, size_t polygon,
                       int components, double* out)
{
    if (!data) {
        return false;
    }
    size_t key = 0;
    if (mapping == fbx_map_polygon_vertex) {
        key = polygon_vertex;
    } else if (mapping == fbx_map_vertex) {
        key = vertex;
    } else if (mapping == fbx_map_polygon) {
        key = polygon;
    }
    size_t element = key;
    if (indexed && index_data) {
        if (key >= index_count) {
            return false;
        }
        double raw = index_data[key];
        if (raw < 0.0) {
            return false;
        }
        element = (size_t)raw;
    }
    if ((element + 1) * (size_t)components > data_count) {
        return false;
    }
    for (int k = 0; k < components; k++) {
        out[k] = data[element * (size_t)components + (size_t)k];
    }
    return true;
}

/* turns the arrays of one Geometry into triangles */
void loader_fbx_emit_geometry(loader_fbx_context* context)
{
    if (!context->vertices || !context->polygon_index || context->vertex_count < 3) {
        loader_fbx_free_arrays(context);
        return;
    }

    double scale = context->unit_scale * 0.01;
    size_t count = context->polygon_index_count;
    size_t start = 0;
    size_t polygon_number = 0;

    while (start < count) {
        size_t end = start;
        while (end < count && context->polygon_index[end] >= 0.0) {
            end++;
        }
        if (end >= count) {
            break;
        }
        size_t corners = end - start + 1;
        if (corners >= 3) {
            if (!loader_fbx_grow_output(context, (uint32_t)corners, (uint32_t)((corners - 2) * 3))) {
                context->failed = true;
                break;
            }
            uint32_t first = context->out_vertex_count;
            for (size_t k = 0; k < corners; k++) {
                double raw = context->polygon_index[start + k];
                long vertex_index = (long)raw;
                if (vertex_index < 0) {
                    vertex_index = ~vertex_index;
                }
                loader_vertex* v = &context->out_vertices[context->out_vertex_count];
                memset(v, 0, sizeof(*v));
                if ((size_t)(vertex_index + 1) * 3 <= context->vertex_count) {
                    v->pos[0] = (float)(context->vertices[vertex_index * 3 + 0] * scale);
                    v->pos[1] = (float)(context->vertices[vertex_index * 3 + 1] * scale);
                    v->pos[2] = (float)(context->vertices[vertex_index * 3 + 2] * scale);
                }
                v->color[0] = 0.55f;
                v->color[1] = 0.55f;
                v->color[2] = 0.55f;
                v->color[3] = 1.0f;

                double value[3];
                if (loader_fbx_lookup(context->uvs, context->uv_count, context->uv_index, context->uv_index_count,
                                      context->uv_indexed, context->uv_mapping, start + k, (size_t)vertex_index, polygon_number, 2, value)) {
                    v->uv[0] = (float)value[0];
                    v->uv[1] = (float)value[1];
                }
                if (loader_fbx_lookup(context->normals, context->normal_count, context->normal_index, context->normal_index_count,
                                      context->normal_indexed, context->normal_mapping, start + k, (size_t)vertex_index, polygon_number, 3, value)) {
                    v->normal[0] = (float)value[0];
                    v->normal[1] = (float)value[1];
                    v->normal[2] = (float)value[2];
                } else {
                    v->normal[0] = 0.0f;
                    v->normal[1] = 0.0f;
                    v->normal[2] = 0.0f;
                }
                context->out_vertex_count++;
            }

            /* no normals in the file: use the face normal of the first triangle */
            loader_vertex* a = &context->out_vertices[first];
            loader_vertex* b = &context->out_vertices[first + 1];
            loader_vertex* c = &context->out_vertices[first + 2];
            float ux = b->pos[0] - a->pos[0], uy = b->pos[1] - a->pos[1], uz = b->pos[2] - a->pos[2];
            float vx = c->pos[0] - a->pos[0], vy = c->pos[1] - a->pos[1], vz = c->pos[2] - a->pos[2];
            float nx = uy * vz - uz * vy;
            float ny = uz * vx - ux * vz;
            float nz = ux * vy - uy * vx;
            float length = sqrtf(nx * nx + ny * ny + nz * nz);
            if (length > 1e-12f) {
                nx /= length;
                ny /= length;
                nz /= length;
            }
            for (size_t k = 0; k < corners; k++) {
                loader_vertex* v = &context->out_vertices[first + (uint32_t)k];
                if (v->normal[0] == 0.0f && v->normal[1] == 0.0f && v->normal[2] == 0.0f) {
                    v->normal[0] = nx;
                    v->normal[1] = ny;
                    v->normal[2] = nz;
                }
            }

            for (size_t k = 1; k + 1 < corners; k++) {
                context->out_indices[context->out_index_count++] = first;
                context->out_indices[context->out_index_count++] = first + (uint32_t)k;
                context->out_indices[context->out_index_count++] = first + (uint32_t)k + 1;
            }
        }
        polygon_number++;
        start = end + 1;
    }
    loader_fbx_free_arrays(context);
}

/* called when a node starts (after its properties) and ends */
void loader_fbx_node_begin(loader_fbx_context* context, const char* name)
{
    if (strcmp(name, "Geometry") == 0) {
        loader_fbx_free_arrays(context);
        context->in_geometry = true;
    } else if (context->in_geometry && strcmp(name, "LayerElementNormal") == 0) {
        context->normal_layers_seen++;
        if (context->normal_layers_seen == 1) {
            context->layer = 1;
        }
    } else if (context->in_geometry && strcmp(name, "LayerElementUV") == 0) {
        context->uv_layers_seen++;
        if (context->uv_layers_seen == 1) {
            context->layer = 2;
        }
    } else if (strcmp(name, "P") == 0 && strcmp(context->first_string, "UnitScaleFactor") == 0) {
        context->unit_scale = context->last_number;
    } else if (context->layer != 0 && strcmp(name, "MappingInformationType") == 0) {
        loader_fbx_read_mapping(context, context->first_string, false);
    } else if (context->layer != 0 && strcmp(name, "ReferenceInformationType") == 0) {
        loader_fbx_read_mapping(context, context->first_string, true);
    }
}

void loader_fbx_node_end(loader_fbx_context* context, const char* name)
{
    if (strcmp(name, "Geometry") == 0) {
        loader_fbx_emit_geometry(context);
        context->in_geometry = false;
    } else if (strcmp(name, "LayerElementNormal") == 0 || strcmp(name, "LayerElementUV") == 0) {
        context->layer = 0;
    }
}

/* ---- binary ---- */
double* loader_fbx_decode_array(char type, const uint8_t* raw, size_t length)
{
    double* values = (double*)malloc((length + 1) * sizeof(double));
    if (!values) {
        return NULL;
    }
    for (size_t i = 0; i < length; i++) {
        if (type == 'd') {
            double v;
            memcpy(&v, raw + i * 8, 8);
            values[i] = v;
        } else if (type == 'f') {
            float v;
            memcpy(&v, raw + i * 4, 4);
            values[i] = (double)v;
        } else if (type == 'i') {
            int32_t v = (int32_t)loader_read_le32(raw + i * 4);
            values[i] = (double)v;
        } else if (type == 'l') {
            int64_t v = (int64_t)loader_read_le64(raw + i * 8);
            values[i] = (double)v;
        } else {
            values[i] = (double)raw[i];
        }
    }
    return values;
}

bool loader_fbx_read_binary_properties(loader_fbx_context* context, const uint8_t* data, size_t size, size_t position,
                                       size_t properties_end, uint64_t count, const char* name)
{
    context->first_string[0] = '\0';
    context->last_number = 0.0;
    int slot = loader_fbx_slot_for(context, name);

    for (uint64_t index = 0; index < count; index++) {
        if (position >= properties_end) {
            return false;
        }
        char type = (char)data[position];
        position++;

        if (type == 'Y') {
            if (position + 2 > properties_end) {
                return false;
            }
            int16_t v = (int16_t)(data[position] | (data[position + 1] << 8));
            context->last_number = (double)v;
            position += 2;
        } else if (type == 'C') {
            position += 1;
        } else if (type == 'I' || type == 'F') {
            if (position + 4 > properties_end) {
                return false;
            }
            if (type == 'I') {
                context->last_number = (double)(int32_t)loader_read_le32(data + position);
            } else {
                float v;
                memcpy(&v, data + position, 4);
                context->last_number = (double)v;
            }
            position += 4;
        } else if (type == 'D' || type == 'L') {
            if (position + 8 > properties_end) {
                return false;
            }
            if (type == 'D') {
                double v;
                memcpy(&v, data + position, 8);
                context->last_number = v;
            } else {
                context->last_number = (double)(int64_t)loader_read_le64(data + position);
            }
            position += 8;
        } else if (type == 'S' || type == 'R') {
            if (position + 4 > properties_end) {
                return false;
            }
            uint32_t length = loader_read_le32(data + position);
            position += 4;
            if (position + length > properties_end) {
                return false;
            }
            if (type == 'S' && context->first_string[0] == '\0') {
                size_t copy = length;
                if (copy > sizeof(context->first_string) - 1) {
                    copy = sizeof(context->first_string) - 1;
                }
                memcpy(context->first_string, data + position, copy);
                context->first_string[copy] = '\0';
            }
            position += length;
        } else if (type == 'f' || type == 'd' || type == 'l' || type == 'i' || type == 'b') {
            if (position + 12 > properties_end) {
                return false;
            }
            uint32_t array_length = loader_read_le32(data + position);
            uint32_t encoding = loader_read_le32(data + position + 4);
            uint32_t compressed_length = loader_read_le32(data + position + 8);
            position += 12;
            if (position + compressed_length > properties_end) {
                return false;
            }
            size_t element_size = 1;
            if (type == 'd' || type == 'l') {
                element_size = 8;
            } else if (type == 'f' || type == 'i') {
                element_size = 4;
            }
            if (slot != fbx_slot_none) {
                size_t raw_length = (size_t)array_length * element_size;
                if (encoding == 0) {
                    if ((size_t)compressed_length < raw_length) {
                        return false;
                    }
                    double* values = loader_fbx_decode_array(type, data + position, array_length);
                    if (!values) {
                        return false;
                    }
                    loader_fbx_store_array(context, slot, values, array_length);
                } else {
                    uint8_t* raw = NULL;
                    size_t produced = 0;
                    if (!loader_zlib_decompress(data + position, compressed_length, &raw, &produced, raw_length) || produced < raw_length) {
                        free(raw);
                        return false;
                    }
                    double* values = loader_fbx_decode_array(type, raw, array_length);
                    free(raw);
                    if (!values) {
                        return false;
                    }
                    loader_fbx_store_array(context, slot, values, array_length);
                }
            }
            position += compressed_length;
        } else {
            return false;
        }
    }
    (void)size;
    return true;
}

bool loader_fbx_walk_binary(loader_fbx_context* context, const uint8_t* data, size_t size, size_t* position,
                            size_t end, uint32_t version, int depth)
{
    size_t header_size = 13;
    if (version >= 7500) {
        header_size = 25;
    }
    if (depth > 64) {
        return false;
    }

    while (*position + header_size <= end) {
        size_t p = *position;
        uint64_t end_offset = 0;
        uint64_t property_count = 0;
        uint64_t property_length = 0;
        size_t name_length = 0;
        if (version >= 7500) {
            end_offset = loader_read_le64(data + p);
            property_count = loader_read_le64(data + p + 8);
            property_length = loader_read_le64(data + p + 16);
            name_length = data[p + 24];
        } else {
            end_offset = loader_read_le32(data + p);
            property_count = loader_read_le32(data + p + 4);
            property_length = loader_read_le32(data + p + 8);
            name_length = data[p + 12];
        }
        p += header_size;

        if (end_offset == 0) {
            *position = p;
            return true;
        }
        if (end_offset > size || p + name_length > size) {
            return false;
        }

        char name[256];
        memcpy(name, data + p, name_length);
        name[name_length] = '\0';
        p += name_length;

        size_t properties_end = p + (size_t)property_length;
        if (properties_end > end_offset || properties_end > size) {
            return false;
        }
        if (!loader_fbx_read_binary_properties(context, data, size, p, properties_end, property_count, name)) {
            return false;
        }

        loader_fbx_node_begin(context, name);
        if (properties_end < end_offset) {
            size_t child_position = properties_end;
            if (!loader_fbx_walk_binary(context, data, size, &child_position, (size_t)end_offset, version, depth + 1)) {
                return false;
            }
        }
        loader_fbx_node_end(context, name);

        *position = (size_t)end_offset;
        if (context->failed) {
            return false;
        }
    }
    return true;
}

/* ---- ascii ---- */
void loader_fbx_skip_blanks(const char* text, size_t size, size_t* position)
{
    while (*position < size) {
        char c = text[*position];
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            (*position)++;
        } else if (c == ';') {
            while (*position < size && text[*position] != '\n') {
                (*position)++;
            }
        } else {
            break;
        }
    }
}

bool loader_fbx_walk_ascii(loader_fbx_context* context, const char* text, size_t size, size_t* position,
                           const char* parent_name, int depth)
{
    if (depth > 64) {
        return false;
    }

    for (;;) {
        loader_fbx_skip_blanks(text, size, position);
        if (*position >= size) {
            return true;
        }
        if (text[*position] == '}') {
            (*position)++;
            return true;
        }

        /* node name up to the colon */
        char name[128];
        size_t name_length = 0;
        while (*position < size && text[*position] != ':' && text[*position] != '\n' && text[*position] != '{' && text[*position] != '}') {
            if (name_length < sizeof(name) - 1) {
                name[name_length++] = text[*position];
            }
            (*position)++;
        }
        name[name_length] = '\0';
        if (*position >= size || text[*position] != ':') {
            if (*position < size && text[*position] == '}') {
                continue;
            }
            while (*position < size && text[*position] != '\n') {
                (*position)++;
            }
            continue;
        }
        (*position)++;

        if (strcmp(name, "a") == 0) {
            size_t capacity = 1024;
            size_t count = 0;
            double* values = (double*)malloc(capacity * sizeof(double));
            if (!values) {
                return false;
            }
            for (;;) {
                loader_fbx_skip_blanks(text, size, position);
                while (*position < size && text[*position] == ',') {
                    (*position)++;
                    loader_fbx_skip_blanks(text, size, position);
                }
                if (*position >= size || text[*position] == '}') {
                    break;
                }
                char* number_end = NULL;
                double value = strtod(text + *position, &number_end);
                if (number_end == text + *position) {
                    (*position)++;
                    continue;
                }
                *position = (size_t)(number_end - text);
                if (count == capacity) {
                    capacity *= 2;
                    double* grown = (double*)realloc(values, capacity * sizeof(double));
                    if (!grown) {
                        free(values);
                        return false;
                    }
                    values = grown;
                }
                values[count++] = value;
            }
            int slot = loader_fbx_slot_for(context, parent_name);
            loader_fbx_store_array(context, slot, values, count);
            continue;
        }

        /* the rest of the line: first quoted string, last number, optional opening brace */
        context->first_string[0] = '\0';
        context->last_number = 0.0;
        bool has_children = false;
        while (*position < size && text[*position] != '\n') {
            char c = text[*position];
            if (c == '"') {
                size_t begin = ++(*position);
                while (*position < size && text[*position] != '"' && text[*position] != '\n') {
                    (*position)++;
                }
                if (context->first_string[0] == '\0') {
                    size_t copy = *position - begin;
                    if (copy > sizeof(context->first_string) - 1) {
                        copy = sizeof(context->first_string) - 1;
                    }
                    memcpy(context->first_string, text + begin, copy);
                    context->first_string[copy] = '\0';
                }
                if (*position < size && text[*position] == '"') {
                    (*position)++;
                }
            } else if (c == '{') {
                has_children = true;
                (*position)++;
                break;
            } else if (c == ';') {
                while (*position < size && text[*position] != '\n') {
                    (*position)++;
                }
            } else if ((c >= '0' && c <= '9') || c == '-' || c == '+' || c == '.') {
                char* number_end = NULL;
                double value = strtod(text + *position, &number_end);
                if (number_end == text + *position) {
                    (*position)++;
                } else {
                    context->last_number = value;
                    *position = (size_t)(number_end - text);
                }
            } else {
                (*position)++;
            }
        }

        loader_fbx_node_begin(context, name);
        if (has_children) {
            if (!loader_fbx_walk_ascii(context, text, size, position, name, depth + 1)) {
                return false;
            }
        }
        loader_fbx_node_end(context, name);
        if (context->failed) {
            return false;
        }
    }
}

bool loader_load_fbx_file(const char* filepath, loader_mesh_data* out_mesh)
{
    size_t size = 0;
    uint8_t* data = loader_read_file(filepath, &size);
    if (!data) {
        return false;
    }

    loader_fbx_context context;
    memset(&context, 0, sizeof(context));
    context.unit_scale = 1.0;

    bool ok = false;
    const char binary_magic[] = "Kaydara FBX Binary  ";
    if (size > 27 && memcmp(data, binary_magic, 20) == 0) {
        uint32_t version = loader_read_le32(data + 23);
        size_t position = 27;
        ok = loader_fbx_walk_binary(&context, data, size, &position, size, version, 0);
    } else {
        size_t position = 0;
        ok = loader_fbx_walk_ascii(&context, (const char*)data, size, &position, "", 0);
    }
    free(data);
    loader_fbx_free_arrays(&context);

    if (!ok || context.failed || context.out_vertex_count == 0 || context.out_index_count == 0) {
        free(context.out_vertices);
        free(context.out_indices);
        return false;
    }

    out_mesh->vertices = context.out_vertices;
    out_mesh->indices = context.out_indices;
    out_mesh->vertex_count = context.out_vertex_count;
    out_mesh->index_count = context.out_index_count;
    return true;
}

/* the first loader's own raw mesh format: counts, then 28 byte vertices (pos + color), then indices */
bool loader_load_raw_mesh_file(const char* filepath, loader_mesh_data* out_mesh)
{
    FILE* file = fopen(filepath, "rb");
    if (!file) {
        return false;
    }

    uint32_t vertex_count = 0;
    uint32_t index_count = 0;
    if (fread(&vertex_count, sizeof(uint32_t), 1, file) != 1 || fread(&index_count, sizeof(uint32_t), 1, file) != 1) {
        fclose(file);
        return false;
    }
    if (vertex_count == 0 || vertex_count > 16000000u || index_count > 64000000u) {
        fclose(file);
        return false;
    }

    float (*raw)[7] = (float (*)[7])malloc(sizeof(float) * 7 * vertex_count);
    loader_vertex* vertices = (loader_vertex*)calloc(vertex_count, sizeof(loader_vertex));
    uint32_t* indices = (uint32_t*)malloc(sizeof(uint32_t) * (index_count + 1));
    if (!raw || !vertices || !indices ||
        fread(raw, sizeof(float) * 7, vertex_count, file) != vertex_count ||
        fread(indices, sizeof(uint32_t), index_count, file) != index_count) {
        free(raw);
        free(vertices);
        free(indices);
        fclose(file);
        return false;
    }
    fclose(file);

    for (uint32_t i = 0; i < vertex_count; i++) {
        for (int k = 0; k < 3; k++) {
            vertices[i].pos[k] = raw[i][k];
        }
        for (int k = 0; k < 4; k++) {
            vertices[i].color[k] = raw[i][3 + k];
        }
        vertices[i].normal[1] = 1.0f;
    }
    free(raw);

    out_mesh->vertices = vertices;
    out_mesh->indices = indices;
    out_mesh->vertex_count = vertex_count;
    out_mesh->index_count = index_count;
    return true;
}

PHOENIX_LOADER_API bool load_3d_mesh_asset(const char* filepath, loader_mesh_data* out_mesh)
{
    if (!filepath || !out_mesh) {
        return false;
    }
    memset(out_mesh, 0, sizeof(*out_mesh));
    if (loader_has_extension(filepath, ".fbx")) {
        return loader_load_fbx_file(filepath, out_mesh);
    }
    return loader_load_raw_mesh_file(filepath, out_mesh);
}

PHOENIX_LOADER_API void remove_3d_mesh_asset(loader_mesh_data* mesh)
{
    if (mesh) {
        if (mesh->vertices) {
            free(mesh->vertices);
        }
        if (mesh->indices) {
            free(mesh->indices);
        }
        mesh->vertices = NULL;
        mesh->indices = NULL;
        mesh->vertex_count = 0;
        mesh->index_count = 0;
    }
}

/* ------------------------------------------------------------------------- */
/* fbx saving: one Geometry + one Model, in meters, triangles                  */
/*   corners carry their own normal and uv (ByPolygonVertex, Direct).          */
/* ------------------------------------------------------------------------- */
void loader_fbx_text_numbers(loader_buffer* buffer, const double* values, size_t count, bool as_integers)
{
    for (size_t i = 0; i < count; i++) {
        if (as_integers) {
            loader_buffer_text(buffer, "%ld", (long)values[i]);
        } else {
            loader_buffer_text(buffer, "%.9g", values[i]);
        }
        if (i + 1 < count) {
            loader_buffer_text(buffer, ",");
            if ((i % 12) == 11) {
                loader_buffer_text(buffer, "\n\t\t\t\t");
            }
        }
    }
}

/* flat arrays shared by the ascii and the binary writer */
typedef struct {
    double* positions;      size_t position_count;
    double* polygon_index;  size_t polygon_index_count;
    double* normals;        size_t normal_count;
    double* uvs;            size_t uv_count;
} loader_fbx_export;

bool loader_fbx_build_export(const loader_mesh_data* mesh, loader_fbx_export* out)
{
    memset(out, 0, sizeof(*out));
    if (!mesh || !mesh->vertices || !mesh->indices || mesh->vertex_count == 0 || mesh->index_count < 3) {
        return false;
    }
    size_t triangles = mesh->index_count / 3;
    out->position_count = (size_t)mesh->vertex_count * 3;
    out->polygon_index_count = triangles * 3;
    out->normal_count = triangles * 3 * 3;
    out->uv_count = triangles * 3 * 2;
    out->positions = (double*)malloc(out->position_count * sizeof(double));
    out->polygon_index = (double*)malloc(out->polygon_index_count * sizeof(double));
    out->normals = (double*)malloc(out->normal_count * sizeof(double));
    out->uvs = (double*)malloc(out->uv_count * sizeof(double));
    if (!out->positions || !out->polygon_index || !out->normals || !out->uvs) {
        free(out->positions);
        free(out->polygon_index);
        free(out->normals);
        free(out->uvs);
        memset(out, 0, sizeof(*out));
        return false;
    }

    for (uint32_t i = 0; i < mesh->vertex_count; i++) {
        out->positions[i * 3 + 0] = mesh->vertices[i].pos[0];
        out->positions[i * 3 + 1] = mesh->vertices[i].pos[1];
        out->positions[i * 3 + 2] = mesh->vertices[i].pos[2];
    }
    for (size_t t = 0; t < triangles; t++) {
        for (size_t k = 0; k < 3; k++) {
            uint32_t index = mesh->indices[t * 3 + k];
            if (index >= mesh->vertex_count) {
                index = 0;
            }
            /* the last index of every polygon is stored as its bitwise not */
            if (k == 2) {
                out->polygon_index[t * 3 + k] = (double)(-(long)index - 1);
            } else {
                out->polygon_index[t * 3 + k] = (double)index;
            }
            const loader_vertex* v = &mesh->vertices[index];
            out->normals[(t * 3 + k) * 3 + 0] = v->normal[0];
            out->normals[(t * 3 + k) * 3 + 1] = v->normal[1];
            out->normals[(t * 3 + k) * 3 + 2] = v->normal[2];
            out->uvs[(t * 3 + k) * 2 + 0] = v->uv[0];
            out->uvs[(t * 3 + k) * 2 + 1] = v->uv[1];
        }
    }
    return true;
}

void loader_fbx_free_export(loader_fbx_export* data)
{
    free(data->positions);
    free(data->polygon_index);
    free(data->normals);
    free(data->uvs);
    memset(data, 0, sizeof(*data));
}

void loader_fbx_write_ascii_array(loader_buffer* buffer, const char* name, const double* values, size_t count, bool as_integers)
{
    loader_buffer_text(buffer, "\t\t%s: *%lu {\n\t\t\ta: ", name, (unsigned long)count);
    loader_fbx_text_numbers(buffer, values, count, as_integers);
    loader_buffer_text(buffer, "\n\t\t}\n");
}

bool loader_save_fbx_ascii(const char* filepath, const loader_fbx_export* data)
{
    loader_buffer buffer;
    memset(&buffer, 0, sizeof(buffer));

    loader_buffer_text(&buffer, "; FBX 7.4.0 project file\n; written by the phoenix loader\n\n");
    loader_buffer_text(&buffer, "FBXHeaderExtension:  {\n\tFBXHeaderVersion: 1003\n\tFBXVersion: 7400\n\tCreator: \"Phoenix Engine\"\n}\n");
    loader_buffer_text(&buffer, "GlobalSettings:  {\n\tVersion: 1000\n\tProperties70:  {\n");
    loader_buffer_text(&buffer, "\t\tP: \"UpAxis\", \"int\", \"Integer\", \"\",1\n");
    loader_buffer_text(&buffer, "\t\tP: \"UpAxisSign\", \"int\", \"Integer\", \"\",1\n");
    loader_buffer_text(&buffer, "\t\tP: \"FrontAxis\", \"int\", \"Integer\", \"\",2\n");
    loader_buffer_text(&buffer, "\t\tP: \"FrontAxisSign\", \"int\", \"Integer\", \"\",1\n");
    loader_buffer_text(&buffer, "\t\tP: \"CoordAxis\", \"int\", \"Integer\", \"\",0\n");
    loader_buffer_text(&buffer, "\t\tP: \"CoordAxisSign\", \"int\", \"Integer\", \"\",1\n");
    loader_buffer_text(&buffer, "\t\tP: \"UnitScaleFactor\", \"double\", \"Number\", \"\",100\n");
    loader_buffer_text(&buffer, "\t}\n}\n\n");

    loader_buffer_text(&buffer, "Objects:  {\n\tGeometry: 1000, \"Geometry::mesh\", \"Mesh\" {\n");
    loader_fbx_write_ascii_array(&buffer, "Vertices", data->positions, data->position_count, false);
    loader_fbx_write_ascii_array(&buffer, "PolygonVertexIndex", data->polygon_index, data->polygon_index_count, true);
    loader_buffer_text(&buffer, "\t\tGeometryVersion: 124\n");
    loader_buffer_text(&buffer, "\t\tLayerElementNormal: 0 {\n\t\t\tVersion: 101\n\t\t\tName: \"\"\n\t\t\tMappingInformationType: \"ByPolygonVertex\"\n\t\t\tReferenceInformationType: \"Direct\"\n");
    loader_fbx_write_ascii_array(&buffer, "\tNormals", data->normals, data->normal_count, false);
    loader_buffer_text(&buffer, "\t\t}\n");
    loader_buffer_text(&buffer, "\t\tLayerElementUV: 0 {\n\t\t\tVersion: 101\n\t\t\tName: \"UVMap\"\n\t\t\tMappingInformationType: \"ByPolygonVertex\"\n\t\t\tReferenceInformationType: \"Direct\"\n");
    loader_fbx_write_ascii_array(&buffer, "\tUV", data->uvs, data->uv_count, false);
    loader_buffer_text(&buffer, "\t\t}\n");
    loader_buffer_text(&buffer, "\t\tLayer: 0 {\n\t\t\tVersion: 100\n\t\t\tLayerElement:  {\n\t\t\t\tType: \"LayerElementNormal\"\n\t\t\t\tTypedIndex: 0\n\t\t\t}\n");
    loader_buffer_text(&buffer, "\t\t\tLayerElement:  {\n\t\t\t\tType: \"LayerElementUV\"\n\t\t\t\tTypedIndex: 0\n\t\t\t}\n\t\t}\n");
    loader_buffer_text(&buffer, "\t}\n");
    loader_buffer_text(&buffer, "\tModel: 2000, \"Model::mesh\", \"Mesh\" {\n\t\tVersion: 232\n\t\tProperties70:  {\n\t\t}\n\t\tShading: T\n\t\tCulling: \"CullingOff\"\n\t}\n}\n\n");
    loader_buffer_text(&buffer, "Connections:  {\n\tC: \"OO\",2000,0\n\tC: \"OO\",1000,2000\n}\n");

    bool ok = !buffer.failed && loader_write_file(filepath, buffer.data, buffer.size);
    free(buffer.data);
    return ok;
}

/* ---- binary writer (version 7400: 32 bit node headers, arrays stored raw) ---- */
size_t loader_bin_begin(loader_buffer* buffer, const char* name, uint32_t property_count)
{
    size_t start = buffer->size;
    uint32_t placeholder = 0;
    uint8_t name_length = (uint8_t)strlen(name);
    loader_buffer_append(buffer, &placeholder, 4);
    loader_buffer_append(buffer, &property_count, 4);
    loader_buffer_append(buffer, &placeholder, 4);
    loader_buffer_append(buffer, &name_length, 1);
    loader_buffer_append(buffer, name, name_length);
    return start;
}

/* after the last property is written */
void loader_bin_properties_done(loader_buffer* buffer, size_t start)
{
    if (buffer->failed) {
        return;
    }
    uint8_t name_length = buffer->data[start + 12];
    uint32_t property_length = (uint32_t)(buffer->size - (start + 13 + name_length));
    memcpy(buffer->data + start + 8, &property_length, 4);
}

void loader_bin_end(loader_buffer* buffer, size_t start, bool has_children)
{
    if (has_children) {
        loader_buffer_append(buffer, NULL, 13);
    }
    if (buffer->failed) {
        return;
    }
    uint32_t end_offset = (uint32_t)buffer->size;
    memcpy(buffer->data + start, &end_offset, 4);
}

void loader_bin_int(loader_buffer* buffer, int32_t value)
{
    uint8_t type = 'I';
    loader_buffer_append(buffer, &type, 1);
    loader_buffer_append(buffer, &value, 4);
}

void loader_bin_long(loader_buffer* buffer, int64_t value)
{
    uint8_t type = 'L';
    loader_buffer_append(buffer, &type, 1);
    loader_buffer_append(buffer, &value, 8);
}

void loader_bin_double(loader_buffer* buffer, double value)
{
    uint8_t type = 'D';
    loader_buffer_append(buffer, &type, 1);
    loader_buffer_append(buffer, &value, 8);
}

void loader_bin_string(loader_buffer* buffer, const char* text)
{
    uint8_t type = 'S';
    uint32_t length = (uint32_t)strlen(text);
    loader_buffer_append(buffer, &type, 1);
    loader_buffer_append(buffer, &length, 4);
    loader_buffer_append(buffer, text, length);
}

void loader_bin_double_array(loader_buffer* buffer, const double* values, size_t count)
{
    uint8_t type = 'd';
    uint32_t length = (uint32_t)count;
    uint32_t encoding = 0;
    uint32_t byte_length = (uint32_t)(count * 8);
    loader_buffer_append(buffer, &type, 1);
    loader_buffer_append(buffer, &length, 4);
    loader_buffer_append(buffer, &encoding, 4);
    loader_buffer_append(buffer, &byte_length, 4);
    loader_buffer_append(buffer, values, count * 8);
}

void loader_bin_int_array(loader_buffer* buffer, const double* values, size_t count)
{
    uint8_t type = 'i';
    uint32_t length = (uint32_t)count;
    uint32_t encoding = 0;
    uint32_t byte_length = (uint32_t)(count * 4);
    loader_buffer_append(buffer, &type, 1);
    loader_buffer_append(buffer, &length, 4);
    loader_buffer_append(buffer, &encoding, 4);
    loader_buffer_append(buffer, &byte_length, 4);
    for (size_t i = 0; i < count; i++) {
        int32_t value = (int32_t)values[i];
        loader_buffer_append(buffer, &value, 4);
    }
}

/* a leaf node with one int / string / array property */
void loader_bin_leaf_int(loader_buffer* buffer, const char* name, int32_t value)
{
    size_t node = loader_bin_begin(buffer, name, 1);
    loader_bin_int(buffer, value);
    loader_bin_properties_done(buffer, node);
    loader_bin_end(buffer, node, false);
}

void loader_bin_leaf_string(loader_buffer* buffer, const char* name, const char* value)
{
    size_t node = loader_bin_begin(buffer, name, 1);
    loader_bin_string(buffer, value);
    loader_bin_properties_done(buffer, node);
    loader_bin_end(buffer, node, false);
}

void loader_bin_leaf_doubles(loader_buffer* buffer, const char* name, const double* values, size_t count)
{
    size_t node = loader_bin_begin(buffer, name, 1);
    loader_bin_double_array(buffer, values, count);
    loader_bin_properties_done(buffer, node);
    loader_bin_end(buffer, node, false);
}

void loader_bin_layer_element(loader_buffer* buffer, const char* node_name, const char* label, const char* array_name,
                              const double* values, size_t count)
{
    size_t node = loader_bin_begin(buffer, node_name, 1);
    loader_bin_int(buffer, 0);
    loader_bin_properties_done(buffer, node);
    loader_bin_leaf_int(buffer, "Version", 101);
    loader_bin_leaf_string(buffer, "Name", label);
    loader_bin_leaf_string(buffer, "MappingInformationType", "ByPolygonVertex");
    loader_bin_leaf_string(buffer, "ReferenceInformationType", "Direct");
    loader_bin_leaf_doubles(buffer, array_name, values, count);
    loader_bin_end(buffer, node, true);
}

bool loader_save_fbx_binary(const char* filepath, const loader_fbx_export* data)
{
    loader_buffer buffer;
    memset(&buffer, 0, sizeof(buffer));

    const char magic[] = "Kaydara FBX Binary  ";
    uint32_t version = 7400;
    loader_buffer_append(&buffer, magic, 21);                /* 20 characters plus the terminating zero */
    uint8_t marker[2] = { 0x1A, 0x00 };
    loader_buffer_append(&buffer, marker, 2);
    loader_buffer_append(&buffer, &version, 4);

    size_t header = loader_bin_begin(&buffer, "FBXHeaderExtension", 0);
    loader_bin_properties_done(&buffer, header);
    loader_bin_leaf_int(&buffer, "FBXHeaderVersion", 1003);
    loader_bin_leaf_int(&buffer, "FBXVersion", 7400);
    loader_bin_leaf_string(&buffer, "Creator", "Phoenix Engine");
    loader_bin_end(&buffer, header, true);

    size_t settings = loader_bin_begin(&buffer, "GlobalSettings", 0);
    loader_bin_properties_done(&buffer, settings);
    loader_bin_leaf_int(&buffer, "Version", 1000);
    size_t properties = loader_bin_begin(&buffer, "Properties70", 0);
    loader_bin_properties_done(&buffer, properties);
    size_t unit = loader_bin_begin(&buffer, "P", 5);
    loader_bin_string(&buffer, "UnitScaleFactor");
    loader_bin_string(&buffer, "double");
    loader_bin_string(&buffer, "Number");
    loader_bin_string(&buffer, "");
    loader_bin_double(&buffer, 100.0);
    loader_bin_properties_done(&buffer, unit);
    loader_bin_end(&buffer, unit, false);
    loader_bin_end(&buffer, properties, true);
    loader_bin_end(&buffer, settings, true);

    size_t objects = loader_bin_begin(&buffer, "Objects", 0);
    loader_bin_properties_done(&buffer, objects);

    size_t geometry = loader_bin_begin(&buffer, "Geometry", 3);
    loader_bin_long(&buffer, 1000);
    loader_bin_string(&buffer, "mesh");
    loader_bin_string(&buffer, "Mesh");
    loader_bin_properties_done(&buffer, geometry);

    size_t vertices = loader_bin_begin(&buffer, "Vertices", 1);
    loader_bin_double_array(&buffer, data->positions, data->position_count);
    loader_bin_properties_done(&buffer, vertices);
    loader_bin_end(&buffer, vertices, false);

    size_t polygons = loader_bin_begin(&buffer, "PolygonVertexIndex", 1);
    loader_bin_int_array(&buffer, data->polygon_index, data->polygon_index_count);
    loader_bin_properties_done(&buffer, polygons);
    loader_bin_end(&buffer, polygons, false);

    loader_bin_leaf_int(&buffer, "GeometryVersion", 124);
    loader_bin_layer_element(&buffer, "LayerElementNormal", "", "Normals", data->normals, data->normal_count);
    loader_bin_layer_element(&buffer, "LayerElementUV", "UVMap", "UV", data->uvs, data->uv_count);
    loader_bin_end(&buffer, geometry, true);

    size_t model = loader_bin_begin(&buffer, "Model", 3);
    loader_bin_long(&buffer, 2000);
    loader_bin_string(&buffer, "mesh");
    loader_bin_string(&buffer, "Mesh");
    loader_bin_properties_done(&buffer, model);
    loader_bin_leaf_int(&buffer, "Version", 232);
    loader_bin_end(&buffer, model, true);

    loader_bin_end(&buffer, objects, true);

    size_t connections = loader_bin_begin(&buffer, "Connections", 0);
    loader_bin_properties_done(&buffer, connections);
    size_t link_model = loader_bin_begin(&buffer, "C", 3);
    loader_bin_string(&buffer, "OO");
    loader_bin_long(&buffer, 2000);
    loader_bin_long(&buffer, 0);
    loader_bin_properties_done(&buffer, link_model);
    loader_bin_end(&buffer, link_model, false);
    size_t link_geometry = loader_bin_begin(&buffer, "C", 3);
    loader_bin_string(&buffer, "OO");
    loader_bin_long(&buffer, 1000);
    loader_bin_long(&buffer, 2000);
    loader_bin_properties_done(&buffer, link_geometry);
    loader_bin_end(&buffer, link_geometry, false);
    loader_bin_end(&buffer, connections, true);

    loader_buffer_append(&buffer, NULL, 13);      /* the null record that closes the top level */

    bool ok = !buffer.failed && loader_write_file(filepath, buffer.data, buffer.size);
    free(buffer.data);
    return ok;
}

PHOENIX_LOADER_API bool save_3d_mesh_asset(const char* filepath, const loader_mesh_data* mesh, bool binary)
{
    if (!filepath) {
        return false;
    }
    loader_fbx_export data;
    if (!loader_fbx_build_export(mesh, &data)) {
        return false;
    }
    bool ok = false;
    if (binary) {
        ok = loader_save_fbx_binary(filepath, &data);
    } else {
        ok = loader_save_fbx_ascii(filepath, &data);
    }
    loader_fbx_free_export(&data);
    return ok;
}
