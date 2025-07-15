/*
 * kjson_stringify.c - Native PostgreSQL implementation of kJSON stringifier
 * 
 * This implementation uses PostgreSQL's memory management throughout.
 */

#include "postgres.h"
#include "lib/stringinfo.h"
#include "utils/builtins.h"
#include "kjson_internal.h"

#include <ctype.h>
#include <math.h>

/* Forward declarations */
static void stringify_value(StringInfo buf, const kjson_value *value, int indent_level, const kjson_write_options *opts);
static void stringify_string(StringInfo buf, const char *str, const kjson_write_options *opts);
static void stringify_array(StringInfo buf, const kjson_value *array, int indent_level, const kjson_write_options *opts);
static void stringify_object(StringInfo buf, const kjson_value *object, int indent_level, const kjson_write_options *opts);
static void append_indent(StringInfo buf, int level, int spaces);
static bool needs_quotes(const char *key);

/* Append indentation */
static void
append_indent(StringInfo buf, int level, int spaces)
{
    int total;
    int i;
    
    if (spaces <= 0) return;
    
    total = level * spaces;
    for (i = 0; i < total; i++) {
        appendStringInfoChar(buf, ' ');
    }
}

/* Check if object key needs quotes */
static bool
needs_quotes(const char *key)
{
    if (!key || !*key) return true;
    
    /* First character must be letter, underscore, or dollar */
    if (!isalpha(key[0]) && key[0] != '_' && key[0] != '$') {
        return true;
    }
    
    /* Rest can be alphanumeric, underscore, or dollar */
    for (const char *p = key + 1; *p; p++) {
        if (!isalnum(*p) && *p != '_' && *p != '$') {
            return true;
        }
    }
    
    return false;
}

/* Select the best quote character for PostgreSQL context */
static char
select_quote_char_for_postgres(const char *str)
{
    int single_quotes = 0;
    int double_quotes = 0;
    int backticks = 0;
    int backslashes = 0;
    int single_cost, double_cost, backtick_cost;
    int min_cost;
    char quote_char;
    
    if (!str) return '"';
    
    /* Count occurrences of each quote type */
    for (const char *p = str; *p; p++) {
        switch (*p) {
            case '\'': single_quotes++; break;
            case '"': double_quotes++; break;
            case '`': backticks++; break;
            case '\\': backslashes++; break;
        }
    }
    
    /* Calculate escaping cost for each quote type 
     * In PostgreSQL context, single quotes have EXTRA burden because
     * the entire kJSON string will be embedded in a PostgreSQL single-quoted string
     * So single quotes inside require double escaping: \' -> \\' in kJSON -> \\\\\' in PostgreSQL
     */
    single_cost = (single_quotes * 2) + backslashes;  /* Extra burden for PostgreSQL */
    double_cost = double_quotes + backslashes;
    backtick_cost = backticks + backslashes;
    
    /* Find minimum cost and choose quote type 
     * In case of tie: double > backtick > single (different from other clients)
     * because single quotes are expensive in PostgreSQL context
     */
    min_cost = double_cost;
    quote_char = '"';
    
    if (backtick_cost < min_cost) {
        min_cost = backtick_cost;
        quote_char = '`';
    }
    
    if (single_cost < min_cost) {
        quote_char = '\'';
    }
    
    return quote_char;
}

/* Stringify a string value with PostgreSQL-aware smart quote selection */
static void
stringify_string(StringInfo buf, const char *str, const kjson_write_options *opts)
{
    char quote_char;
    
    if (opts && opts->use_single_quotes) {
        /* Force single quotes if explicitly requested */
        quote_char = '\'';
    } else {
        /* Use PostgreSQL-aware smart quote selection */
        quote_char = select_quote_char_for_postgres(str);
    }
    
    appendStringInfoChar(buf, quote_char);
    
    if (str) {
        while (*str) {
            switch (*str) {
                case '"':
                    if (quote_char == '"') {
                        appendStringInfoString(buf, "\\\"");
                    } else {
                        appendStringInfoChar(buf, '"');
                    }
                    break;
                case '\'':
                    if (quote_char == '\'') {
                        appendStringInfoString(buf, "\\'");
                    } else {
                        appendStringInfoChar(buf, '\'');
                    }
                    break;
                case '`':
                    if (quote_char == '`') {
                        appendStringInfoString(buf, "\\`");
                    } else {
                        appendStringInfoChar(buf, '`');
                    }
                    break;
                case '\\':
                    appendStringInfoString(buf, "\\\\");
                    break;
                case '\b':
                    appendStringInfoString(buf, "\\b");
                    break;
                case '\f':
                    appendStringInfoString(buf, "\\f");
                    break;
                case '\n':
                    appendStringInfoString(buf, "\\n");
                    break;
                case '\r':
                    appendStringInfoString(buf, "\\r");
                    break;
                case '\t':
                    appendStringInfoString(buf, "\\t");
                    break;
                default:
                    if ((unsigned char)*str < 0x20) {
                        appendStringInfo(buf, "\\u%04x", (unsigned char)*str);
                    } else {
                        appendStringInfoChar(buf, *str);
                    }
            }
            str++;
        }
    }
    
    appendStringInfoChar(buf, quote_char);
}

/* Stringify an array */
static void
stringify_array(StringInfo buf, const kjson_value *array, int indent_level, const kjson_write_options *opts)
{
    bool pretty;
    size_t i;
    
    appendStringInfoChar(buf, '[');
    
    if (array->u.array.count == 0) {
        appendStringInfoChar(buf, ']');
        return;
    }
    
    pretty = (opts && opts->indent > 0);
    
    for (i = 0; i < array->u.array.count; i++) {
        if (i > 0) {
            appendStringInfoChar(buf, ',');
        }
        
        if (pretty) {
            appendStringInfoChar(buf, '\n');
            append_indent(buf, indent_level + 1, opts->indent);
        }
        
        stringify_value(buf, array->u.array.items[i], indent_level + 1, opts);
        
        /* Add trailing comma for last element in pretty mode */
        if (pretty && i == array->u.array.count - 1) {
            appendStringInfoChar(buf, ',');
        }
    }
    
    if (pretty) {
        appendStringInfoChar(buf, '\n');
        append_indent(buf, indent_level, opts->indent);
    }
    
    appendStringInfoChar(buf, ']');
}

/* Stringify an object */
static void
stringify_object(StringInfo buf, const kjson_value *object, int indent_level, const kjson_write_options *opts)
{
    bool pretty;
    bool quote_keys;
    kjson_member *member;
    bool first;
    
    appendStringInfoChar(buf, '{');
    
    if (object->u.object.count == 0) {
        appendStringInfoChar(buf, '}');
        return;
    }
    
    pretty = (opts && opts->indent > 0);
    quote_keys = (opts && opts->quote_keys);
    
    member = object->u.object.first;
    first = true;
    
    while (member) {
        if (!first) {
            appendStringInfoChar(buf, ',');
        }
        
        if (pretty) {
            appendStringInfoChar(buf, '\n');
            append_indent(buf, indent_level + 1, opts->indent);
        }
        
        first = false;
        
        /* Key */
        if (quote_keys || needs_quotes(member->key)) {
            stringify_string(buf, member->key, opts);
        } else {
            appendStringInfoString(buf, member->key);
        }
        
        appendStringInfoChar(buf, ':');
        
        if (pretty) {
            appendStringInfoChar(buf, ' ');
        }
        
        /* Value */
        stringify_value(buf, member->value, indent_level + 1, opts);
        
        member = member->next;
        
        /* Add trailing comma in pretty mode */
        if (pretty && member == NULL) {
            appendStringInfoChar(buf, ',');
        }
    }
    
    if (pretty) {
        appendStringInfoChar(buf, '\n');
        append_indent(buf, indent_level, opts->indent);
    }
    
    appendStringInfoChar(buf, '}');
}

/* Stringify a value */
static void
stringify_value(StringInfo buf, const kjson_value *value, int indent_level, const kjson_write_options *opts)
{
    if (!value) {
        appendStringInfoString(buf, "null");
        return;
    }
    
    switch (value->type) {
        case KJSON_TYPE_NULL:
            appendStringInfoString(buf, "null");
            break;
            
        case KJSON_TYPE_BOOLEAN:
            appendStringInfoString(buf, value->u.boolean ? "true" : "false");
            break;
            
        case KJSON_TYPE_NUMBER:
            {
                char numbuf[64];
                
                if (isnan(value->u.number)) {
                    appendStringInfoString(buf, "null");
                } else if (isinf(value->u.number)) {
                    appendStringInfoString(buf, value->u.number > 0 ? "null" : "null");
                } else {
                    snprintf(numbuf, sizeof(numbuf), "%.17g", value->u.number);
                    appendStringInfoString(buf, numbuf);
                }
            }
            break;
            
        case KJSON_TYPE_STRING:
            stringify_string(buf, value->u.string, opts);
            break;
            
        case KJSON_TYPE_BIGINT:
            if (value->u.bigint) {
                if (value->u.bigint->negative) {
                    appendStringInfoChar(buf, '-');
                }
                appendStringInfoString(buf, value->u.bigint->digits);
                if (opts && opts->bigint_suffix) {
                    appendStringInfoChar(buf, 'n');
                }
            } else {
                appendStringInfoString(buf, "0n");
            }
            break;
            
        case KJSON_TYPE_DECIMAL128:
            if (value->u.decimal) {
                const char *digits;
                size_t len;
                int exp;
                int point_pos;
                size_t i;
                int j;
                
                if (value->u.decimal->negative) {
                    appendStringInfoChar(buf, '-');
                }
                
                /* Insert decimal point at appropriate position */
                
                digits = value->u.decimal->digits;
                len = strlen(digits);
                exp = value->u.decimal->exp;
                
                if (exp >= 0) {
                    /* No decimal point needed */
                    appendStringInfoString(buf, digits);
                    for (j = 0; j < exp; j++) {
                        appendStringInfoChar(buf, '0');
                    }
                } else {
                    point_pos = len + exp;
                    if (point_pos <= 0) {
                        /* Need leading zeros */
                        appendStringInfoString(buf, "0.");
                        for (j = point_pos; j < 0; j++) {
                            appendStringInfoChar(buf, '0');
                        }
                        appendStringInfoString(buf, digits);
                    } else {
                        /* Insert decimal point within digits */
                        for (i = 0; i < len; i++) {
                            if (i == point_pos) {
                                appendStringInfoChar(buf, '.');
                            }
                            appendStringInfoChar(buf, digits[i]);
                        }
                    }
                }
                
                if (opts && opts->decimal_suffix) {
                    appendStringInfoChar(buf, 'm');
                }
            } else {
                appendStringInfoString(buf, "0m");
            }
            break;
            
        case KJSON_TYPE_UUID:
            if (value->u.uuid) {
                /* Format: xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx */
                const unsigned char *bytes = value->u.uuid->bytes;
                appendStringInfo(buf, 
                    "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
                    bytes[0], bytes[1], bytes[2], bytes[3],
                    bytes[4], bytes[5], bytes[6], bytes[7],
                    bytes[8], bytes[9], bytes[10], bytes[11],
                    bytes[12], bytes[13], bytes[14], bytes[15]);
            } else {
                appendStringInfoString(buf, "00000000-0000-0000-0000-000000000000");
            }
            break;
            
        case KJSON_TYPE_INSTANT:
            if (value->u.instant) {
                /* ISO format instant with nanosecond precision */
                int64_t ns = value->u.instant->nanoseconds;
                time_t seconds = ns / 1000000000LL;
                struct tm *tm = gmtime(&seconds);
                int nanoseconds = ns % 1000000000LL;
                
                appendStringInfo(buf, 
                    "%04d-%02d-%02dT%02d:%02d:%02d.%09dZ",
                    tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
                    tm->tm_hour, tm->tm_min, tm->tm_sec,
                    nanoseconds);
            } else {
                appendStringInfoString(buf, "1970-01-01T00:00:00.000000000Z");
            }
            break;
            
        case KJSON_TYPE_DURATION:
            if (value->u.duration) {
                /* Format as ISO 8601 duration string */
                char *duration_str = format_iso_duration(value->u.duration);
                appendStringInfoString(buf, duration_str);
            } else {
                appendStringInfoString(buf, "PT0S");
            }
            break;
            
        case KJSON_TYPE_ARRAY:
            stringify_array(buf, value, indent_level, opts);
            break;
            
        case KJSON_TYPE_OBJECT:
            stringify_object(buf, value, indent_level, opts);
            break;
            
        default:
            appendStringInfoString(buf, "null");
    }
}

/* Main stringify function */
char *
pg_kjson_stringify(const kjson_value *value, const kjson_write_options *opts)
{
    StringInfoData buf;
    initStringInfo(&buf);
    
    stringify_value(&buf, value, 0, opts);
    
    return buf.data;
}