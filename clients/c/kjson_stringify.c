/*
 * kjson_stringify.c - kJSON stringification implementation
 * 
 * Converts kjson_value trees to kJSON text format.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include <ctype.h>
#include <time.h>

#include "kjson.h"

/* Default write options */
static const kjson_write_options default_write_options = {
    .pretty = false,
    .indent = 2,
    .quote_keys = false,
    .bigint_suffix = true,
    .decimal_suffix = true,
    .escape_unicode = false
};

/* String buffer for building output */
typedef struct {
    char *data;
    size_t length;
    size_t capacity;
    const kjson_write_options *options;
    int depth;
    void *(*alloc)(size_t);
    void (*dealloc)(void *);
} string_buffer;

/* Forward declarations */
static bool stringify_value(string_buffer *buf, const kjson_value *value);
static bool buffer_append(string_buffer *buf, const char *str, size_t len);
static bool buffer_append_char(string_buffer *buf, char c);
static bool buffer_append_string(string_buffer *buf, const char *str);

/* Initialize buffer */
static bool buffer_init(string_buffer *buf, size_t initial_capacity,
                       const kjson_write_options *options,
                       void *(*alloc)(size_t), void (*dealloc)(void *))
{
    buf->capacity = initial_capacity > 0 ? initial_capacity : 256;
    buf->data = alloc ? alloc(buf->capacity) : malloc(buf->capacity);
    if (!buf->data) return false;
    
    buf->length = 0;
    buf->options = options ? options : &default_write_options;
    buf->depth = 0;
    buf->alloc = alloc ? alloc : malloc;
    buf->dealloc = dealloc ? dealloc : free;
    
    return true;
}

/* Free buffer (but not the data, which is returned) */
static char *buffer_finish(string_buffer *buf)
{
    if (buf->length >= buf->capacity) {
        /* Need one more byte for null terminator */
        size_t new_capacity = buf->capacity + 1;
        char *new_data = buf->alloc(new_capacity);
        if (!new_data) {
            buf->dealloc(buf->data);
            return NULL;
        }
        memcpy(new_data, buf->data, buf->length);
        buf->dealloc(buf->data);
        buf->data = new_data;
    }
    
    buf->data[buf->length] = '\0';
    return buf->data;
}

/* Grow buffer if needed */
static bool buffer_grow(string_buffer *buf, size_t min_additional)
{
    size_t required = buf->length + min_additional;
    if (required <= buf->capacity) return true;
    
    size_t new_capacity = buf->capacity * 2;
    while (new_capacity < required) {
        new_capacity *= 2;
    }
    
    char *new_data = buf->alloc(new_capacity);
    if (!new_data) return false;
    
    memcpy(new_data, buf->data, buf->length);
    buf->dealloc(buf->data);
    buf->data = new_data;
    buf->capacity = new_capacity;
    
    return true;
}

/* Append data to buffer */
static bool buffer_append(string_buffer *buf, const char *str, size_t len)
{
    if (!buffer_grow(buf, len)) return false;
    
    memcpy(buf->data + buf->length, str, len);
    buf->length += len;
    return true;
}

/* Append single character */
static bool buffer_append_char(string_buffer *buf, char c)
{
    return buffer_append(buf, &c, 1);
}

/* Append null-terminated string */
static bool buffer_append_string(string_buffer *buf, const char *str)
{
    return buffer_append(buf, str, strlen(str));
}

/* Append indentation */
static bool buffer_append_indent(string_buffer *buf)
{
    if (!buf->options->pretty) return true;
    
    int spaces = buf->depth * buf->options->indent;
    for (int i = 0; i < spaces; i++) {
        if (!buffer_append_char(buf, ' ')) return false;
    }
    return true;
}

/* Append newline if pretty printing */
static bool buffer_append_newline(string_buffer *buf)
{
    if (buf->options->pretty) {
        return buffer_append_char(buf, '\n');
    }
    return true;
}

/* Check if string needs quotes */
static bool string_needs_quotes(const char *str)
{
    if (!str || !*str) return true;
    
    /* First character must be letter, underscore, or dollar */
    if (!isalpha(str[0]) && str[0] != '_' && str[0] != '$')
        return true;
        
    /* Rest can be alphanumeric, underscore, or dollar */
    for (const char *p = str + 1; *p; p++) {
        if (!isalnum(*p) && *p != '_' && *p != '$')
            return true;
    }
    
    /* Check reserved words */
    if (strcmp(str, "true") == 0 ||
        strcmp(str, "false") == 0 ||
        strcmp(str, "null") == 0 ||
        strcmp(str, "undefined") == 0 ||
        strcmp(str, "Infinity") == 0 ||
        strcmp(str, "NaN") == 0) {
        return true;
    }
    
    return false;
}

/* Select the best quote character for a string */
static char select_quote_char(const char *str, size_t len)
{
    size_t single_quotes = 0;
    size_t double_quotes = 0;
    size_t backticks = 0;
    size_t backslashes = 0;
    
    /* Count occurrences of each quote type */
    for (size_t i = 0; i < len; i++) {
        switch (str[i]) {
            case '\'': single_quotes++; break;
            case '"': double_quotes++; break;
            case '`': backticks++; break;
            case '\\': backslashes++; break;
        }
    }
    
    /* Calculate escaping cost for each quote type */
    size_t single_cost = single_quotes + backslashes;
    size_t double_cost = double_quotes + backslashes;
    size_t backtick_cost = backticks + backslashes;
    
    /* Find minimum cost and choose quote type
     * In case of tie: single > double > backtick */
    size_t min_cost = single_cost;
    char quote_char = '\'';
    
    if (double_cost < min_cost) {
        min_cost = double_cost;
        quote_char = '"';
    }
    
    if (backtick_cost < min_cost) {
        quote_char = '`';
    }
    
    return quote_char;
}

/* Stringify escaped string */
static bool stringify_string_escaped(string_buffer *buf, const char *str, size_t len)
{
    char quote_char = select_quote_char(str, len);
    
    if (!buffer_append_char(buf, quote_char)) return false;
    
    for (size_t i = 0; i < len; i++) {
        unsigned char c = str[i];
        
        switch (c) {
            case '"':
                if (quote_char == '"') {
                    if (!buffer_append_string(buf, "\\\"")) return false;
                } else {
                    if (!buffer_append_char(buf, c)) return false;
                }
                break;
            case '\'':
                if (quote_char == '\'') {
                    if (!buffer_append_string(buf, "\\'")) return false;
                } else {
                    if (!buffer_append_char(buf, c)) return false;
                }
                break;
            case '`':
                if (quote_char == '`') {
                    if (!buffer_append_string(buf, "\\`")) return false;
                } else {
                    if (!buffer_append_char(buf, c)) return false;
                }
                break;
            case '\\':
                if (!buffer_append_string(buf, "\\\\")) return false;
                break;
            case '\b':
                if (!buffer_append_string(buf, "\\b")) return false;
                break;
            case '\f':
                if (!buffer_append_string(buf, "\\f")) return false;
                break;
            case '\n':
                if (!buffer_append_string(buf, "\\n")) return false;
                break;
            case '\r':
                if (!buffer_append_string(buf, "\\r")) return false;
                break;
            case '\t':
                if (!buffer_append_string(buf, "\\t")) return false;
                break;
            default:
                if (c < 0x20 || (buf->options->escape_unicode && c > 0x7F)) {
                    /* Escape as \uXXXX */
                    char hex[7];
                    snprintf(hex, sizeof(hex), "\\u%04x", c);
                    if (!buffer_append_string(buf, hex)) return false;
                } else {
                    if (!buffer_append_char(buf, c)) return false;
                }
                break;
        }
    }
    
    return buffer_append_char(buf, quote_char);
}

/* Stringify string value */
static bool stringify_string(string_buffer *buf, const char *str)
{
    if (!str) str = "";
    return stringify_string_escaped(buf, str, strlen(str));
}

/* Stringify number */
static bool stringify_number(string_buffer *buf, double value)
{
    char number[32];
    
    if (isnan(value)) {
        return buffer_append_string(buf, "NaN");
    } else if (isinf(value)) {
        if (value < 0) {
            return buffer_append_string(buf, "-Infinity");
        } else {
            return buffer_append_string(buf, "Infinity");
        }
    } else {
        /* Use %.17g for maximum precision */
        int len = snprintf(number, sizeof(number), "%.17g", value);
        if (len < 0 || (size_t)len >= sizeof(number)) return false;
        return buffer_append(buf, number, len);
    }
}

/* Stringify BigInt */
static bool stringify_bigint(string_buffer *buf, const kjson_bigint *bigint)
{
    if (!bigint || !bigint->digits) return false;
    
    if (bigint->negative && !buffer_append_char(buf, '-')) return false;
    if (!buffer_append_string(buf, bigint->digits)) return false;
    if (buf->options->bigint_suffix && !buffer_append_char(buf, 'n')) return false;
    
    return true;
}

/* Stringify Decimal128 */
static bool stringify_decimal128(string_buffer *buf, const kjson_decimal128 *decimal)
{
    if (!decimal || !decimal->digits) return false;
    
    if (decimal->negative && !buffer_append_char(buf, '-')) return false;
    if (!buffer_append_string(buf, decimal->digits)) return false;
    
    /* TODO: Handle exponent properly */
    if (decimal->exponent != 0) {
        char exp[16];
        snprintf(exp, sizeof(exp), "e%d", decimal->exponent);
        if (!buffer_append_string(buf, exp)) return false;
    }
    
    if (buf->options->decimal_suffix && !buffer_append_char(buf, 'm')) return false;
    
    return true;
}

/* Stringify UUID */
static bool stringify_uuid(string_buffer *buf, const kjson_uuid *uuid)
{
    if (!uuid) return false;
    
    char uuid_str[37];
    snprintf(uuid_str, sizeof(uuid_str),
             "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
             uuid->bytes[0], uuid->bytes[1], uuid->bytes[2], uuid->bytes[3],
             uuid->bytes[4], uuid->bytes[5],
             uuid->bytes[6], uuid->bytes[7],
             uuid->bytes[8], uuid->bytes[9],
             uuid->bytes[10], uuid->bytes[11], uuid->bytes[12], 
             uuid->bytes[13], uuid->bytes[14], uuid->bytes[15]);
    
    return buffer_append_string(buf, uuid_str);
}

/* Stringify Instant */
static bool stringify_instant(string_buffer *buf, const kjson_instant *instant)
{
    if (!instant) return false;
    
    /* Convert nanoseconds to time_t */
    time_t seconds = instant->nanoseconds / 1000000000LL;
    int nanos = instant->nanoseconds % 1000000000LL;
    
    struct tm *tm = gmtime(&seconds);
    if (!tm) return false;
    
    char instant_str[32];
    int len = strftime(instant_str, sizeof(instant_str), "%Y-%m-%dT%H:%M:%S", tm);
    if (len == 0) return false;
    
    /* Add nanoseconds */
    char full_instant[50];
    if (nanos > 0) {
        snprintf(full_instant, sizeof(full_instant), "%s.%09dZ", instant_str, nanos);
    } else {
        snprintf(full_instant, sizeof(full_instant), "%sZ", instant_str);
    }
    
    return buffer_append_string(buf, full_instant);
}

/* Stringify array */
static bool stringify_array(string_buffer *buf, const kjson_value *array)
{
    if (!buffer_append_char(buf, '[')) return false;
    
    bool first = true;
    size_t count = array->u.array.count;
    
    if (count > 0 && buf->options->pretty) {
        buf->depth++;
    }
    
    for (size_t i = 0; i < count; i++) {
        if (!first) {
            if (!buffer_append_char(buf, ',')) return false;
        } else {
            first = false;
        }
        
        if (buf->options->pretty && count > 0) {
            if (!buffer_append_newline(buf)) return false;
            if (!buffer_append_indent(buf)) return false;
        } else if (!first && !buf->options->pretty) {
            if (!buffer_append_char(buf, ' ')) return false;
        }
        
        if (!stringify_value(buf, array->u.array.items[i])) return false;
    }
    
    if (count > 0 && buf->options->pretty) {
        buf->depth--;
        if (!buffer_append_newline(buf)) return false;
        if (!buffer_append_indent(buf)) return false;
    }
    
    return buffer_append_char(buf, ']');
}

/* Stringify object */
static bool stringify_object(string_buffer *buf, const kjson_value *object)
{
    if (!buffer_append_char(buf, '{')) return false;
    
    bool first = true;
    kjson_member *member = object->u.object.first;
    
    if (member && buf->options->pretty) {
        buf->depth++;
    }
    
    while (member) {
        if (!first) {
            if (!buffer_append_char(buf, ',')) return false;
        } else {
            first = false;
        }
        
        if (buf->options->pretty && object->u.object.count > 0) {
            if (!buffer_append_newline(buf)) return false;
            if (!buffer_append_indent(buf)) return false;
        } else if (!first && !buf->options->pretty) {
            if (!buffer_append_char(buf, ' ')) return false;
        }
        
        /* Key */
        if (buf->options->quote_keys || string_needs_quotes(member->key)) {
            if (!stringify_string(buf, member->key)) return false;
        } else {
            if (!buffer_append_string(buf, member->key)) return false;
        }
        
        if (!buffer_append_char(buf, ':')) return false;
        if (!buffer_append_char(buf, ' ')) return false;
        
        /* Value */
        if (!stringify_value(buf, member->value)) return false;
        
        member = member->next;
    }
    
    if (object->u.object.count > 0 && buf->options->pretty) {
        buf->depth--;
        if (!buffer_append_newline(buf)) return false;
        if (!buffer_append_indent(buf)) return false;
    }
    
    return buffer_append_char(buf, '}');
}

/* Stringify value */
static bool stringify_value(string_buffer *buf, const kjson_value *value)
{
    if (!value) return buffer_append_string(buf, "null");
    
    switch (value->type) {
        case KJSON_TYPE_NULL:
            return buffer_append_string(buf, "null");
            
        case KJSON_TYPE_BOOLEAN:
            return buffer_append_string(buf, value->u.boolean ? "true" : "false");
            
        case KJSON_TYPE_NUMBER:
            return stringify_number(buf, value->u.number);
            
        case KJSON_TYPE_STRING:
            return stringify_string(buf, value->u.string);
            
        case KJSON_TYPE_BIGINT:
            return stringify_bigint(buf, value->u.bigint);
            
        case KJSON_TYPE_DECIMAL128:
            return stringify_decimal128(buf, value->u.decimal);
            
        case KJSON_TYPE_UUID:
            return stringify_uuid(buf, value->u.uuid);
            
        case KJSON_TYPE_INSTANT:
            return stringify_instant(buf, value->u.instant);
            
        case KJSON_TYPE_ARRAY:
            return stringify_array(buf, value);
            
        case KJSON_TYPE_OBJECT:
            return stringify_object(buf, value);
            
        case KJSON_TYPE_UNDEFINED:
            return buffer_append_string(buf, "undefined");
            
        default:
            return false;
    }
}

/* Public API: Stringify with options */
char *kjson_stringify_ex(const kjson_value *value, const kjson_write_options *options)
{
    string_buffer buf;
    
    if (!buffer_init(&buf, 256, options, NULL, NULL)) {
        return NULL;
    }
    
    if (!stringify_value(&buf, value)) {
        free(buf.data);
        return NULL;
    }
    
    return buffer_finish(&buf);
}

/* Public API: Simple stringify */
char *kjson_stringify(const kjson_value *value)
{
    return kjson_stringify_ex(value, &default_write_options);
}

/* Get string length without allocating */
size_t kjson_stringify_length_ex(const kjson_value *value, const kjson_write_options *options)
{
    string_buffer buf;
    
    if (!buffer_init(&buf, 256, options, NULL, NULL)) {
        return 0;
    }
    
    if (!stringify_value(&buf, value)) {
        free(buf.data);
        return 0;
    }
    
    size_t length = buf.length;
    free(buf.data);
    return length;
}

/* Get string length */
size_t kjson_stringify_length(const kjson_value *value)
{
    return kjson_stringify_length_ex(value, &default_write_options);
}