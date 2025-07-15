/*
 * kjson_binary.c - Native PostgreSQL implementation of kJSON binary format
 * 
 * This implementation uses PostgreSQL's memory management throughout.
 */

#include "postgres.h"
#include "lib/stringinfo.h"
#include "utils/memutils.h"
#include "kjson_internal.h"

#include <string.h>

/* Type bytes for binary format */
#define KJSON_BINARY_NULL       0x00
#define KJSON_BINARY_TRUE       0x01
#define KJSON_BINARY_FALSE      0x02
#define KJSON_BINARY_INT8       0x03
#define KJSON_BINARY_INT16      0x04
#define KJSON_BINARY_INT32      0x05
#define KJSON_BINARY_INT64      0x06
#define KJSON_BINARY_FLOAT64    0x07
#define KJSON_BINARY_STRING     0x08
#define KJSON_BINARY_ARRAY      0x09
#define KJSON_BINARY_OBJECT     0x0A
#define KJSON_BINARY_BIGINT     0x0B
#define KJSON_BINARY_DECIMAL128 0x0C
#define KJSON_BINARY_UUID       0x0D
#define KJSON_BINARY_DATE       0x0E
#define KJSON_BINARY_DURATION   0x0F

/* Forward declarations */
static void encode_value(StringInfo buf, const kjson_value *value);
static void encode_varint(StringInfo buf, uint64_t value);
static kjson_value *decode_value(const uint8_t **data, const uint8_t *end, MemoryContext memctx);
static uint64_t decode_varint(const uint8_t **data, const uint8_t *end);

/* Encode variable-length integer */
static void
encode_varint(StringInfo buf, uint64_t value)
{
    while (value >= 0x80) {
        appendStringInfoChar(buf, (value & 0x7F) | 0x80);
        value >>= 7;
    }
    appendStringInfoChar(buf, value & 0x7F);
}

/* Decode variable-length integer */
static uint64_t
decode_varint(const uint8_t **data, const uint8_t *end)
{
    uint64_t value = 0;
    int shift = 0;
    
    while (*data < end) {
        uint8_t byte = **data;
        (*data)++;
        
        value |= ((uint64_t)(byte & 0x7F)) << shift;
        
        if ((byte & 0x80) == 0) {
            return value;
        }
        
        shift += 7;
        if (shift >= 64) {
            return 0; /* Overflow */
        }
    }
    
    return 0; /* Incomplete */
}

/* Encode a value */
static void
encode_value(StringInfo buf, const kjson_value *value)
{
    if (!value) {
        appendStringInfoChar(buf, KJSON_BINARY_NULL);
        return;
    }
    
    switch (value->type) {
        case KJSON_TYPE_NULL:
            appendStringInfoChar(buf, KJSON_BINARY_NULL);
            break;
            
        case KJSON_TYPE_BOOLEAN:
            appendStringInfoChar(buf, value->u.boolean ? 
                               KJSON_BINARY_TRUE : KJSON_BINARY_FALSE);
            break;
            
        case KJSON_TYPE_NUMBER:
            {
                /* Always encode as float64 for simplicity */
                double d;
                
                appendStringInfoChar(buf, KJSON_BINARY_FLOAT64);
                d = value->u.number;
                appendBinaryStringInfo(buf, (char *)&d, sizeof(double));
            }
            break;
            
        case KJSON_TYPE_STRING:
            {
                size_t len;
                
                appendStringInfoChar(buf, KJSON_BINARY_STRING);
                len = value->u.string ? strlen(value->u.string) : 0;
                encode_varint(buf, len);
                if (len > 0) {
                    appendBinaryStringInfo(buf, value->u.string, len);
                }
            }
            break;
            
        case KJSON_TYPE_BIGINT:
            {
                size_t len;
                
                appendStringInfoChar(buf, KJSON_BINARY_BIGINT);
                if (value->u.bigint) {
                    appendStringInfoChar(buf, value->u.bigint->negative ? 1 : 0);
                    len = strlen(value->u.bigint->digits);
                    encode_varint(buf, len);
                    appendBinaryStringInfo(buf, value->u.bigint->digits, len);
                } else {
                    appendStringInfoChar(buf, 0);
                    encode_varint(buf, 1);
                    appendStringInfoChar(buf, '0');
                }
            }
            break;
            
        case KJSON_TYPE_DECIMAL128:
            {
                int32_t exp;
                uint32_t uexp;
                size_t len;
                
                appendStringInfoChar(buf, KJSON_BINARY_DECIMAL128);
                if (value->u.decimal) {
                    appendStringInfoChar(buf, value->u.decimal->negative ? 1 : 0);
                    
                    /* Encode exponent as signed varint */
                    exp = value->u.decimal->exp;
                    uexp = (exp < 0) ? ((-exp) << 1) | 1 : (exp << 1);
                    encode_varint(buf, uexp);
                    
                    len = strlen(value->u.decimal->digits);
                    encode_varint(buf, len);
                    appendBinaryStringInfo(buf, value->u.decimal->digits, len);
                } else {
                    appendStringInfoChar(buf, 0);
                    encode_varint(buf, 0);
                    encode_varint(buf, 1);
                    appendStringInfoChar(buf, '0');
                }
            }
            break;
            
        case KJSON_TYPE_UUID:
            {
                appendStringInfoChar(buf, KJSON_BINARY_UUID);
                if (value->u.uuid) {
                    appendBinaryStringInfo(buf, (char *)value->u.uuid->bytes, 16);
                } else {
                    /* Null UUID */
                    for (int i = 0; i < 16; i++) {
                        appendStringInfoChar(buf, 0);
                    }
                }
            }
            break;
            
        case KJSON_TYPE_INSTANT:
            {
                uint64_t ns;
                int16_t tz;
                uint64_t zero;
                int16_t tz_zero;
                
                appendStringInfoChar(buf, KJSON_BINARY_DATE);
                if (value->u.instant) {
                    /* Encode nanoseconds and timezone offset */
                    ns = value->u.instant->nanoseconds;
                    appendBinaryStringInfo(buf, (char *)&ns, sizeof(uint64_t));
                    
                    tz = value->u.instant->tz_offset;
                    appendBinaryStringInfo(buf, (char *)&tz, sizeof(int16_t));
                } else {
                    zero = 0;
                    appendBinaryStringInfo(buf, (char *)&zero, sizeof(uint64_t));
                    tz_zero = 0;
                    appendBinaryStringInfo(buf, (char *)&tz_zero, sizeof(int16_t));
                }
            }
            break;
            
        case KJSON_TYPE_DURATION:
            {
                appendStringInfoChar(buf, KJSON_BINARY_DURATION);
                if (value->u.duration) {
                    /* Encode all duration components */
                    appendBinaryStringInfo(buf, (char *)&value->u.duration->years, sizeof(int32_t));
                    appendBinaryStringInfo(buf, (char *)&value->u.duration->months, sizeof(int32_t));
                    appendBinaryStringInfo(buf, (char *)&value->u.duration->days, sizeof(int32_t));
                    appendBinaryStringInfo(buf, (char *)&value->u.duration->hours, sizeof(int32_t));
                    appendBinaryStringInfo(buf, (char *)&value->u.duration->minutes, sizeof(int32_t));
                    appendBinaryStringInfo(buf, (char *)&value->u.duration->nanoseconds, sizeof(int64_t));
                    appendBinaryStringInfo(buf, (char *)&value->u.duration->negative, sizeof(bool));
                } else {
                    /* Encode zero duration */
                    int32_t zero32;
                    int64_t zero64;
                    bool zero_bool;
                    
                    zero32 = 0;
                    zero64 = 0;
                    zero_bool = false;
                    appendBinaryStringInfo(buf, (char *)&zero32, sizeof(int32_t)); /* years */
                    appendBinaryStringInfo(buf, (char *)&zero32, sizeof(int32_t)); /* months */
                    appendBinaryStringInfo(buf, (char *)&zero32, sizeof(int32_t)); /* days */
                    appendBinaryStringInfo(buf, (char *)&zero32, sizeof(int32_t)); /* hours */
                    appendBinaryStringInfo(buf, (char *)&zero32, sizeof(int32_t)); /* minutes */
                    appendBinaryStringInfo(buf, (char *)&zero64, sizeof(int64_t)); /* nanoseconds */
                    appendBinaryStringInfo(buf, (char *)&zero_bool, sizeof(bool)); /* negative */
                }
            }
            break;
            
        case KJSON_TYPE_ARRAY:
            {
                appendStringInfoChar(buf, KJSON_BINARY_ARRAY);
                encode_varint(buf, value->u.array.count);
                
                for (size_t i = 0; i < value->u.array.count; i++) {
                    encode_value(buf, value->u.array.items[i]);
                }
            }
            break;
            
        case KJSON_TYPE_OBJECT:
            {
                kjson_member *member;
                size_t key_len;
                
                appendStringInfoChar(buf, KJSON_BINARY_OBJECT);
                encode_varint(buf, value->u.object.count);
                
                member = value->u.object.first;
                while (member) {
                    /* Encode key */
                    key_len = strlen(member->key);
                    encode_varint(buf, key_len);
                    appendBinaryStringInfo(buf, member->key, key_len);
                    
                    /* Encode value */
                    encode_value(buf, member->value);
                    
                    member = member->next;
                }
            }
            break;
            
        default:
            appendStringInfoChar(buf, KJSON_BINARY_NULL);
    }
}

/* Decode a value */
static kjson_value *
decode_value(const uint8_t **data, const uint8_t *end, MemoryContext memctx)
{
    uint8_t type;
    kjson_value *value;
    
    if (*data >= end) {
        return NULL;
    }
    
    type = **data;
    (*data)++;
    
    value = MemoryContextAllocZero(memctx, sizeof(kjson_value));
    
    switch (type) {
        case KJSON_BINARY_NULL:
            value->type = KJSON_TYPE_NULL;
            break;
            
        case KJSON_BINARY_TRUE:
            value->type = KJSON_TYPE_BOOLEAN;
            value->u.boolean = true;
            break;
            
        case KJSON_BINARY_FALSE:
            value->type = KJSON_TYPE_BOOLEAN;
            value->u.boolean = false;
            break;
            
        case KJSON_BINARY_FLOAT64:
            if (*data + sizeof(double) > end) {
                return NULL;
            }
            value->type = KJSON_TYPE_NUMBER;
            memcpy(&value->u.number, *data, sizeof(double));
            *data += sizeof(double);
            break;
            
        case KJSON_BINARY_STRING:
            {
                uint64_t len;
                
                len = decode_varint(data, end);
                if (*data + len > end) {
                    return NULL;
                }
                
                value->type = KJSON_TYPE_STRING;
                value->u.string = MemoryContextAlloc(memctx, len + 1);
                memcpy(value->u.string, *data, len);
                value->u.string[len] = '\0';
                *data += len;
            }
            break;
            
        case KJSON_BINARY_BIGINT:
            {
                bool negative;
                uint64_t len;
                
                if (*data >= end) return NULL;
                negative = **data;
                (*data)++;
                
                len = decode_varint(data, end);
                if (*data + len > end) {
                    return NULL;
                }
                
                value->type = KJSON_TYPE_BIGINT;
                value->u.bigint = MemoryContextAlloc(memctx, sizeof(kjson_bigint));
                value->u.bigint->negative = negative;
                value->u.bigint->digits = MemoryContextAlloc(memctx, len + 1);
                memcpy(value->u.bigint->digits, *data, len);
                value->u.bigint->digits[len] = '\0';
                *data += len;
            }
            break;
            
        case KJSON_BINARY_DECIMAL128:
            {
                bool negative;
                uint64_t uexp;
                int32_t exp;
                uint64_t len;
                
                if (*data >= end) return NULL;
                negative = **data;
                (*data)++;
                
                /* Decode exponent */
                uexp = decode_varint(data, end);
                exp = (uexp & 1) ? -(uexp >> 1) : (uexp >> 1);
                
                len = decode_varint(data, end);
                if (*data + len > end) {
                    return NULL;
                }
                
                value->type = KJSON_TYPE_DECIMAL128;
                value->u.decimal = MemoryContextAlloc(memctx, sizeof(kjson_decimal128));
                value->u.decimal->negative = negative;
                value->u.decimal->exp = exp;
                value->u.decimal->digits = MemoryContextAlloc(memctx, len + 1);
                memcpy(value->u.decimal->digits, *data, len);
                value->u.decimal->digits[len] = '\0';
                *data += len;
            }
            break;
            
        case KJSON_BINARY_UUID:
            {
                if (*data + 16 > end) {
                    return NULL;
                }
                
                value->type = KJSON_TYPE_UUID;
                value->u.uuid = MemoryContextAlloc(memctx, sizeof(kjson_uuid));
                memcpy(value->u.uuid->bytes, *data, 16);
                *data += 16;
            }
            break;
            
        case KJSON_BINARY_DATE:
            {
                if (*data + sizeof(uint64_t) + sizeof(int16_t) > end) {
                    return NULL;
                }
                
                value->type = KJSON_TYPE_INSTANT;
                value->u.instant = MemoryContextAlloc(memctx, sizeof(kjson_instant));
                memcpy(&value->u.instant->nanoseconds, *data, sizeof(uint64_t));
                *data += sizeof(uint64_t);
                memcpy(&value->u.instant->tz_offset, *data, sizeof(int16_t));
                *data += sizeof(int16_t);
            }
            break;
            
        case KJSON_BINARY_DURATION:
            {
                /* Check if we have enough data for all duration components */
                size_t duration_size;
                
                duration_size = sizeof(int32_t) * 5 + sizeof(int64_t) + sizeof(bool);
                if (*data + duration_size > end) {
                    return NULL;
                }
                
                value->type = KJSON_TYPE_DURATION;
                value->u.duration = MemoryContextAlloc(memctx, sizeof(kjson_duration));
                
                /* Decode all duration components */
                memcpy(&value->u.duration->years, *data, sizeof(int32_t));
                *data += sizeof(int32_t);
                memcpy(&value->u.duration->months, *data, sizeof(int32_t));
                *data += sizeof(int32_t);
                memcpy(&value->u.duration->days, *data, sizeof(int32_t));
                *data += sizeof(int32_t);
                memcpy(&value->u.duration->hours, *data, sizeof(int32_t));
                *data += sizeof(int32_t);
                memcpy(&value->u.duration->minutes, *data, sizeof(int32_t));
                *data += sizeof(int32_t);
                memcpy(&value->u.duration->nanoseconds, *data, sizeof(int64_t));
                *data += sizeof(int64_t);
                memcpy(&value->u.duration->negative, *data, sizeof(bool));
                *data += sizeof(bool);
            }
            break;
            
        case KJSON_BINARY_ARRAY:
            {
                uint64_t count;
                
                count = decode_varint(data, end);
                
                value->type = KJSON_TYPE_ARRAY;
                value->u.array.count = count;
                value->u.array.capacity = count;
                value->u.array.items = MemoryContextAllocZero(memctx,
                    sizeof(kjson_value *) * count);
                
                for (size_t i = 0; i < count; i++) {
                    value->u.array.items[i] = decode_value(data, end, memctx);
                    if (!value->u.array.items[i]) {
                        return NULL;
                    }
                }
            }
            break;
            
        case KJSON_BINARY_OBJECT:
            {
                uint64_t count;
                kjson_member *last;
                uint64_t key_len;
                char *key;
                kjson_value *member_value;
                kjson_member *member;
                
                count = decode_varint(data, end);
                
                value->type = KJSON_TYPE_OBJECT;
                value->u.object.count = count;
                value->u.object.first = NULL;
                
                last = NULL;
                
                for (size_t i = 0; i < count; i++) {
                    /* Decode key */
                    key_len = decode_varint(data, end);
                    if (*data + key_len > end) {
                        return NULL;
                    }
                    
                    key = MemoryContextAlloc(memctx, key_len + 1);
                    memcpy(key, *data, key_len);
                    key[key_len] = '\0';
                    *data += key_len;
                    
                    /* Decode value */
                    member_value = decode_value(data, end, memctx);
                    if (!member_value) {
                        return NULL;
                    }
                    
                    /* Create member */
                    member = MemoryContextAlloc(memctx, sizeof(kjson_member));
                    member->key = key;
                    member->value = member_value;
                    member->next = NULL;
                    
                    /* Add to list */
                    if (last) {
                        last->next = member;
                    } else {
                        value->u.object.first = member;
                    }
                    last = member;
                }
            }
            break;
            
        default:
            return NULL;
    }
    
    return value;
}

/* Public API: Encode to binary */
StringInfo
pg_kjson_encode_binary(const kjson_value *value)
{
    StringInfoData buf;
    StringInfo result;
    
    initStringInfo(&buf);
    
    encode_value(&buf, value);
    
    /* Return the StringInfo directly for efficiency */
    result = palloc(sizeof(StringInfoData));
    *result = buf;
    return result;
}

/* Public API: Decode from binary */
kjson_value *
pg_kjson_decode_binary(const uint8_t *data, size_t size, MemoryContext memctx)
{
    const uint8_t *ptr = data;
    const uint8_t *end = data + size;
    
    return decode_value(&ptr, end, memctx);
}