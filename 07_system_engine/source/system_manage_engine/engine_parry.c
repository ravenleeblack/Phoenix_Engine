#include "engine_globals.h"

void* create_parry(unsigned long long length, unsigned long long stride) {
    unsigned long long header_size = PARRY_FIELD_LENGTH * sizeof(unsigned long long);
    unsigned long long array_size = length * stride;
    unsigned long long* new_array = kallocate(header_size + array_size, MEMORY_TAG_DARRAY);
    kset_memory(new_array, 0, header_size + array_size);
    new_array[PARRY_CAPACITY] = length;
    new_array[PARRY_LENGTH] = 0;
    new_array[PARRY_STRIDE] = stride;
    return (void*)(new_array + PARRY_FIELD_LENGTH);
}

void destroy_parry(void* array) {
    unsigned long long* header = (unsigned long long*)array - PARRY_FIELD_LENGTH;
    unsigned long long header_size = PARRY_FIELD_LENGTH * sizeof(unsigned long long);
    unsigned long long total_size = header_size + header[PARRY_CAPACITY] * header[PARRY_STRIDE];
    kfree(header, total_size, MEMORY_TAG_DARRAY);
}

unsigned long long get_parry_field(void* array, unsigned long long field) {
    unsigned long long* header = (unsigned long long*)array - PARRY_FIELD_LENGTH;
    return header[field];
}

void set_parry_field(void* array, unsigned long long field, unsigned long long value) {
    unsigned long long* header = (unsigned long long*)array - PARRY_FIELD_LENGTH;
    header[field] = value;
}

void* resize_parry(void* array) {
    unsigned long long length = parry_length(array);
    unsigned long long stride = parry_stride(array);
    void* temp = create_parry(
        (PARRY_RESIZE_FACTOR * parry_capacity(array)),
        stride);
    kcopy_memory(temp, array, length * stride);

    set_parry_field(temp, PARRY_LENGTH, length);
    destroy_parry(array);
    return temp;
}

void* push_parry(void* array, const void* value_ptr) {
    unsigned long long length = parry_length(array);
    unsigned long long stride = parry_stride(array);
    if (length >= parry_capacity(array)) {
        array = resize_parry(array);
    }

    unsigned long long addr = (unsigned long long)array;
    addr += (length * stride);
    kcopy_memory((void*)addr, value_ptr, stride);
    set_parry_field(array, PARRY_LENGTH, length + 1);
    return array;
}

void pop_parry(void* array, void* dest) {
    unsigned long long length = parry_length(array);
    unsigned long long stride = parry_stride(array);
    unsigned long long addr = (unsigned long long)array;
    addr += ((length - 1) * stride);
    kcopy_memory(dest, (void*)addr, stride);
    set_parry_field(array, PARRY_LENGTH, length - 1);
}

void* pop_at_parry(void* array, unsigned long long index, void* dest) {
    unsigned long long length = parry_length(array);
    unsigned long long stride = parry_stride(array);
    if (index >= length) {
        phoenix_error("PARRY", "Index outside the bounds of this array! Length: %llu, index: %llu", length, index);
        return array;
    }

    unsigned long long addr = (unsigned long long)array;
    kcopy_memory(dest, (void*)(addr + (index * stride)), stride);

    if (index != length - 1) {
        kcopy_memory(
            (void*)(addr + (index * stride)),
            (void*)(addr + ((index + 1) * stride)),
            stride * (length - index));
    }

    set_parry_field(array, PARRY_LENGTH, length - 1);
    return array;
}

void* insert_at_parry(void* array, unsigned long long index, void* value_ptr) {
    unsigned long long length = parry_length(array);
    unsigned long long stride = parry_stride(array);
    if (index >= length) {
        phoenix_error("PARRY", "Index outside the bounds of this array! Length: %llu, index: %llu", length, index);
        return array;
    }
    if (length >= parry_capacity(array)) {
        array = resize_parry(array);
    }

    unsigned long long addr = (unsigned long long)array;

    if (index != length - 1) {
        kcopy_memory(
            (void*)(addr + ((index + 1) * stride)),
            (void*)(addr + (index * stride)),
            stride * (length - index));
    }

    kcopy_memory((void*)(addr + (index * stride)), value_ptr, stride);

    set_parry_field(array, PARRY_LENGTH, length + 1);
    return array;
}
