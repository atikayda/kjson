/*
 * kjson_binary.c - kJSON binary format (kJSONB) implementation
 * 
 * Encodes and decodes kjson_value trees to/from binary format.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include <limits.h>

#include "kjson.h"

/* Binary buffer for encoding */
typedef struct {
    uint8_t *data;
    size_t length;
    size_t capacity;
    void *(*alloc)(size_t);
    void (*dealloc)(void *);
} binary_buffer;

/* Decoding state */
typedef struct {
    const uint8_t *data;
    size_t length;
    size_t position;
    kjson_error error;
    void *(*alloc)(size_t);
    void (*dealloc)(void *);
} decode_state;

/* Forward declarations */
static bool encode_value(binary_buffer *buf, const kjson_value *value);
static kjson_value *decode_value(decode_state *state);
static bool buffer_write_varint(binary_buffer *buf, uint64_t value);
static bool decode_varint(decode_state *state, uint64_t *value);

/* Initialize binary buffer */
static bool buffer_init(binary_buffer *buf, size_t initial_capacity,
                       void *(*alloc)(size_t), void (*dealloc)(void *))
{
    buf->capacity = initial_capacity > 0 ? initial_capacity : 256;
    buf->alloc = alloc ? alloc : malloc;
    buf->dealloc = dealloc ? dealloc : free;
    buf->data = buf->alloc(buf->capacity);
    if (!buf->data) return false;
    
    buf->length = 0;
    return true;
}

/* Grow buffer if needed */
static bool buffer_grow(binary_buffer *buf, size_t min_additional)
{
    size_t required = buf->length + min_additional;
    if (required <= buf->capacity) return true;
    
    size_t new_capacity = buf->capacity * 2;
    while (new_capacity < required) {
        new_capacity *= 2;
    }
    
    uint8_t *new_data = buf->alloc(new_capacity);
    if (!new_data) return false;
    
    memcpy(new_data, buf->data, buf->length);
    buf->dealloc(buf->data);
    buf->data = new_data;
    buf->capacity = new_capacity;
    
    return true;
}

/* Write byte to buffer */
static bool buffer_write_byte(binary_buffer *buf, uint8_t byte)
{
    if (!buffer_grow(buf, 1)) return false;
    buf->data[buf->length++] = byte;
    return true;
}

/* Write bytes to buffer */
static bool buffer_write_bytes(binary_buffer *buf, const void *data, size_t len)
{
    if (!buffer_grow(buf, len)) return false;
    memcpy(buf->data + buf->length, data, len);
    buf->length += len;
    return true;
}

/* Write variable-length integer */
static bool buffer_write_varint(binary_buffer *buf, uint64_t value)
{
    while (value >= 0x80) {
        if (!buffer_write_byte(buf, (value & 0x7F) | 0x80)) return false;
        value >>= 7;
    }
    return buffer_write_byte(buf, value & 0x7F);
}

/* Write signed varint using zigzag encoding */
static bool buffer_write_signed_varint(binary_buffer *buf, int64_t value)
{
    /* Zigzag encoding: positive -> 2n, negative -> 2n+1 */
    uint64_t encoded = (value < 0) ? (((uint64_t)(-(value + 1)) << 1) | 1) : ((uint64_t)value << 1);
    return buffer_write_varint(buf, encoded);
}

/* Write 32-bit float */
static bool buffer_write_float32(binary_buffer *buf, float value)
{
    return buffer_write_bytes(buf, &value, sizeof(float));
}

/* Write 64-bit float */
static bool buffer_write_float64(binary_buffer *buf, double value)
{
    return buffer_write_bytes(buf, &value, sizeof(double));
}

/* Encode string */
static bool encode_string(binary_buffer *buf, const char *str)
{
    if (!str) str = "";
    size_t len = strlen(str);
    
    if (!buffer_write_varint(buf, len)) return false;
    return buffer_write_bytes(buf, str, len);
}

/* Encode BigInt */
static bool encode_bigint(binary_buffer *buf, const kjson_bigint *bigint)
{
    if (!bigint || !bigint->digits) return false;
    
    if (!buffer_write_byte(buf, KJSONB_BIGINT)) return false;
    if (!buffer_write_byte(buf, bigint->negative ? 1 : 0)) return false;
    return encode_string(buf, bigint->digits);
}

/* Encode Decimal128 */
static bool encode_decimal128(binary_buffer *buf, const kjson_decimal128 *decimal)
{
    if (!decimal || !decimal->digits) return false;
    
    if (!buffer_write_byte(buf, KJSONB_DECIMAL128)) return false;
    if (!buffer_write_byte(buf, decimal->negative ? 1 : 0)) return false;
    if (!buffer_write_signed_varint(buf, decimal->exponent)) return false;
    return encode_string(buf, decimal->digits);
}

/* Encode UUID */
static bool encode_uuid(binary_buffer *buf, const kjson_uuid *uuid)
{
    if (!uuid) return false;
    
    if (!buffer_write_byte(buf, KJSONB_UUID)) return false;
    return buffer_write_bytes(buf, uuid->bytes, 16);
}

/* Encode Instant */
static bool encode_instant(binary_buffer *buf, const kjson_instant *instant)
{
    if (!instant) return false;
    
    if (!buffer_write_byte(buf, KJSONB_INSTANT)) return false;
    if (!buffer_write_signed_varint(buf, instant->nanoseconds)) return false;
    
    return true;
}

/* Encode array */
static bool encode_array(binary_buffer *buf, const kjson_value *array)
{
    if (!buffer_write_byte(buf, KJSONB_ARRAY)) return false;
    if (!buffer_write_varint(buf, array->u.array.count)) return false;
    
    for (size_t i = 0; i < array->u.array.count; i++) {
        if (!encode_value(buf, array->u.array.items[i])) return false;
    }
    
    return true;
}

/* Encode object */
static bool encode_object(binary_buffer *buf, const kjson_value *object)
{
    if (!buffer_write_byte(buf, KJSONB_OBJECT)) return false;
    if (!buffer_write_varint(buf, object->u.object.count)) return false;
    
    kjson_member *member = object->u.object.first;
    while (member) {
        if (!encode_string(buf, member->key)) return false;
        if (!encode_value(buf, member->value)) return false;
        member = member->next;
    }
    
    return true;
}

/* Encode value */
static bool encode_value(binary_buffer *buf, const kjson_value *value)
{
    if (!value) {
        return buffer_write_byte(buf, KJSONB_NULL);
    }
    
    switch (value->type) {
        case KJSON_TYPE_NULL:
            return buffer_write_byte(buf, KJSONB_NULL);
            
        case KJSON_TYPE_BOOLEAN:
            return buffer_write_byte(buf, value->u.boolean ? KJSONB_TRUE : KJSONB_FALSE);
            
        case KJSON_TYPE_NUMBER: {
            double d = value->u.number;
            
            /* Try to encode as integer if possible */
            if (d == floor(d) && d >= INT64_MIN && d <= INT64_MAX) {
                int64_t i = (int64_t)d;
                
                if (i >= INT8_MIN && i <= INT8_MAX) {
                    if (!buffer_write_byte(buf, KJSONB_INT8)) return false;
                    return buffer_write_byte(buf, (uint8_t)i);
                } else if (i >= INT16_MIN && i <= INT16_MAX) {
                    if (!buffer_write_byte(buf, KJSONB_INT16)) return false;
                    int16_t val = (int16_t)i;
                    return buffer_write_bytes(buf, &val, sizeof(int16_t));
                } else if (i >= INT32_MIN && i <= INT32_MAX) {
                    if (!buffer_write_byte(buf, KJSONB_INT32)) return false;
                    int32_t val = (int32_t)i;
                    return buffer_write_bytes(buf, &val, sizeof(int32_t));
                } else {
                    if (!buffer_write_byte(buf, KJSONB_INT64)) return false;
                    return buffer_write_bytes(buf, &i, sizeof(int64_t));
                }
            }
            
            /* Use float if possible */
            float f = (float)d;
            if ((double)f == d) {
                if (!buffer_write_byte(buf, KJSONB_FLOAT32)) return false;
                return buffer_write_float32(buf, f);
            }
            
            /* Use double */
            if (!buffer_write_byte(buf, KJSONB_FLOAT64)) return false;
            return buffer_write_float64(buf, d);
        }
            
        case KJSON_TYPE_STRING:
            if (!buffer_write_byte(buf, KJSONB_STRING)) return false;
            return encode_string(buf, value->u.string);
            
        case KJSON_TYPE_BIGINT:
            return encode_bigint(buf, value->u.bigint);
            
        case KJSON_TYPE_DECIMAL128:
            return encode_decimal128(buf, value->u.decimal);
            
        case KJSON_TYPE_UUID:
            return encode_uuid(buf, value->u.uuid);
            
        case KJSON_TYPE_INSTANT:
            return encode_instant(buf, value->u.instant);
            
        case KJSON_TYPE_ARRAY:
            return encode_array(buf, value);
            
        case KJSON_TYPE_OBJECT:
            return encode_object(buf, value);
            
        case KJSON_TYPE_UNDEFINED:
            return buffer_write_byte(buf, KJSONB_UNDEFINED);
            
        case KJSON_TYPE_BINARY:
            if (!buffer_write_byte(buf, KJSONB_BINARY)) return false;
            if (!buffer_write_varint(buf, value->binary_size)) return false;
            return buffer_write_bytes(buf, value->u.binary, value->binary_size);
            
        default:
            return false;
    }
}

/* Read byte from decode state */
static bool decode_read_byte(decode_state *state, uint8_t *byte)
{
    if (state->position >= state->length) {
        state->error = KJSON_ERROR_INCOMPLETE;
        return false;
    }
    *byte = state->data[state->position++];
    return true;
}

/* Read bytes from decode state */
static bool decode_read_bytes(decode_state *state, void *dest, size_t len)
{
    if (state->position + len > state->length) {
        state->error = KJSON_ERROR_INCOMPLETE;
        return false;
    }
    memcpy(dest, state->data + state->position, len);
    state->position += len;
    return true;
}

/* Decode variable-length integer */
static bool decode_varint(decode_state *state, uint64_t *value)
{
    *value = 0;
    int shift = 0;
    
    while (true) {
        uint8_t byte;
        if (!decode_read_byte(state, &byte)) return false;
        
        if (shift >= 64) {
            state->error = KJSON_ERROR_OVERFLOW;
            return false;
        }
        
        *value |= ((uint64_t)(byte & 0x7F)) << shift;
        
        if ((byte & 0x80) == 0) break;
        shift += 7;
    }
    
    return true;
}

/* Decode signed varint using zigzag encoding */
static bool decode_signed_varint(decode_state *state, int64_t *value)
{
    uint64_t encoded;
    if (!decode_varint(state, &encoded)) return false;
    
    /* Zigzag decoding */
    if (encoded & 1) {
        *value = -((int64_t)(encoded >> 1)) - 1;
    } else {
        *value = (int64_t)(encoded >> 1);
    }
    
    return true;
}

/* Decode string */
static char *decode_string(decode_state *state)
{
    uint64_t len;
    if (!decode_varint(state, &len)) return NULL;
    
    if (len > SIZE_MAX - 1) {
        state->error = KJSON_ERROR_SIZE_EXCEEDED;
        return NULL;
    }
    
    char *str = state->alloc(len + 1);
    if (!str) {
        state->error = KJSON_ERROR_MEMORY;
        return NULL;
    }
    
    if (len > 0) {
        if (!decode_read_bytes(state, str, len)) {
            state->dealloc(str);
            return NULL;
        }
    }
    
    str[len] = '\0';
    return str;
}

/* Create value helper */
static kjson_value *create_value(decode_state *state, kjson_type type)
{
    kjson_value *value = state->alloc(sizeof(kjson_value));
    if (!value) {
        state->error = KJSON_ERROR_MEMORY;
        return NULL;
    }
    memset(value, 0, sizeof(kjson_value));
    value->type = type;
    return value;
}

/* Decode value */
static kjson_value *decode_value(decode_state *state)
{
    uint8_t type_byte;
    if (!decode_read_byte(state, &type_byte)) return NULL;
    
    switch (type_byte) {
        case KJSONB_NULL:
            return create_value(state, KJSON_TYPE_NULL);
            
        case KJSONB_FALSE: {
            kjson_value *v = create_value(state, KJSON_TYPE_BOOLEAN);
            if (v) v->u.boolean = false;
            return v;
        }
            
        case KJSONB_TRUE: {
            kjson_value *v = create_value(state, KJSON_TYPE_BOOLEAN);
            if (v) v->u.boolean = true;
            return v;
        }
            
        case KJSONB_INT8: {
            uint8_t val;
            if (!decode_read_byte(state, &val)) return NULL;
            kjson_value *v = create_value(state, KJSON_TYPE_NUMBER);
            if (v) v->u.number = (int8_t)val;
            return v;
        }
            
        case KJSONB_INT16: {
            int16_t val;
            if (!decode_read_bytes(state, &val, sizeof(int16_t))) return NULL;
            kjson_value *v = create_value(state, KJSON_TYPE_NUMBER);
            if (v) v->u.number = val;
            return v;
        }
            
        case KJSONB_INT32: {
            int32_t val;
            if (!decode_read_bytes(state, &val, sizeof(int32_t))) return NULL;
            kjson_value *v = create_value(state, KJSON_TYPE_NUMBER);
            if (v) v->u.number = val;
            return v;
        }
            
        case KJSONB_INT64: {
            int64_t val;
            if (!decode_read_bytes(state, &val, sizeof(int64_t))) return NULL;
            kjson_value *v = create_value(state, KJSON_TYPE_NUMBER);
            if (v) v->u.number = (double)val;
            return v;
        }
            
        case KJSONB_FLOAT32: {
            float val;
            if (!decode_read_bytes(state, &val, sizeof(float))) return NULL;
            kjson_value *v = create_value(state, KJSON_TYPE_NUMBER);
            if (v) v->u.number = val;
            return v;
        }
            
        case KJSONB_FLOAT64: {
            double val;
            if (!decode_read_bytes(state, &val, sizeof(double))) return NULL;
            kjson_value *v = create_value(state, KJSON_TYPE_NUMBER);
            if (v) v->u.number = val;
            return v;
        }
            
        case KJSONB_STRING: {
            char *str = decode_string(state);
            if (!str) return NULL;
            kjson_value *v = create_value(state, KJSON_TYPE_STRING);
            if (!v) {
                state->dealloc(str);
                return NULL;
            }
            v->u.string = str;
            return v;
        }
            
        case KJSONB_BIGINT: {
            uint8_t negative;
            if (!decode_read_byte(state, &negative)) return NULL;
            
            char *digits = decode_string(state);
            if (!digits) return NULL;
            
            kjson_value *v = create_value(state, KJSON_TYPE_BIGINT);
            if (!v) {
                state->dealloc(digits);
                return NULL;
            }
            
            v->u.bigint = state->alloc(sizeof(kjson_bigint));
            if (!v->u.bigint) {
                state->dealloc(digits);
                kjson_free(v);
                state->error = KJSON_ERROR_MEMORY;
                return NULL;
            }
            
            v->u.bigint->negative = negative != 0;
            v->u.bigint->digits = digits;
            return v;
        }
            
        case KJSONB_DECIMAL128: {
            uint8_t negative;
            if (!decode_read_byte(state, &negative)) return NULL;
            
            int64_t exponent;
            if (!decode_signed_varint(state, &exponent)) return NULL;
            
            char *digits = decode_string(state);
            if (!digits) return NULL;
            
            kjson_value *v = create_value(state, KJSON_TYPE_DECIMAL128);
            if (!v) {
                state->dealloc(digits);
                return NULL;
            }
            
            v->u.decimal = state->alloc(sizeof(kjson_decimal128));
            if (!v->u.decimal) {
                state->dealloc(digits);
                kjson_free(v);
                state->error = KJSON_ERROR_MEMORY;
                return NULL;
            }
            
            v->u.decimal->negative = negative != 0;
            v->u.decimal->exponent = (int32_t)exponent;
            v->u.decimal->digits = digits;
            return v;
        }
            
        case KJSONB_UUID: {
            kjson_value *v = create_value(state, KJSON_TYPE_UUID);
            if (!v) return NULL;
            
            v->u.uuid = state->alloc(sizeof(kjson_uuid));
            if (!v->u.uuid) {
                kjson_free(v);
                state->error = KJSON_ERROR_MEMORY;
                return NULL;
            }
            
            if (!decode_read_bytes(state, v->u.uuid->bytes, 16)) {
                kjson_free(v);
                return NULL;
            }
            
            return v;
        }
            
        case KJSONB_INSTANT: {
            int64_t nanoseconds;
            if (!decode_signed_varint(state, &nanoseconds)) return NULL;
            
            kjson_value *v = create_value(state, KJSON_TYPE_INSTANT);
            if (!v) return NULL;
            
            v->u.instant = state->alloc(sizeof(kjson_instant));
            if (!v->u.instant) {
                kjson_free(v);
                state->error = KJSON_ERROR_MEMORY;
                return NULL;
            }
            
            v->u.instant->nanoseconds = nanoseconds;
            return v;
        }
            
        case KJSONB_ARRAY: {
            uint64_t count;
            if (!decode_varint(state, &count)) return NULL;
            
            kjson_value *v = create_value(state, KJSON_TYPE_ARRAY);
            if (!v) return NULL;
            
            if (count > 0) {
                v->u.array.items = state->alloc(sizeof(kjson_value *) * count);
                if (!v->u.array.items) {
                    kjson_free(v);
                    state->error = KJSON_ERROR_MEMORY;
                    return NULL;
                }
                
                v->u.array.capacity = count;
                
                for (size_t i = 0; i < count; i++) {
                    kjson_value *item = decode_value(state);
                    if (!item) {
                        /* Free partially constructed array */
                        for (size_t j = 0; j < i; j++) {
                            kjson_free(v->u.array.items[j]);
                        }
                        state->dealloc(v->u.array.items);
                        kjson_free(v);
                        return NULL;
                    }
                    v->u.array.items[i] = item;
                    v->u.array.count++;
                }
            }
            
            return v;
        }
            
        case KJSONB_OBJECT: {
            uint64_t count;
            if (!decode_varint(state, &count)) return NULL;
            
            kjson_value *v = create_value(state, KJSON_TYPE_OBJECT);
            if (!v) return NULL;
            
            kjson_member *last = NULL;
            
            for (size_t i = 0; i < count; i++) {
                char *key = decode_string(state);
                if (!key) {
                    kjson_free(v);
                    return NULL;
                }
                
                kjson_value *value = decode_value(state);
                if (!value) {
                    state->dealloc(key);
                    kjson_free(v);
                    return NULL;
                }
                
                kjson_member *member = state->alloc(sizeof(kjson_member));
                if (!member) {
                    state->dealloc(key);
                    kjson_free(value);
                    kjson_free(v);
                    state->error = KJSON_ERROR_MEMORY;
                    return NULL;
                }
                
                member->key = key;
                member->value = value;
                member->next = NULL;
                
                if (last) {
                    last->next = member;
                } else {
                    v->u.object.first = member;
                }
                last = member;
                v->u.object.count++;
            }
            
            return v;
        }
            
        case KJSONB_UNDEFINED:
            return create_value(state, KJSON_TYPE_UNDEFINED);
            
        case KJSONB_BINARY: {
            uint64_t size;
            if (!decode_varint(state, &size)) return NULL;
            
            if (size > SIZE_MAX) {
                state->error = KJSON_ERROR_SIZE_EXCEEDED;
                return NULL;
            }
            
            kjson_value *v = create_value(state, KJSON_TYPE_BINARY);
            if (!v) return NULL;
            
            if (size > 0) {
                v->u.binary = state->alloc(size);
                if (!v->u.binary) {
                    kjson_free(v);
                    state->error = KJSON_ERROR_MEMORY;
                    return NULL;
                }
                
                if (!decode_read_bytes(state, v->u.binary, size)) {
                    kjson_free(v);
                    return NULL;
                }
            }
            
            v->binary_size = size;
            return v;
        }
            
        default:
            state->error = KJSON_ERROR_INVALID_BINARY;
            return NULL;
    }
}

/* Public API: Encode to binary */
uint8_t *kjson_encode_binary(const kjson_value *value, size_t *size)
{
    binary_buffer buf;
    
    if (!buffer_init(&buf, 256, NULL, NULL)) {
        return NULL;
    }
    
    if (!encode_value(&buf, value)) {
        free(buf.data);
        return NULL;
    }
    
    *size = buf.length;
    return buf.data;
}

/* Public API: Encode to buffer */
kjson_error kjson_encode_binary_to(const kjson_value *value, uint8_t *buffer, 
                                   size_t buffer_size, size_t *written)
{
    binary_buffer buf = {
        .data = buffer,
        .length = 0,
        .capacity = buffer_size,
        .alloc = NULL,
        .dealloc = NULL
    };
    
    if (!encode_value(&buf, value)) {
        return KJSON_ERROR_SIZE_EXCEEDED;
    }
    
    *written = buf.length;
    return KJSON_OK;
}

/* Public API: Decode from binary */
kjson_value *kjson_decode_binary(const uint8_t *data, size_t size, kjson_error *error)
{
    decode_state state = {
        .data = data,
        .length = size,
        .position = 0,
        .error = KJSON_OK,
        .alloc = malloc,
        .dealloc = free
    };
    
    kjson_value *value = decode_value(&state);
    
    if (value && state.position < state.length) {
        kjson_free(value);
        state.error = KJSON_ERROR_TRAILING_DATA;
        value = NULL;
    }
    
    if (error) {
        *error = state.error;
    }
    
    return value;
}

/* Public API: Get binary size */
size_t kjson_binary_size(const kjson_value *value)
{
    binary_buffer buf;
    
    if (!buffer_init(&buf, 256, NULL, NULL)) {
        return 0;
    }
    
    if (!encode_value(&buf, value)) {
        free(buf.data);
        return 0;
    }
    
    size_t size = buf.length;
    free(buf.data);
    return size;
}