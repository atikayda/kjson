/*
 * kjson_parser.c - Native PostgreSQL implementation of kJSON parser
 * 
 * This implementation uses PostgreSQL's memory management throughout
 * to avoid allocator mismatches.
 */

#include "postgres.h"
#include "fmgr.h"
#include "utils/builtins.h"
#include "utils/memutils.h"
#include "lib/stringinfo.h"
#include "kjson_internal.h"

#include <ctype.h>
#include <math.h>

/* Public function prototype */
kjson_value *pg_kjson_parse(const char *input, size_t length, MemoryContext memctx);

/* Parser state */
typedef struct {
    const char *input;
    size_t length;
    size_t position;
    MemoryContext memctx;
} parser_state;

/* Forward declarations */
static kjson_value *parse_value(parser_state *state);
static kjson_value *parse_null(parser_state *state);
static kjson_value *parse_boolean(parser_state *state);
static kjson_value *parse_number(parser_state *state);
static kjson_value *parse_string(parser_state *state);
static kjson_value *parse_array(parser_state *state);
static kjson_value *parse_object(parser_state *state);
static void skip_whitespace(parser_state *state);
static void skip_comment(parser_state *state);
static bool is_date_string(const char *str, size_t len);
static bool is_uuid_string(const char *str, size_t len);

/* Allocate memory in parser context */
static void *
parser_alloc(parser_state *state, size_t size)
{
    return MemoryContextAlloc(state->memctx, size);
}

/* Skip whitespace and comments */
static void
skip_whitespace(parser_state *state)
{
    char c;
    
    while (state->position < state->length) {
        c = state->input[state->position];
        
        if (isspace(c)) {
            state->position++;
            continue;
        }
        
        /* Single-line comment */
        if (c == '/' && state->position + 1 < state->length && 
            state->input[state->position + 1] == '/') {
            skip_comment(state);
            continue;
        }
        
        /* Multi-line comment */
        if (c == '/' && state->position + 1 < state->length && 
            state->input[state->position + 1] == '*') {
            state->position += 2;
            while (state->position + 1 < state->length) {
                if (state->input[state->position] == '*' && 
                    state->input[state->position + 1] == '/') {
                    state->position += 2;
                    break;
                }
                state->position++;
            }
            continue;
        }
        
        break;
    }
}

/* Skip single-line comment */
static void
skip_comment(parser_state *state)
{
    state->position += 2; /* Skip // */
    while (state->position < state->length && 
           state->input[state->position] != '\n') {
        state->position++;
    }
}

/* Parse null */
static kjson_value *
parse_null(parser_state *state)
{
    kjson_value *value;
    
    if (state->position + 4 <= state->length &&
        strncmp(state->input + state->position, "null", 4) == 0) {
        state->position += 4;
        
        value = parser_alloc(state, sizeof(kjson_value));
        value->type = KJSON_TYPE_NULL;
        return value;
    }
    return NULL;
}

/* Parse boolean */
static kjson_value *
parse_boolean(parser_state *state)
{
    kjson_value *value = NULL;
    
    if (state->position + 4 <= state->length &&
        strncmp(state->input + state->position, "true", 4) == 0) {
        state->position += 4;
        value = parser_alloc(state, sizeof(kjson_value));
        value->type = KJSON_TYPE_BOOLEAN;
        value->u.boolean = true;
    }
    else if (state->position + 5 <= state->length &&
             strncmp(state->input + state->position, "false", 5) == 0) {
        state->position += 5;
        value = parser_alloc(state, sizeof(kjson_value));
        value->type = KJSON_TYPE_BOOLEAN;
        value->u.boolean = false;
    }
    
    return value;
}

/* Parse number (including BigInt and Decimal128) */
static kjson_value *
parse_number(parser_state *state)
{
    size_t start;
    bool negative;
    /* bool has_decimal; // unused */
    /* bool has_exponent; // unused */
    bool is_bigint;
    bool is_decimal;
    size_t len;
    char *num_str;
    kjson_value *value;
    char *dot;
    char *exp;
    
    start = state->position;
    negative = false;
    /* has_decimal = false; // unused */
    /* has_exponent = false; // unused */
    
    /* Optional minus */
    if (state->input[state->position] == '-') {
        negative = true;
        state->position++;
    }
    
    /* Integer part */
    if (!isdigit(state->input[state->position])) {
        state->position = start;
        return NULL;
    }
    
    while (state->position < state->length && 
           isdigit(state->input[state->position])) {
        state->position++;
    }
    
    /* Decimal part */
    if (state->position < state->length && 
        state->input[state->position] == '.') {
        /* has_decimal = true; // unused */
        state->position++;
        
        if (!isdigit(state->input[state->position])) {
            state->position = start;
            return NULL;
        }
        
        while (state->position < state->length && 
               isdigit(state->input[state->position])) {
            state->position++;
        }
    }
    
    /* Exponent part */
    if (state->position < state->length && 
        (state->input[state->position] == 'e' || 
         state->input[state->position] == 'E')) {
        /* has_exponent = true; // unused */
        state->position++;
        
        if (state->input[state->position] == '+' || 
            state->input[state->position] == '-') {
            state->position++;
        }
        
        if (!isdigit(state->input[state->position])) {
            state->position = start;
            return NULL;
        }
        
        while (state->position < state->length && 
               isdigit(state->input[state->position])) {
            state->position++;
        }
    }
    
    /* Check for type suffixes */
    is_bigint = false;
    is_decimal = false;
    
    if (state->position < state->length) {
        if (state->input[state->position] == 'n') {
            is_bigint = true;
            state->position++;
        }
        else if (state->input[state->position] == 'm') {
            is_decimal = true;
            state->position++;
        }
    }
    
    /* Extract number string */
    len = state->position - start;
    if (is_bigint) len--; /* Remove 'n' suffix */
    if (is_decimal) len--; /* Remove 'm' suffix */
    
    num_str = parser_alloc(state, len + 1);
    memcpy(num_str, state->input + start, len);
    num_str[len] = '\0';
    
    value = parser_alloc(state, sizeof(kjson_value));
    
    if (is_bigint) {
        value->type = KJSON_TYPE_BIGINT;
        value->u.bigint = parser_alloc(state, sizeof(kjson_bigint));
        value->u.bigint->negative = negative;
        value->u.bigint->digits = num_str;
        if (negative) {
            value->u.bigint->digits++; /* Skip minus sign */
        }
    }
    else if (is_decimal) {
        value->type = KJSON_TYPE_DECIMAL128;
        value->u.decimal = parser_alloc(state, sizeof(kjson_decimal128));
        value->u.decimal->negative = negative;
        
        /* Parse mantissa and exponent */
        dot = strchr(num_str, '.');
        exp = strchr(num_str, 'e');
        if (!exp) exp = strchr(num_str, 'E');
        
        if (exp) {
            *exp = '\0';
            value->u.decimal->exp = atoi(exp + 1);
        } else {
            value->u.decimal->exp = 0;
        }
        
        /* Remove decimal point and adjust exponent */
        if (dot) {
            size_t frac_len = strlen(dot + 1);
            if (exp) {
                frac_len = (exp - dot - 1);
            }
            value->u.decimal->exp -= frac_len;
            
            /* Remove decimal point */
            memmove(dot, dot + 1, strlen(dot + 1) + 1);
        }
        
        value->u.decimal->digits = num_str;
        if (negative) {
            value->u.decimal->digits++; /* Skip minus sign */
        }
    }
    else {
        value->type = KJSON_TYPE_NUMBER;
        value->u.number = strtod(num_str, NULL);
    }
    
    return value;
}

/* Parse string */
static kjson_value *
parse_string(parser_state *state)
{
    char quote;
    char c;
    StringInfoData buf;
    kjson_value *value;
    unsigned char *bytes;
    int byte_idx;
    int high, low;
    
    if (state->position >= state->length) {
        return NULL;
    }
    
    quote = state->input[state->position];
    if (quote != '"' && quote != '\'' && quote != '`') {
        return NULL;
    }
    
    state->position++;
    
    initStringInfo(&buf);
    
    while (state->position < state->length) {
        c = state->input[state->position];
        
        if (c == quote) {
            state->position++;
            break;
        }
        
        if (c == '\\' && state->position + 1 < state->length) {
            state->position++;
            c = state->input[state->position];
            
            switch (c) {
                case '"':
                case '\'':
                case '`':
                case '\\':
                case '/':
                    appendStringInfoChar(&buf, c);
                    break;
                case 'b':
                    appendStringInfoChar(&buf, '\b');
                    break;
                case 'f':
                    appendStringInfoChar(&buf, '\f');
                    break;
                case 'n':
                    appendStringInfoChar(&buf, '\n');
                    break;
                case 'r':
                    appendStringInfoChar(&buf, '\r');
                    break;
                case 't':
                    appendStringInfoChar(&buf, '\t');
                    break;
                case 'u':
                    /* Unicode escape - simplified handling */
                    if (state->position + 4 < state->length) {
                        state->position += 4;
                        appendStringInfoChar(&buf, '?'); /* Placeholder */
                    }
                    break;
                default:
                    appendStringInfoChar(&buf, c);
            }
        }
        else {
            appendStringInfoChar(&buf, c);
        }
        
        state->position++;
    }
    
    /* Check for special string types */
    value = parser_alloc(state, sizeof(kjson_value));
    
    if (is_uuid_string(buf.data, buf.len)) {
        value->type = KJSON_TYPE_UUID;
        value->u.uuid = parser_alloc(state, sizeof(kjson_uuid));
        
        /* Simple UUID parsing */
        bytes = value->u.uuid->bytes;
        byte_idx = 0;
        
        for (int i = 0; i < buf.len && byte_idx < 16; i++) {
            if (buf.data[i] == '-') continue;
            
            high = (buf.data[i] >= 'a') ? (buf.data[i] - 'a' + 10) :
                       (buf.data[i] >= 'A') ? (buf.data[i] - 'A' + 10) :
                       (buf.data[i] - '0');
            i++;
            low = (buf.data[i] >= 'a') ? (buf.data[i] - 'a' + 10) :
                      (buf.data[i] >= 'A') ? (buf.data[i] - 'A' + 10) :
                      (buf.data[i] - '0');
            
            bytes[byte_idx++] = (high << 4) | low;
        }
    }
    else if (is_date_string(buf.data, buf.len)) {
        value->type = KJSON_TYPE_INSTANT;
        value->u.instant = parser_alloc(state, sizeof(kjson_instant));
        
        /* Simple ISO date parsing - just store the timestamp */
        value->u.instant->nanoseconds = 0; /* Would need proper parsing */
        value->u.instant->tz_offset = 0;
    }
    else {
        value->type = KJSON_TYPE_STRING;
        value->u.string = parser_alloc(state, buf.len + 1);
        memcpy(value->u.string, buf.data, buf.len);
        value->u.string[buf.len] = '\0';
    }
    
    pfree(buf.data);
    return value;
}

/* Parse array */
static kjson_value *
parse_array(parser_state *state)
{
    kjson_value *array;
    char c;
    
    if (state->position >= state->length || 
        state->input[state->position] != '[') {
        return NULL;
    }
    
    state->position++;
    
    array = parser_alloc(state, sizeof(kjson_value));
    array->type = KJSON_TYPE_ARRAY;
    array->u.array.count = 0;
    array->u.array.capacity = 4;
    array->u.array.items = parser_alloc(state, 
        sizeof(kjson_value *) * array->u.array.capacity);
    
    skip_whitespace(state);
    
    /* Empty array */
    if (state->position < state->length && 
        state->input[state->position] == ']') {
        state->position++;
        return array;
    }
    
    while (true) {
        kjson_value *value;
        size_t new_capacity;
        kjson_value **new_items;
        
        value = parse_value(state);
        if (!value) {
            return NULL;
        }
        
        /* Grow array if needed */
        if (array->u.array.count >= array->u.array.capacity) {
            new_capacity = array->u.array.capacity * 2;
            new_items = parser_alloc(state,
                sizeof(kjson_value *) * new_capacity);
            memcpy(new_items, array->u.array.items,
                   sizeof(kjson_value *) * array->u.array.count);
            array->u.array.items = new_items;
            array->u.array.capacity = new_capacity;
        }
        
        array->u.array.items[array->u.array.count++] = value;
        
        skip_whitespace(state);
        
        if (state->position >= state->length) {
            return NULL;
        }
        
        c = state->input[state->position];
        
        if (c == ']') {
            state->position++;
            return array;
        }
        
        if (c == ',') {
            state->position++;
            skip_whitespace(state);
            
            /* Allow trailing comma */
            if (state->position < state->length && 
                state->input[state->position] == ']') {
                state->position++;
                return array;
            }
        }
        else {
            return NULL;
        }
    }
}

/* Parse object */
static kjson_value *
parse_object(parser_state *state)
{
    kjson_value *object;
    kjson_member *last_member;
    size_t key_start;
    size_t key_len;
    kjson_value *value;
    kjson_member *member;
    char c;
    
    if (state->position >= state->length || 
        state->input[state->position] != '{') {
        return NULL;
    }
    
    state->position++;
    
    object = parser_alloc(state, sizeof(kjson_value));
    object->type = KJSON_TYPE_OBJECT;
    object->u.object.first = NULL;
    object->u.object.count = 0;
    
    last_member = NULL;
    
    skip_whitespace(state);
    
    /* Empty object */
    if (state->position < state->length && 
        state->input[state->position] == '}') {
        state->position++;
        return object;
    }
    
    while (true) {
        char *key;
        kjson_value *key_value;
        
        key = NULL;
        
        skip_whitespace(state);
        
        /* Parse key - quoted or unquoted */
        if (state->input[state->position] == '"' || 
            state->input[state->position] == '\'' ||
            state->input[state->position] == '`') {
            key_value = parse_string(state);
            if (!key_value || key_value->type != KJSON_TYPE_STRING) {
                return NULL;
            }
            key = key_value->u.string;
        }
        else {
            /* Unquoted key */
            key_start = state->position;
            
            if (!isalpha(state->input[state->position]) && 
                state->input[state->position] != '_' &&
                state->input[state->position] != '$') {
                return NULL;
            }
            
            while (state->position < state->length &&
                   (isalnum(state->input[state->position]) ||
                    state->input[state->position] == '_' ||
                    state->input[state->position] == '$')) {
                state->position++;
            }
            
            key_len = state->position - key_start;
            key = parser_alloc(state, key_len + 1);
            memcpy(key, state->input + key_start, key_len);
            key[key_len] = '\0';
        }
        
        skip_whitespace(state);
        
        /* Expect colon */
        if (state->position >= state->length || 
            state->input[state->position] != ':') {
            return NULL;
        }
        
        state->position++;
        skip_whitespace(state);
        
        /* Parse value */
        value = parse_value(state);
        if (!value) {
            return NULL;
        }
        
        /* Create member */
        member = parser_alloc(state, sizeof(kjson_member));
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
        
        skip_whitespace(state);
        
        if (state->position >= state->length) {
            return NULL;
        }
        
        c = state->input[state->position];
        
        if (c == '}') {
            state->position++;
            return object;
        }
        
        if (c == ',') {
            state->position++;
            skip_whitespace(state);
            
            /* Allow trailing comma */
            if (state->position < state->length && 
                state->input[state->position] == '}') {
                state->position++;
                return object;
            }
        }
        else {
            return NULL;
        }
    }
}

/* Parse unquoted literal (for unquoted UUIDs and dates) */
static kjson_value *
parse_unquoted_literal(parser_state *state)
{
    size_t start;
    bool is_uuid;
    int i;
    char c;
    kjson_value *value;
    unsigned char *bytes;
    int byte_idx;
    int j;
    int high, low;
    /* size_t date_len; // unused */
    size_t date_end;
    
    start = state->position;
    
    /* UUID pattern: xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx */
    if (state->length - state->position >= 36) {
        is_uuid = true;
        for (i = 0; i < 36; i++) {
            c = state->input[state->position + i];
            if (i == 8 || i == 13 || i == 18 || i == 23) {
                if (c != '-') {
                    is_uuid = false;
                    break;
                }
            } else {
                if (!isxdigit(c)) {
                    is_uuid = false;
                    break;
                }
            }
        }
        
        if (is_uuid) {
            /* Check if we're at a word boundary after the UUID */
            if (state->position + 36 < state->length) {
                char next = state->input[state->position + 36];
                if (isalnum(next) || next == '_' || next == '$') {
                    /* Not a valid UUID if followed by identifier characters */
                    return NULL;
                }
            }
            
            state->position += 36;
            
            value = parser_alloc(state, sizeof(kjson_value));
            byte_idx = 0;
            
            value->type = KJSON_TYPE_UUID;
            value->u.uuid = parser_alloc(state, sizeof(kjson_uuid));
            
            /* Parse UUID bytes */
            bytes = value->u.uuid->bytes;
            
            for (j = 0; j < 36 && byte_idx < 16; j++) {
                if (state->input[start + j] == '-') continue;
                
                high = (state->input[start + j] >= 'a') ? 
                       (state->input[start + j] - 'a' + 10) :
                       (state->input[start + j] >= 'A') ? 
                       (state->input[start + j] - 'A' + 10) :
                       (state->input[start + j] - '0');
                j++;
                low = (state->input[start + j] >= 'a') ? 
                      (state->input[start + j] - 'a' + 10) :
                      (state->input[start + j] >= 'A') ? 
                      (state->input[start + j] - 'A' + 10) :
                      (state->input[start + j] - '0');
                
                bytes[byte_idx++] = (high << 4) | low;
            }
            
            return value;
        }
    }
    
    /* ISO date pattern */
    if (state->length - state->position >= 20 &&
        isdigit(state->input[state->position]) &&
        state->input[state->position + 4] == '-' &&
        state->input[state->position + 7] == '-' &&
        state->input[state->position + 10] == 'T') {
        
        /* Find end of date */
        date_end = state->position + 10;
        while (date_end < state->length &&
               (isdigit(state->input[date_end]) ||
                state->input[date_end] == ':' ||
                state->input[date_end] == '.' ||
                state->input[date_end] == 'T' ||
                state->input[date_end] == 'Z' ||
                state->input[date_end] == '+' ||
                state->input[date_end] == '-')) {
            date_end++;
        }
        
        /* date_len = date_end - state->position; // unused */
        
        /* Check if we're at a word boundary after the date */
        if (date_end < state->length) {
            char next = state->input[date_end];
            if (isalnum(next) || next == '_' || next == '$') {
                /* Not a valid date if followed by identifier characters */
                return NULL;
            }
        }
        
        state->position = date_end;
        
        value = parser_alloc(state, sizeof(kjson_value));
        value->type = KJSON_TYPE_INSTANT;
        value->u.instant = parser_alloc(state, sizeof(kjson_instant));
        value->u.instant->nanoseconds = 0; /* Would need proper parsing */
        value->u.instant->tz_offset = 0;
        
        return value;
    }
    
    return NULL;
}

/* Parse value */
static kjson_value *
parse_value(parser_state *state)
{
    char c;
    kjson_value *unquoted;
    
    skip_whitespace(state);
    
    if (state->position >= state->length) {
        return NULL;
    }
    
    c = state->input[state->position];
    
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
    
    /* Boolean true */
    if (c == 't') {
        return parse_boolean(state);
    }
    
    /* Boolean false */
    if (c == 'f') {
        return parse_boolean(state);
    }
    
    /* Null */
    if (c == 'n' && state->position + 4 <= state->length &&
        strncmp(state->input + state->position, "null", 4) == 0) {
        return parse_null(state);
    }
    
    /* Try unquoted literal first (UUID or date) before number */
    /* This is important because UUIDs and dates can start with digits */
    unquoted = parse_unquoted_literal(state);
    if (unquoted) {
        return unquoted;
    }
    
    /* Number */
    if (c == '-' || isdigit(c)) {
        return parse_number(state);
    }
    
    /* If nothing matched, return NULL */
    return NULL;
}

/* Check if string is a UUID */
static bool
is_uuid_string(const char *str, size_t len)
{
    if (len != 36) return false;
    
    for (size_t i = 0; i < len; i++) {
        if (i == 8 || i == 13 || i == 18 || i == 23) {
            if (str[i] != '-') return false;
        } else {
            if (!isxdigit(str[i])) return false;
        }
    }
    
    return true;
}

/* Check if string is an ISO date */
static bool
is_date_string(const char *str, size_t len)
{
    if (len < 20) return false;
    
    /* Basic check for YYYY-MM-DDTHH:MM:SS format */
    if (!isdigit(str[0]) || !isdigit(str[1]) || 
        !isdigit(str[2]) || !isdigit(str[3])) return false;
    if (str[4] != '-') return false;
    if (!isdigit(str[5]) || !isdigit(str[6])) return false;
    if (str[7] != '-') return false;
    if (!isdigit(str[8]) || !isdigit(str[9])) return false;
    if (str[10] != 'T') return false;
    
    return true;
}

/* Main parse function */
kjson_value *
pg_kjson_parse(const char *input, size_t length, MemoryContext memctx)
{
    parser_state state;
    kjson_value *value;
    
    state.input = input;
    state.length = length;
    state.position = 0;
    state.memctx = memctx;
    
    value = parse_value(&state);
    
    if (value) {
        skip_whitespace(&state);
        if (state.position < state.length) {
            /* Trailing data */
            return NULL;
        }
    }
    
    return value;
}