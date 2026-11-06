#ifndef PHOENIX_ENGINE_DYNAMIC_ARRAY_H
#define PHOENIX_ENGINE_DYNAMIC_ARRAY_H



#include "engine_defines.h"


/*
RULES THAT MUST BE FOLLOWED.
uncomment as we start getting things fixed: 
Must use lowercase seperated by underscores. Even structs names, enum names, etc.
Must use verbs before nouns when naming anything
Must not use ternerys. i fucking hate ternery operators.
Must use braced control-flow headers: every if, else if, and else must use braces.
Must call extern in engine_globals.h and init in engine_globals.c
Must not have statics in any file. I fucking hate statics.
No static structs, No static enums, No static anything.
Math functions must go in math file and header.
Drawing stuff must go into drawing stuff and headers.
ALl information gets routed into statusbar.

NO CAPITAL LETTERS UNLESS WINDOWS OR VULKAN DEMANDS IT.
DO NOT REMOVE THIS COMMENT EVER. THESE ARE THE RULES.
IF SOMETHING IS BREAKING THESE RULES CHANGE IT THEN WE CHANGE EVERYTHING CONNECTED AS WE GO.
THIS INCLUDES INSIDE THE SOURCE FILES NOT JUST HEADERS
*/


enum {
    PARRY_CAPACITY,
    PARRY_LENGTH,
    PARRY_STRIDE,
    PARRY_FIELD_LENGTH
};

phoenix_engine_api void* create_parry(unsigned long long length, unsigned long long stride);
phoenix_engine_api void destroy_parry(void* array);

phoenix_engine_api unsigned long long get_parry_field(void* array, unsigned long long field);
phoenix_engine_api void set_parry_field(void* array, unsigned long long field, unsigned long long value);

phoenix_engine_api void* resize_parry(void* array);

phoenix_engine_api void* push_parry(void* array, const void* value_ptr);
phoenix_engine_api void pop_parry(void* array, void* dest);

phoenix_engine_api void* pop_at_parry(void* array, unsigned long long index, void* dest);
phoenix_engine_api void* insert_at_parry(void* array, unsigned long long index, void* value_ptr);

#define PARRY_DEFAULT_CAPACITY 1
#define PARRY_RESIZE_FACTOR 2

#define parry_create(type) \
    create_parry(PARRY_DEFAULT_CAPACITY, sizeof(type))

#define parry_reserve(type, capacity) \
    create_parry(capacity, sizeof(type))

#define parry_destroy(array) destroy_parry(array);

// FIXED: Converted GNU extension typeof() over to standard ISO compliant __typeof__ tracking
#define parry_push(array, value)           \
    {                                       \
        __typeof__(value) temp = value;     \
        array = push_parry(array, &temp);  \
    }

#define parry_pop(array, value_ptr) \
    pop_parry(array, value_ptr)

// FIXED: Converted GNU extension typeof() over to standard ISO compliant __typeof__ tracking
#define parry_insert_at(array, index, value)           \
    {                                                   \
        __typeof__(value) temp = value;                 \
        array = insert_at_parry(array, index, &temp);   \
    }

#define parry_pop_at(array, index, value_ptr) \
    pop_at_parry(array, index, value_ptr)

#define parry_clear(array) \
    set_parry_field(array, PARRY_LENGTH, 0)

#define parry_capacity(array) \
    get_parry_field(array, PARRY_CAPACITY)

#define parry_length(array) \
    get_parry_field(array, PARRY_LENGTH)

#define parry_stride(array) \
    get_parry_field(array, PARRY_STRIDE)

#define parry_length_set(array, value) \
    set_parry_field(array, PARRY_LENGTH, value)

    
#endif