/*
 * kjson.c - kJSON C Library Implementation
 * 
 * Core implementation of the kJSON parser, stringifier, and binary encoder/decoder.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <ctype.h>
#include <math.h>
#include <limits.h>
#include <errno.h>
#include <time.h>
#include <assert.h>

#include "kjson.h"

/* Default memory allocator functions */
static void *default_malloc(size_t size) { return KJSON_MALLOC(size); }
static void default_free(void *ptr) { KJSON_FREE(ptr); }
/* static void *default_realloc(void *ptr, size_t size) { return KJSON_REALLOC(ptr, size); } // unused */

/* Default options */
static const kjson_options default_options = {
    .allow_comments = true,
    .allow_trailing_commas = true,
    .allow_unquoted_keys = true,
    .parse_instants = true,
    .strict_numbers = false,
    .max_depth = 1000,
    .max_string_length = 1024 * 1024 * 1024,  /* 1GB */
    .allocator = NULL,
    .deallocator = NULL,
    .allocator_ctx = NULL
};

/* Parser state */
typedef struct {
    const char *input;
    size_t length;
    size_t position;
    const kjson_options *options;
    size_t depth;
    kjson_error error;
    size_t line;
    size_t column;
    
    /* Memory allocator */
    void *(*alloc)(size_t);
    void (*dealloc)(void *);
} parser_state;

/* Forward declarations */
static kjson_value *parse_value(parser_state *state);
static void skip_whitespace(parser_state *state);
static bool skip_comment(parser_state *state);
static void skip_whitespace_and_comments(parser_state *state);
static kjson_value *parse_object(parser_state *state);
static kjson_value *parse_array(parser_state *state);
static kjson_value *parse_string(parser_state *state);
static kjson_value *parse_number(parser_state *state);
static kjson_value *parse_literal(parser_state *state, const char *literal, kjson_value *value);
static kjson_value *parse_uuid(parser_state *state);
static bool is_instant_start(parser_state *state);
static kjson_value *parse_instant(parser_state *state);

/* Memory allocation helpers */
static void *state_alloc(parser_state *state, size_t size)
{
    return state->alloc ? state->alloc(size) : malloc(size);
}

static void state_free(parser_state *state, void *ptr)
{
    if (state->dealloc)
        state->dealloc(ptr);
    else
        free(ptr);
}

/* Error handling */
static void set_error(parser_state *state, kjson_error error)
{
    state->error = error;
}

/* Character helpers */
static bool is_hex_digit(char c)
{
    return (c >= '0' && c <= '9') || 
           (c >= 'a' && c <= 'f') || 
           (c >= 'A' && c <= 'F');
}

static int hex_value(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

/* Skip whitespace */
static void skip_whitespace(parser_state *state)
{
    while (state->position < state->length) {
        char c = state->input[state->position];
        if (c == ' ' || c == '\t' || c == '\r') {
            state->position++;
            state->column++;
        } else if (c == '\n') {
            state->position++;
            state->line++;
            state->column = 1;
        } else {
            break;
        }
    }
}

/* Skip single-line comment */
static bool skip_single_line_comment(parser_state *state)
{
    if (state->position + 1 < state->length &&
        state->input[state->position] == '/' &&
        state->input[state->position + 1] == '/') {
        
        state->position += 2;
        state->column += 2;
        
        while (state->position < state->length && 
               state->input[state->position] != '\n') {
            state->position++;
            state->column++;
        }
        
        if (state->position < state->length) {
            state->position++; // Skip newline
            state->line++;
            state->column = 1;
        }
        
        return true;
    }
    return false;
}

/* Skip multi-line comment */
static bool skip_multi_line_comment(parser_state *state)
{
    if (state->position + 1 < state->length &&
        state->input[state->position] == '/' &&
        state->input[state->position + 1] == '*') {
        
        state->position += 2;
        state->column += 2;
        
        while (state->position + 1 < state->length) {
            if (state->input[state->position] == '*' &&
                state->input[state->position + 1] == '/') {
                state->position += 2;
                state->column += 2;
                return true;
            }
            
            if (state->input[state->position] == '\n') {
                state->line++;
                state->column = 1;
            } else {
                state->column++;
            }
            state->position++;
        }
        
        set_error(state, KJSON_ERROR_INCOMPLETE);
        return false;
    }
    return false;
}

/* Skip comment */
static bool skip_comment(parser_state *state)
{
    if (!state->options->allow_comments)
        return false;
        
    return skip_single_line_comment(state) || skip_multi_line_comment(state);
}

/* Skip whitespace and comments */
static void skip_whitespace_and_comments(parser_state *state)
{
    bool changed;
    do {
        changed = false;
        size_t old_pos = state->position;
        skip_whitespace(state);
        if (state->position != old_pos)
            changed = true;
            
        if (skip_comment(state))
            changed = true;
    } while (changed);
}

/* Value creation helpers */
static kjson_value *create_value(parser_state *state, kjson_type type)
{
    kjson_value *value = (kjson_value *)state_alloc(state, sizeof(kjson_value));
    if (!value) {
        set_error(state, KJSON_ERROR_MEMORY);
        return NULL;
    }
    memset(value, 0, sizeof(kjson_value));
    value->type = type;
    return value;
}

/* Parse string */
static kjson_value *parse_string(parser_state *state)
{
    if (state->position >= state->length) {
        set_error(state, KJSON_ERROR_INCOMPLETE);
        return NULL;
    }
    
    char quote = state->input[state->position];
    if (quote != '"' && quote != '\'' && quote != '`') {
        set_error(state, KJSON_ERROR_INVALID_STRING);
        return NULL;
    }
    
    state->position++;
    state->column++;
    
    /* size_t start = state->position; // unused */
    size_t capacity = 64;
    char *buffer = (char *)state_alloc(state, capacity);
    if (!buffer) {
        set_error(state, KJSON_ERROR_MEMORY);
        return NULL;
    }
    
    size_t length = 0;
    
    while (state->position < state->length) {
        char c = state->input[state->position];
        
        if (c == quote) {
            state->position++;
            state->column++;
            
            /* Create string value */
            kjson_value *value = create_value(state, KJSON_TYPE_STRING);
            if (!value) {
                state_free(state, buffer);
                return NULL;
            }
            
            /* Null terminate and assign */
            buffer[length] = '\0';
            value->u.string = buffer;
            
            return value;
        }
        
        if (c == '\\') {
            state->position++;
            state->column++;
            
            if (state->position >= state->length) {
                state_free(state, buffer);
                set_error(state, KJSON_ERROR_INCOMPLETE);
                return NULL;
            }
            
            c = state->input[state->position];
            char escaped;
            
            switch (c) {
                case '"':  escaped = '"'; break;
                case '\'': escaped = '\''; break;
                case '`':  escaped = '`'; break;
                case '\\': escaped = '\\'; break;
                case '/':  escaped = '/'; break;
                case 'b':  escaped = '\b'; break;
                case 'f':  escaped = '\f'; break;
                case 'n':  escaped = '\n'; break;
                case 'r':  escaped = '\r'; break;
                case 't':  escaped = '\t'; break;
                case 'u': {
                    /* Unicode escape */
                    if (state->position + 4 >= state->length) {
                        state_free(state, buffer);
                        set_error(state, KJSON_ERROR_INCOMPLETE);
                        return NULL;
                    }
                    
                    int code = 0;
                    for (int i = 0; i < 4; i++) {
                        state->position++;
                        state->column++;
                        char h = state->input[state->position];
                        if (!is_hex_digit(h)) {
                            state_free(state, buffer);
                            set_error(state, KJSON_ERROR_INVALID_ESCAPE);
                            return NULL;
                        }
                        code = (code << 4) | hex_value(h);
                    }
                    
                    /* Convert to UTF-8 */
                    if (code <= 0x7F) {
                        escaped = (char)code;
                    } else {
                        /* TODO: Proper UTF-8 encoding */
                        escaped = '?';
                    }
                    break;
                }
                default:
                    state_free(state, buffer);
                    set_error(state, KJSON_ERROR_INVALID_ESCAPE);
                    return NULL;
            }
            
            c = escaped;
        }
        
        /* Grow buffer if needed */
        if (length >= capacity - 1) {
            capacity *= 2;
            if (capacity > state->options->max_string_length) {
                state_free(state, buffer);
                set_error(state, KJSON_ERROR_SIZE_EXCEEDED);
                return NULL;
            }
            
            char *new_buffer = (char *)state_alloc(state, capacity);
            if (!new_buffer) {
                state_free(state, buffer);
                set_error(state, KJSON_ERROR_MEMORY);
                return NULL;
            }
            
            memcpy(new_buffer, buffer, length);
            state_free(state, buffer);
            buffer = new_buffer;
        }
        
        buffer[length++] = c;
        state->position++;
        
        if (c == '\n') {
            state->line++;
            state->column = 1;
        } else {
            state->column++;
        }
    }
    
    state_free(state, buffer);
    set_error(state, KJSON_ERROR_INCOMPLETE);
    return NULL;
}

/* Parse number or BigInt */
static kjson_value *parse_number(parser_state *state)
{
    size_t start = state->position;
    bool negative = false;
    /* bool has_decimal = false; // unused */
    /* bool has_exponent = false; // unused */
    
    /* Check for negative sign */
    if (state->position < state->length && state->input[state->position] == '-') {
        negative = true;
        state->position++;
        state->column++;
    }
    
    /* Parse integer part */
    if (state->position >= state->length || !isdigit(state->input[state->position])) {
        set_error(state, KJSON_ERROR_INVALID_NUMBER);
        return NULL;
    }
    
    /* Skip digits */
    while (state->position < state->length && isdigit(state->input[state->position])) {
        state->position++;
        state->column++;
    }
    
    /* Check for decimal point */
    if (state->position < state->length && state->input[state->position] == '.') {
        /* has_decimal = true; // unused */
        state->position++;
        state->column++;
        
        /* Must have digits after decimal */
        if (state->position >= state->length || !isdigit(state->input[state->position])) {
            set_error(state, KJSON_ERROR_INVALID_NUMBER);
            return NULL;
        }
        
        while (state->position < state->length && isdigit(state->input[state->position])) {
            state->position++;
            state->column++;
        }
    }
    
    /* Check for exponent */
    if (state->position < state->length && 
        (state->input[state->position] == 'e' || state->input[state->position] == 'E')) {
        /* has_exponent = true; // unused */
        state->position++;
        state->column++;
        
        /* Optional sign */
        if (state->position < state->length && 
            (state->input[state->position] == '+' || state->input[state->position] == '-')) {
            state->position++;
            state->column++;
        }
        
        /* Must have digits */
        if (state->position >= state->length || !isdigit(state->input[state->position])) {
            set_error(state, KJSON_ERROR_INVALID_NUMBER);
            return NULL;
        }
        
        while (state->position < state->length && isdigit(state->input[state->position])) {
            state->position++;
            state->column++;
        }
    }
    
    /* Check for type suffixes */
    bool is_bigint = false;
    bool is_decimal128 = false;
    
    if (state->position < state->length) {
        if (state->input[state->position] == 'n') {
            is_bigint = true;
            state->position++;
            state->column++;
        } else if (state->input[state->position] == 'm') {
            is_decimal128 = true;
            state->position++;
            state->column++;
        }
    }
    
    /* Extract number string */
    size_t length = state->position - start;
    char *num_str = (char *)state_alloc(state, length + 1);
    if (!num_str) {
        set_error(state, KJSON_ERROR_MEMORY);
        return NULL;
    }
    
    memcpy(num_str, state->input + start, length);
    num_str[length] = '\0';
    
    /* Create appropriate value type */
    kjson_value *value = NULL;
    
    if (is_bigint) {
        /* Remove 'n' suffix */
        num_str[length - 1] = '\0';
        
        value = create_value(state, KJSON_TYPE_BIGINT);
        if (!value) {
            state_free(state, num_str);
            return NULL;
        }
        
        value->u.bigint = (kjson_bigint *)state_alloc(state, sizeof(kjson_bigint));
        if (!value->u.bigint) {
            state_free(state, value);
            state_free(state, num_str);
            set_error(state, KJSON_ERROR_MEMORY);
            return NULL;
        }
        
        value->u.bigint->negative = negative;
        value->u.bigint->digits = num_str;
        
    } else if (is_decimal128) {
        /* Remove 'm' suffix */
        num_str[length - 1] = '\0';
        
        value = create_value(state, KJSON_TYPE_DECIMAL128);
        if (!value) {
            state_free(state, num_str);
            return NULL;
        }
        
        value->u.decimal = (kjson_decimal128 *)state_alloc(state, sizeof(kjson_decimal128));
        if (!value->u.decimal) {
            state_free(state, value);
            state_free(state, num_str);
            set_error(state, KJSON_ERROR_MEMORY);
            return NULL;
        }
        
        /* TODO: Parse decimal properly with exponent */
        value->u.decimal->negative = negative;
        value->u.decimal->digits = num_str;
        value->u.decimal->exponent = 0;
        
    } else {
        /* Regular number */
        value = create_value(state, KJSON_TYPE_NUMBER);
        if (!value) {
            state_free(state, num_str);
            return NULL;
        }
        
        char *end;
        errno = 0;
        double d = strtod(num_str, &end);
        
        if (errno == ERANGE || (state->options->strict_numbers && (!isfinite(d)))) {
            state_free(state, value);
            state_free(state, num_str);
            set_error(state, KJSON_ERROR_OVERFLOW);
            return NULL;
        }
        
        value->u.number = d;
        state_free(state, num_str);
    }
    
    return value;
}

/* Parse boolean */
static kjson_value *parse_boolean(parser_state *state, bool value)
{
    kjson_value *v = create_value(state, KJSON_TYPE_BOOLEAN);
    if (!v) return NULL;
    v->u.boolean = value;
    return v;
}

/* Parse literal (true, false, null) */
static kjson_value *parse_literal(parser_state *state, const char *literal, kjson_value *value)
{
    size_t len = strlen(literal);
    
    if (state->position + len > state->length) {
        set_error(state, KJSON_ERROR_INCOMPLETE);
        return NULL;
    }
    
    if (strncmp(state->input + state->position, literal, len) != 0) {
        set_error(state, KJSON_ERROR_UNEXPECTED_TOKEN);
        return NULL;
    }
    
    state->position += len;
    state->column += len;
    
    return value;
}

/* Check if UUID format */
static bool is_uuid_format(parser_state *state)
{
    /* UUID format: 8-4-4-4-12 hex digits */
    size_t pos = state->position;
    
    /* 8 hex digits */
    for (int i = 0; i < 8; i++) {
        if (pos >= state->length || !is_hex_digit(state->input[pos]))
            return false;
        pos++;
    }
    
    if (pos >= state->length || state->input[pos] != '-')
        return false;
    pos++;
    
    /* 4 hex digits */
    for (int i = 0; i < 4; i++) {
        if (pos >= state->length || !is_hex_digit(state->input[pos]))
            return false;
        pos++;
    }
    
    if (pos >= state->length || state->input[pos] != '-')
        return false;
    pos++;
    
    /* 4 hex digits */
    for (int i = 0; i < 4; i++) {
        if (pos >= state->length || !is_hex_digit(state->input[pos]))
            return false;
        pos++;
    }
    
    if (pos >= state->length || state->input[pos] != '-')
        return false;
    pos++;
    
    /* 4 hex digits */
    for (int i = 0; i < 4; i++) {
        if (pos >= state->length || !is_hex_digit(state->input[pos]))
            return false;
        pos++;
    }
    
    if (pos >= state->length || state->input[pos] != '-')
        return false;
    pos++;
    
    /* 12 hex digits */
    for (int i = 0; i < 12; i++) {
        if (pos >= state->length || !is_hex_digit(state->input[pos]))
            return false;
        pos++;
    }
    
    return true;
}

/* Parse UUID */
static kjson_value *parse_uuid(parser_state *state)
{
    if (!is_uuid_format(state)) {
        set_error(state, KJSON_ERROR_INVALID_UUID);
        return NULL;
    }
    
    kjson_value *value = create_value(state, KJSON_TYPE_UUID);
    if (!value) return NULL;
    
    value->u.uuid = (kjson_uuid *)state_alloc(state, sizeof(kjson_uuid));
    if (!value->u.uuid) {
        state_free(state, value);
        set_error(state, KJSON_ERROR_MEMORY);
        return NULL;
    }
    
    /* Parse UUID bytes */
    size_t byte_idx = 0;
    
    for (int group = 0; group < 5; group++) {
        int digits;
        switch (group) {
            case 0: digits = 8; break;
            case 1:
            case 2:
            case 3: digits = 4; break;
            case 4: digits = 12; break;
        }
        
        for (int i = 0; i < digits; i += 2) {
            int high = hex_value(state->input[state->position]);
            state->position++;
            state->column++;
            
            int low = hex_value(state->input[state->position]);
            state->position++;
            state->column++;
            
            value->u.uuid->bytes[byte_idx++] = (high << 4) | low;
        }
        
        if (group < 4) {
            state->position++; /* Skip '-' */
            state->column++;
        }
    }
    
    return value;
}

/* Check if instant format (ISO 8601) */
static bool is_instant_start(parser_state *state)
{
    if (!state->options->parse_instants)
        return false;
        
    /* Simple check for YYYY-MM-DD format start */
    if (state->position + 10 > state->length)
        return false;
        
    return isdigit(state->input[state->position]) &&
           isdigit(state->input[state->position + 1]) &&
           isdigit(state->input[state->position + 2]) &&
           isdigit(state->input[state->position + 3]) &&
           state->input[state->position + 4] == '-' &&
           isdigit(state->input[state->position + 5]) &&
           isdigit(state->input[state->position + 6]) &&
           state->input[state->position + 7] == '-' &&
           isdigit(state->input[state->position + 8]) &&
           isdigit(state->input[state->position + 9]);
}

/* Parse ISO instant */
static kjson_value *parse_instant(parser_state *state)
{
    /* TODO: Implement proper ISO 8601 parsing */
    /* For now, parse as string and validate format */
    
    size_t start = state->position;
    
    /* Skip date part YYYY-MM-DD */
    state->position += 10;
    state->column += 10;
    
    /* Check for time part */
    if (state->position < state->length && state->input[state->position] == 'T') {
        state->position++;
        state->column++;
        
        /* Skip time HH:MM:SS */
        while (state->position < state->length) {
            char c = state->input[state->position];
            if (c == 'Z' || c == '+' || c == '-' || c == ' ' || 
                c == ',' || c == '}' || c == ']') {
                break;
            }
            state->position++;
            state->column++;
        }
        
        /* Skip timezone */
        if (state->position < state->length && state->input[state->position] == 'Z') {
            state->position++;
            state->column++;
        }
    }
    
    /* Extract instant string */
    size_t length = state->position - start;
    char *instant_str = (char *)state_alloc(state, length + 1);
    if (!instant_str) {
        set_error(state, KJSON_ERROR_MEMORY);
        return NULL;
    }
    
    memcpy(instant_str, state->input + start, length);
    instant_str[length] = '\0';
    
    /* Create instant value */
    kjson_value *value = create_value(state, KJSON_TYPE_INSTANT);
    if (!value) {
        state_free(state, instant_str);
        return NULL;
    }
    
    value->u.instant = (kjson_instant *)state_alloc(state, sizeof(kjson_instant));
    if (!value->u.instant) {
        state_free(state, value);
        state_free(state, instant_str);
        set_error(state, KJSON_ERROR_MEMORY);
        return NULL;
    }
    
    /* TODO: Convert to nanoseconds */
    value->u.instant->nanoseconds = 0;
    
    state_free(state, instant_str);
    return value;
}

/* Parse array */
static kjson_value *parse_array(parser_state *state)
{
    if (state->position >= state->length || state->input[state->position] != '[') {
        set_error(state, KJSON_ERROR_UNEXPECTED_TOKEN);
        return NULL;
    }
    
    state->position++;
    state->column++;
    state->depth++;
    
    if (state->depth > state->options->max_depth) {
        set_error(state, KJSON_ERROR_DEPTH_EXCEEDED);
        return NULL;
    }
    
    kjson_value *array = create_value(state, KJSON_TYPE_ARRAY);
    if (!array) return NULL;
    
    array->u.array.capacity = 4;
    array->u.array.items = (kjson_value **)state_alloc(state, 
        sizeof(kjson_value *) * array->u.array.capacity);
    if (!array->u.array.items) {
        kjson_free(array);
        set_error(state, KJSON_ERROR_MEMORY);
        return NULL;
    }
    
    skip_whitespace_and_comments(state);
    
    /* Empty array */
    if (state->position < state->length && state->input[state->position] == ']') {
        state->position++;
        state->column++;
        state->depth--;
        return array;
    }
    
    while (true) {
        /* Parse value */
        kjson_value *value = parse_value(state);
        if (!value) {
            kjson_free(array);
            return NULL;
        }
        
        /* Grow array if needed */
        if (array->u.array.count >= array->u.array.capacity) {
            size_t new_capacity = array->u.array.capacity * 2;
            kjson_value **new_items = (kjson_value **)state_alloc(state,
                sizeof(kjson_value *) * new_capacity);
            if (!new_items) {
                kjson_free(value);
                kjson_free(array);
                set_error(state, KJSON_ERROR_MEMORY);
                return NULL;
            }
            
            memcpy(new_items, array->u.array.items, 
                   sizeof(kjson_value *) * array->u.array.count);
            state_free(state, array->u.array.items);
            array->u.array.items = new_items;
            array->u.array.capacity = new_capacity;
        }
        
        array->u.array.items[array->u.array.count++] = value;
        
        skip_whitespace_and_comments(state);
        
        if (state->position >= state->length) {
            kjson_free(array);
            set_error(state, KJSON_ERROR_INCOMPLETE);
            return NULL;
        }
        
        char c = state->input[state->position];
        
        if (c == ']') {
            state->position++;
            state->column++;
            state->depth--;
            return array;
        }
        
        if (c == ',') {
            state->position++;
            state->column++;
            skip_whitespace_and_comments(state);
            
            /* Check for trailing comma */
            if (state->position < state->length && 
                state->input[state->position] == ']') {
                if (state->options->allow_trailing_commas) {
                    state->position++;
                    state->column++;
                    state->depth--;
                    return array;
                } else {
                    kjson_free(array);
                    set_error(state, KJSON_ERROR_UNEXPECTED_TOKEN);
                    return NULL;
                }
            }
        } else {
            kjson_free(array);
            set_error(state, KJSON_ERROR_UNEXPECTED_TOKEN);
            return NULL;
        }
    }
}

/* Check if key needs quotes */
/* static bool needs_quotes(const char *key, size_t len) // unused
{
    if (len == 0) return true;
    
    // First character must be letter, underscore, or dollar
    if (!isalpha(key[0]) && key[0] != '_' && key[0] != '$')
        return true;
        
    // Rest can be alphanumeric, underscore, or dollar
    for (size_t i = 1; i < len; i++) {
        if (!isalnum(key[i]) && key[i] != '_' && key[i] != '$')
            return true;
    }
    
    // Check reserved words
    if ((len == 4 && strncmp(key, "true", 4) == 0) ||
        (len == 5 && strncmp(key, "false", 5) == 0) ||
        (len == 4 && strncmp(key, "null", 4) == 0)) {
        return true;
    }
    
    return false;
} */

/* Parse unquoted key */
static char *parse_unquoted_key(parser_state *state)
{
    size_t start = state->position;
    
    /* First character */
    if (state->position >= state->length ||
        (!isalpha(state->input[state->position]) && 
         state->input[state->position] != '_' &&
         state->input[state->position] != '$')) {
        set_error(state, KJSON_ERROR_UNEXPECTED_TOKEN);
        return NULL;
    }
    
    state->position++;
    state->column++;
    
    /* Rest of key */
    while (state->position < state->length) {
        char c = state->input[state->position];
        if (!isalnum(c) && c != '_' && c != '$')
            break;
        state->position++;
        state->column++;
    }
    
    size_t length = state->position - start;
    char *key = (char *)state_alloc(state, length + 1);
    if (!key) {
        set_error(state, KJSON_ERROR_MEMORY);
        return NULL;
    }
    
    memcpy(key, state->input + start, length);
    key[length] = '\0';
    
    return key;
}

/* Parse object */
static kjson_value *parse_object(parser_state *state)
{
    if (state->position >= state->length || state->input[state->position] != '{') {
        set_error(state, KJSON_ERROR_UNEXPECTED_TOKEN);
        return NULL;
    }
    
    state->position++;
    state->column++;
    state->depth++;
    
    if (state->depth > state->options->max_depth) {
        set_error(state, KJSON_ERROR_DEPTH_EXCEEDED);
        return NULL;
    }
    
    kjson_value *object = create_value(state, KJSON_TYPE_OBJECT);
    if (!object) return NULL;
    
    skip_whitespace_and_comments(state);
    
    /* Empty object */
    if (state->position < state->length && state->input[state->position] == '}') {
        state->position++;
        state->column++;
        state->depth--;
        return object;
    }
    
    kjson_member *last_member = NULL;
    
    while (true) {
        /* Parse key */
        char *key = NULL;
        
        skip_whitespace_and_comments(state);
        
        if (state->position >= state->length) {
            kjson_free(object);
            set_error(state, KJSON_ERROR_INCOMPLETE);
            return NULL;
        }
        
        if (state->input[state->position] == '"' || 
            state->input[state->position] == '\'' ||
            state->input[state->position] == '`') {
            /* Quoted key */
            kjson_value *key_value = parse_string(state);
            if (!key_value) {
                kjson_free(object);
                return NULL;
            }
            key = key_value->u.string;
            key_value->u.string = NULL;
            kjson_free(key_value);
        } else if (state->options->allow_unquoted_keys) {
            /* Unquoted key */
            key = parse_unquoted_key(state);
            if (!key) {
                kjson_free(object);
                return NULL;
            }
        } else {
            kjson_free(object);
            set_error(state, KJSON_ERROR_UNEXPECTED_TOKEN);
            return NULL;
        }
        
        skip_whitespace_and_comments(state);
        
        /* Expect colon */
        if (state->position >= state->length || state->input[state->position] != ':') {
            state_free(state, key);
            kjson_free(object);
            set_error(state, KJSON_ERROR_UNEXPECTED_TOKEN);
            return NULL;
        }
        
        state->position++;
        state->column++;
        
        skip_whitespace_and_comments(state);
        
        /* Parse value */
        kjson_value *value = parse_value(state);
        if (!value) {
            state_free(state, key);
            kjson_free(object);
            return NULL;
        }
        
        /* Create member */
        kjson_member *member = (kjson_member *)state_alloc(state, sizeof(kjson_member));
        if (!member) {
            state_free(state, key);
            kjson_free(value);
            kjson_free(object);
            set_error(state, KJSON_ERROR_MEMORY);
            return NULL;
        }
        
        member->key = key;
        member->value = value;
        member->next = NULL;
        
        /* Add to object */
        if (last_member) {
            last_member->next = member;
        } else {
            object->u.object.first = member;
        }
        last_member = member;
        object->u.object.count++;
        
        skip_whitespace_and_comments(state);
        
        if (state->position >= state->length) {
            kjson_free(object);
            set_error(state, KJSON_ERROR_INCOMPLETE);
            return NULL;
        }
        
        char c = state->input[state->position];
        
        if (c == '}') {
            state->position++;
            state->column++;
            state->depth--;
            return object;
        }
        
        if (c == ',') {
            state->position++;
            state->column++;
            skip_whitespace_and_comments(state);
            
            /* Check for trailing comma */
            if (state->position < state->length && 
                state->input[state->position] == '}') {
                if (state->options->allow_trailing_commas) {
                    state->position++;
                    state->column++;
                    state->depth--;
                    return object;
                } else {
                    kjson_free(object);
                    set_error(state, KJSON_ERROR_UNEXPECTED_TOKEN);
                    return NULL;
                }
            }
        } else {
            kjson_free(object);
            set_error(state, KJSON_ERROR_UNEXPECTED_TOKEN);
            return NULL;
        }
    }
}

/* Parse value */
static kjson_value *parse_value(parser_state *state)
{
    skip_whitespace_and_comments(state);
    
    if (state->position >= state->length) {
        set_error(state, KJSON_ERROR_INCOMPLETE);
        return NULL;
    }
    
    char c = state->input[state->position];
    
    /* Object */
    if (c == '{') {
        return parse_object(state);
    }
    
    /* Array */
    if (c == '[') {
        return parse_array(state);
    }
    
    /* String */
    if (c == '"' || c == '\'' || c == '`') {
        return parse_string(state);
    }
    
    /* Number, UUID, or Date - all can start with digits */
    if (c == '-' || (c >= '0' && c <= '9')) {
        /* Check if it's a UUID first (before number parsing) */
        /* This is crucial because UUIDs can start with digits and would
         * otherwise fail when the number parser encounters the first dash */
        if (is_uuid_format(state)) {
            return parse_uuid(state);
        }
        
        /* Check if it's an instant */
        if (is_instant_start(state)) {
            return parse_instant(state);
        }
        
        /* Finally, try parsing as number */
        return parse_number(state);
    }
    
    /* For UUIDs starting with letters (a-f), we still need this check */
    if ((c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F')) {
        if (is_uuid_format(state)) {
            return parse_uuid(state);
        }
    }
    
    /* Literals */
    if (c == 't') {
        kjson_value *v = parse_boolean(state, true);
        return parse_literal(state, "true", v);
    }
    
    if (c == 'f') {
        kjson_value *v = parse_boolean(state, false);
        return parse_literal(state, "false", v);
    }
    
    if (c == 'n') {
        kjson_value *v = create_value(state, KJSON_TYPE_NULL);
        return parse_literal(state, "null", v);
    }
    
    set_error(state, KJSON_ERROR_UNEXPECTED_TOKEN);
    return NULL;
}

/* Public API: Parse kJSON text */
kjson_value *kjson_parse(const char *text, size_t length, kjson_error *error)
{
    return kjson_parse_ex(text, length, &default_options, error);
}

kjson_value *kjson_parse_ex(const char *text, size_t length, 
                            const kjson_options *options, kjson_error *error)
{
    parser_state state = {
        .input = text,
        .length = length,
        .position = 0,
        .options = options ? options : &default_options,
        .depth = 0,
        .error = KJSON_OK,
        .line = 1,
        .column = 1,
        .alloc = options && options->allocator ? options->allocator : default_malloc,
        .dealloc = options && options->deallocator ? options->deallocator : default_free
    };
    
    kjson_value *value = parse_value(&state);
    
    if (value) {
        skip_whitespace_and_comments(&state);
        if (state.position < state.length) {
            kjson_free(value);
            set_error(&state, KJSON_ERROR_TRAILING_DATA);
            value = NULL;
        }
    }
    
    if (error) {
        *error = state.error;
    }
    
    return value;
}

/* Free a value tree */
void kjson_free(kjson_value *value)
{
    if (!value) return;
    
    switch (value->type) {
        case KJSON_TYPE_STRING:
            if (value->u.string) free(value->u.string);
            break;
            
        case KJSON_TYPE_BIGINT:
            if (value->u.bigint) {
                if (value->u.bigint->digits) free(value->u.bigint->digits);
                free(value->u.bigint);
            }
            break;
            
        case KJSON_TYPE_DECIMAL128:
            if (value->u.decimal) {
                if (value->u.decimal->digits) free(value->u.decimal->digits);
                free(value->u.decimal);
            }
            break;
            
        case KJSON_TYPE_UUID:
            if (value->u.uuid) free(value->u.uuid);
            break;
            
        case KJSON_TYPE_INSTANT:
            if (value->u.instant) free(value->u.instant);
            break;
            
        case KJSON_TYPE_DURATION:
            if (value->u.duration) free(value->u.duration);
            break;
            
        case KJSON_TYPE_ARRAY:
            if (value->u.array.items) {
                for (size_t i = 0; i < value->u.array.count; i++) {
                    kjson_free(value->u.array.items[i]);
                }
                free(value->u.array.items);
            }
            break;
            
        case KJSON_TYPE_OBJECT:
            {
                kjson_member *member = value->u.object.first;
                while (member) {
                    kjson_member *next = member->next;
                    if (member->key) free(member->key);
                    kjson_free(member->value);
                    free(member);
                    member = next;
                }
            }
            break;
            
        case KJSON_TYPE_BINARY:
            if (value->u.binary) free(value->u.binary);
            break;
            
        default:
            break;
    }
    
    free(value);
}

/* Error string */
const char *kjson_error_string(kjson_error error)
{
    switch (error) {
        case KJSON_OK: return "No error";
        case KJSON_ERROR_MEMORY: return "Out of memory";
        case KJSON_ERROR_SYNTAX: return "Syntax error";
        case KJSON_ERROR_UNEXPECTED_TOKEN: return "Unexpected token";
        case KJSON_ERROR_INVALID_NUMBER: return "Invalid number";
        case KJSON_ERROR_INVALID_STRING: return "Invalid string";
        case KJSON_ERROR_INVALID_UUID: return "Invalid UUID";
        case KJSON_ERROR_INVALID_DATE: return "Invalid instant";
        case KJSON_ERROR_INVALID_ESCAPE: return "Invalid escape sequence";
        case KJSON_ERROR_DEPTH_EXCEEDED: return "Maximum depth exceeded";
        case KJSON_ERROR_SIZE_EXCEEDED: return "Maximum size exceeded";
        case KJSON_ERROR_INVALID_UTF8: return "Invalid UTF-8";
        case KJSON_ERROR_TRAILING_DATA: return "Trailing data after value";
        case KJSON_ERROR_INCOMPLETE: return "Incomplete JSON";
        case KJSON_ERROR_UNSUPPORTED_TYPE: return "Unsupported type";
        case KJSON_ERROR_OVERFLOW: return "Number overflow";
        case KJSON_ERROR_INVALID_BINARY: return "Invalid binary format";
        default: return "Unknown error";
    }
}

/* Type checking helpers */
kjson_type kjson_get_type(const kjson_value *value)
{
    return value ? value->type : KJSON_TYPE_NULL;
}

bool kjson_is_null(const kjson_value *value)
{
    return value && value->type == KJSON_TYPE_NULL;
}

bool kjson_is_boolean(const kjson_value *value)
{
    return value && value->type == KJSON_TYPE_BOOLEAN;
}

bool kjson_is_number(const kjson_value *value)
{
    return value && value->type == KJSON_TYPE_NUMBER;
}

bool kjson_is_string(const kjson_value *value)
{
    return value && value->type == KJSON_TYPE_STRING;
}

bool kjson_is_array(const kjson_value *value)
{
    return value && value->type == KJSON_TYPE_ARRAY;
}

bool kjson_is_object(const kjson_value *value)
{
    return value && value->type == KJSON_TYPE_OBJECT;
}

bool kjson_is_bigint(const kjson_value *value)
{
    return value && value->type == KJSON_TYPE_BIGINT;
}

bool kjson_is_decimal128(const kjson_value *value)
{
    return value && value->type == KJSON_TYPE_DECIMAL128;
}

bool kjson_is_uuid(const kjson_value *value)
{
    return value && value->type == KJSON_TYPE_UUID;
}

bool kjson_is_instant(const kjson_value *value)
{
    return value && value->type == KJSON_TYPE_INSTANT;
}

bool kjson_is_duration(const kjson_value *value)
{
    return value && value->type == KJSON_TYPE_DURATION;
}

/* Value getters */
bool kjson_get_boolean(const kjson_value *value)
{
    return value && value->type == KJSON_TYPE_BOOLEAN ? value->u.boolean : false;
}

double kjson_get_number(const kjson_value *value)
{
    return value && value->type == KJSON_TYPE_NUMBER ? value->u.number : 0.0;
}

const char *kjson_get_string(const kjson_value *value)
{
    return value && value->type == KJSON_TYPE_STRING ? value->u.string : NULL;
}

/* Array operations */
size_t kjson_array_size(const kjson_value *array)
{
    return array && array->type == KJSON_TYPE_ARRAY ? array->u.array.count : 0;
}

kjson_value *kjson_array_get(const kjson_value *array, size_t index)
{
    if (!array || array->type != KJSON_TYPE_ARRAY || index >= array->u.array.count)
        return NULL;
    return array->u.array.items[index];
}

/* Object operations */
size_t kjson_object_size(const kjson_value *object)
{
    return object && object->type == KJSON_TYPE_OBJECT ? object->u.object.count : 0;
}

kjson_value *kjson_object_get(const kjson_value *object, const char *key)
{
    if (!object || object->type != KJSON_TYPE_OBJECT || !key)
        return NULL;
        
    kjson_member *member = object->u.object.first;
    while (member) {
        if (strcmp(member->key, key) == 0)
            return member->value;
        member = member->next;
    }
    
    return NULL;
}

bool kjson_object_has(const kjson_value *object, const char *key)
{
    return kjson_object_get(object, key) != NULL;
}

/* Value creation helpers */
kjson_value *kjson_create_null(void)
{
    kjson_value *v = malloc(sizeof(kjson_value));
    if (!v) return NULL;
    memset(v, 0, sizeof(kjson_value));
    v->type = KJSON_TYPE_NULL;
    return v;
}

kjson_value *kjson_create_boolean(bool value)
{
    kjson_value *v = malloc(sizeof(kjson_value));
    if (!v) return NULL;
    memset(v, 0, sizeof(kjson_value));
    v->type = KJSON_TYPE_BOOLEAN;
    v->u.boolean = value;
    return v;
}

kjson_value *kjson_create_number(double value)
{
    kjson_value *v = malloc(sizeof(kjson_value));
    if (!v) return NULL;
    memset(v, 0, sizeof(kjson_value));
    v->type = KJSON_TYPE_NUMBER;
    v->u.number = value;
    return v;
}

kjson_value *kjson_create_string(const char *str, size_t len)
{
    kjson_value *v = malloc(sizeof(kjson_value));
    if (!v) return NULL;
    memset(v, 0, sizeof(kjson_value));
    v->type = KJSON_TYPE_STRING;
    
    if (str && len > 0) {
        v->u.string = malloc(len + 1);
        if (!v->u.string) {
            free(v);
            return NULL;
        }
        memcpy(v->u.string, str, len);
        v->u.string[len] = '\0';
    } else {
        v->u.string = strdup("");
    }
    
    return v;
}

kjson_value *kjson_create_bigint(const char *digits, bool negative)
{
    kjson_value *v = malloc(sizeof(kjson_value));
    if (!v) return NULL;
    memset(v, 0, sizeof(kjson_value));
    v->type = KJSON_TYPE_BIGINT;
    
    v->u.bigint = malloc(sizeof(kjson_bigint));
    if (!v->u.bigint) {
        free(v);
        return NULL;
    }
    
    v->u.bigint->negative = negative;
    v->u.bigint->digits = strdup(digits ? digits : "0");
    if (!v->u.bigint->digits) {
        free(v->u.bigint);
        free(v);
        return NULL;
    }
    
    return v;
}

kjson_value *kjson_create_decimal128(const char *digits, int32_t exp, bool negative)
{
    kjson_value *v = malloc(sizeof(kjson_value));
    if (!v) return NULL;
    memset(v, 0, sizeof(kjson_value));
    v->type = KJSON_TYPE_DECIMAL128;
    
    v->u.decimal = malloc(sizeof(kjson_decimal128));
    if (!v->u.decimal) {
        free(v);
        return NULL;
    }
    
    v->u.decimal->negative = negative;
    v->u.decimal->exponent = exp;
    v->u.decimal->digits = strdup(digits ? digits : "0");
    if (!v->u.decimal->digits) {
        free(v->u.decimal);
        free(v);
        return NULL;
    }
    
    return v;
}

kjson_value *kjson_create_uuid(const kjson_uuid *uuid)
{
    kjson_value *v = malloc(sizeof(kjson_value));
    if (!v) return NULL;
    memset(v, 0, sizeof(kjson_value));
    v->type = KJSON_TYPE_UUID;
    
    v->u.uuid = malloc(sizeof(kjson_uuid));
    if (!v->u.uuid) {
        free(v);
        return NULL;
    }
    
    if (uuid) {
        memcpy(v->u.uuid->bytes, uuid->bytes, 16);
    } else {
        memset(v->u.uuid->bytes, 0, 16);
    }
    
    return v;
}

kjson_value *kjson_create_instant(int64_t nanoseconds)
{
    kjson_value *v = malloc(sizeof(kjson_value));
    if (!v) return NULL;
    memset(v, 0, sizeof(kjson_value));
    v->type = KJSON_TYPE_INSTANT;
    
    v->u.instant = malloc(sizeof(kjson_instant));
    if (!v->u.instant) {
        free(v);
        return NULL;
    }
    
    v->u.instant->nanoseconds = nanoseconds;
    
    return v;
}

kjson_value *kjson_create_duration(int64_t nanoseconds)
{
    kjson_value *v = malloc(sizeof(kjson_value));
    if (!v) return NULL;
    memset(v, 0, sizeof(kjson_value));
    v->type = KJSON_TYPE_DURATION;
    
    v->u.duration = malloc(sizeof(kjson_duration));
    if (!v->u.duration) {
        free(v);
        return NULL;
    }
    
    v->u.duration->nanoseconds = nanoseconds;
    
    return v;
}

kjson_value *kjson_create_array(void)
{
    kjson_value *v = malloc(sizeof(kjson_value));
    if (!v) return NULL;
    memset(v, 0, sizeof(kjson_value));
    v->type = KJSON_TYPE_ARRAY;
    
    v->u.array.capacity = 4;
    v->u.array.count = 0;
    v->u.array.items = malloc(sizeof(kjson_value *) * v->u.array.capacity);
    if (!v->u.array.items) {
        free(v);
        return NULL;
    }
    
    return v;
}

kjson_value *kjson_create_object(void)
{
    kjson_value *v = malloc(sizeof(kjson_value));
    if (!v) return NULL;
    memset(v, 0, sizeof(kjson_value));
    v->type = KJSON_TYPE_OBJECT;
    v->u.object.first = NULL;
    v->u.object.count = 0;
    return v;
}

/* Array operations */
kjson_error kjson_array_append(kjson_value *array, kjson_value *value)
{
    if (!array || array->type != KJSON_TYPE_ARRAY || !value)
        return KJSON_ERROR_INVALID_BINARY;
    
    /* Grow array if needed */
    if (array->u.array.count >= array->u.array.capacity) {
        size_t new_capacity = array->u.array.capacity * 2;
        kjson_value **new_items = realloc(array->u.array.items,
                                         sizeof(kjson_value *) * new_capacity);
        if (!new_items) return KJSON_ERROR_MEMORY;
        
        array->u.array.items = new_items;
        array->u.array.capacity = new_capacity;
    }
    
    array->u.array.items[array->u.array.count++] = value;
    return KJSON_OK;
}

/* Object operations */
kjson_error kjson_object_set(kjson_value *object, const char *key, kjson_value *value)
{
    if (!object || object->type != KJSON_TYPE_OBJECT || !key || !value)
        return KJSON_ERROR_INVALID_BINARY;
    
    /* Check if key already exists */
    kjson_member *member = object->u.object.first;
    while (member) {
        if (strcmp(member->key, key) == 0) {
            /* Replace existing value */
            kjson_free(member->value);
            member->value = value;
            return KJSON_OK;
        }
        member = member->next;
    }
    
    /* Add new member */
    kjson_member *new_member = malloc(sizeof(kjson_member));
    if (!new_member) return KJSON_ERROR_MEMORY;
    
    new_member->key = strdup(key);
    if (!new_member->key) {
        free(new_member);
        return KJSON_ERROR_MEMORY;
    }
    
    new_member->value = value;
    new_member->next = object->u.object.first;
    object->u.object.first = new_member;
    object->u.object.count++;
    
    return KJSON_OK;
}

/* Extended getters */
const kjson_bigint *kjson_get_bigint(const kjson_value *value)
{
    return value && value->type == KJSON_TYPE_BIGINT ? value->u.bigint : NULL;
}

const kjson_decimal128 *kjson_get_decimal128(const kjson_value *value)
{
    return value && value->type == KJSON_TYPE_DECIMAL128 ? value->u.decimal : NULL;
}

const kjson_uuid *kjson_get_uuid(const kjson_value *value)
{
    return value && value->type == KJSON_TYPE_UUID ? value->u.uuid : NULL;
}

const kjson_instant *kjson_get_instant(const kjson_value *value)
{
    return value && value->type == KJSON_TYPE_INSTANT ? value->u.instant : NULL;
}

const kjson_duration *kjson_get_duration(const kjson_value *value)
{
    return value && value->type == KJSON_TYPE_DURATION ? value->u.duration : NULL;
}

/* Version string */
const char *kjson_version_string(void)
{
    return "1.0.0";
}