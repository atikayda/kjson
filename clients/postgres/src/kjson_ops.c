/*
 * kjson_ops.c - Operator functions for kjson type
 *
 * This file implements operators like equality, field extraction, etc.
 */

#include "postgres.h"
#include "fmgr.h"
#include "utils/builtins.h"
#include "lib/stringinfo.h"
#include "utils/memutils.h"

/* Use only our internal definitions */
#define PG_KJSON_INTERNAL_ONLY
#include "kjson_internal.h"

/* PG_MODULE_MAGIC is defined in kjson_io_native.c */

/* PostgreSQL kjson type structure */
typedef struct
{
    int32       vl_len_;        /* varlena header */
    uint8_t     data[FLEXIBLE_ARRAY_MEMBER];  /* kJSONB binary data */
} PGKJson;

/* Macros for accessing kjson data */
#define PGKJSON_DATA(x)       ((x)->data)
#define PGKJSON_DATA_SIZE(x)  (VARSIZE(x) - VARHDRSZ)

/*
 * kjson_eq - Equality operator
 *
 * Two kjson values are equal if their binary representations are identical
 */
PG_FUNCTION_INFO_V1(kjson_eq);
Datum
kjson_eq(PG_FUNCTION_ARGS)
{
    PGKJson *a = (PGKJson *) PG_DETOAST_DATUM(PG_GETARG_DATUM(0));
    PGKJson *b = (PGKJson *) PG_DETOAST_DATUM(PG_GETARG_DATUM(1));
    
    /* Compare sizes first */
    if (PGKJSON_DATA_SIZE(a) != PGKJSON_DATA_SIZE(b))
        PG_RETURN_BOOL(false);
    
    /* Compare binary data */
    PG_RETURN_BOOL(memcmp(PGKJSON_DATA(a), PGKJSON_DATA(b), 
                          PGKJSON_DATA_SIZE(a)) == 0);
}

/*
 * kjson_ne - Not equal operator
 */
PG_FUNCTION_INFO_V1(kjson_ne);
Datum
kjson_ne(PG_FUNCTION_ARGS)
{
    /* Just negate equality */
    PG_RETURN_BOOL(!DatumGetBool(kjson_eq(fcinfo)));
}

/*
 * kjson_object_field - Extract object field as kjson
 *
 * Implements the -> operator for object field access
 */
PG_FUNCTION_INFO_V1(kjson_object_field);
Datum
kjson_object_field(PG_FUNCTION_ARGS)
{
    PGKJson      *pgkj = (PGKJson *) PG_DETOAST_DATUM(PG_GETARG_DATUM(0));
    text         *key_text = PG_GETARG_TEXT_PP(1);
    char         *key;
    kjson_value  *value;
    kjson_value  *field_value;
    PGKJson      *result;
    MemoryContext oldcontext;
    MemoryContext parsecontext;
    kjson_member *member;
    StringInfo binary;
    
    /* Get the key string */
    key = text_to_cstring(key_text);
    
    /* Create temporary memory context */
    parsecontext = AllocSetContextCreate(CurrentMemoryContext,
                                         "kJSON decode context",
                                         ALLOCSET_DEFAULT_SIZES);
    
    oldcontext = MemoryContextSwitchTo(parsecontext);
    
    /* Decode the kjson value */
    value = pg_kjson_decode_binary(PGKJSON_DATA(pgkj), PGKJSON_DATA_SIZE(pgkj), parsecontext);
    if (value == NULL)
    {
        MemoryContextSwitchTo(oldcontext);
        MemoryContextDelete(parsecontext);
        pfree(key);
        ereport(ERROR,
                (errcode(ERRCODE_INVALID_BINARY_REPRESENTATION),
                 errmsg("corrupt kjson binary data")));
    }
    
    /* Check if it's an object */
    if (value->type != KJSON_TYPE_OBJECT)
    {
        MemoryContextSwitchTo(oldcontext);
        MemoryContextDelete(parsecontext);
        pfree(key);
        PG_RETURN_NULL();
    }
    
    /* Get the field value */
    field_value = NULL;
    member = value->u.object.first;
    while (member != NULL)
    {
        if (strcmp(member->key, key) == 0)
        {
            field_value = member->value;
            break;
        }
        member = member->next;
    }
    
    if (field_value == NULL)
    {
        MemoryContextSwitchTo(oldcontext);
        MemoryContextDelete(parsecontext);
        pfree(key);
        PG_RETURN_NULL();
    }
    
    /* Encode the field value to binary */
    binary = pg_kjson_encode_binary(field_value);
    
    /* Switch back to original context */
    MemoryContextSwitchTo(oldcontext);
    
    /* Create result */
    result = (PGKJson *) palloc(VARHDRSZ + binary->len);
    SET_VARSIZE(result, VARHDRSZ + binary->len);
    memcpy(PGKJSON_DATA(result), binary->data, binary->len);
    
    /* Clean up */
    MemoryContextDelete(parsecontext);
    pfree(key);
    
    PG_RETURN_POINTER(result);
}

/*
 * kjson_object_field_text - Extract object field as text
 *
 * Implements the ->> operator for object field access
 */
PG_FUNCTION_INFO_V1(kjson_object_field_text);
Datum
kjson_object_field_text(PG_FUNCTION_ARGS)
{
    PGKJson      *pgkj = (PGKJson *) PG_DETOAST_DATUM(PG_GETARG_DATUM(0));
    text         *key_text = PG_GETARG_TEXT_PP(1);
    char         *key;
    kjson_value  *value;
    kjson_value  *field_value;
    char         *text_result;
    text         *pg_text;
    MemoryContext oldcontext;
    MemoryContext parsecontext;
    kjson_member *member;
    text *result_copy;
    
    /* Get the key string */
    key = text_to_cstring(key_text);
    
    /* Create temporary memory context */
    parsecontext = AllocSetContextCreate(CurrentMemoryContext,
                                         "kJSON decode context",
                                         ALLOCSET_DEFAULT_SIZES);
    
    oldcontext = MemoryContextSwitchTo(parsecontext);
    
    /* Decode the kjson value */
    value = pg_kjson_decode_binary(PGKJSON_DATA(pgkj), PGKJSON_DATA_SIZE(pgkj), parsecontext);
    if (value == NULL)
    {
        MemoryContextSwitchTo(oldcontext);
        MemoryContextDelete(parsecontext);
        pfree(key);
        ereport(ERROR,
                (errcode(ERRCODE_INVALID_BINARY_REPRESENTATION),
                 errmsg("corrupt kjson binary data")));
    }
    
    /* Check if it's an object */
    if (value->type != KJSON_TYPE_OBJECT)
    {
        MemoryContextSwitchTo(oldcontext);
        MemoryContextDelete(parsecontext);
        pfree(key);
        PG_RETURN_NULL();
    }
    
    /* Get the field value */
    field_value = NULL;
    member = value->u.object.first;
    while (member != NULL)
    {
        if (strcmp(member->key, key) == 0)
        {
            field_value = member->value;
            break;
        }
        member = member->next;
    }
    
    if (field_value == NULL)
    {
        MemoryContextSwitchTo(oldcontext);
        MemoryContextDelete(parsecontext);
        pfree(key);
        PG_RETURN_NULL();
    }
    
    /* Convert to text based on type */
    if (field_value->type == KJSON_TYPE_STRING)
    {
        /* Return string value directly */
        pg_text = cstring_to_text(field_value->u.string);
    }
    else
    {
        /* Stringify other types */
        kjson_write_options opts = {
            .indent = 0,
            .quote_keys = false,
            .bigint_suffix = true,
            .decimal_suffix = true,
            .escape_unicode = false,
            .use_single_quotes = false
        };
        
        text_result = pg_kjson_stringify(field_value, &opts);
        pg_text = cstring_to_text(text_result);
        pfree(text_result);
    }
    
    /* Switch back to original context and copy result */
    MemoryContextSwitchTo(oldcontext);
    result_copy = (text *) palloc(VARSIZE(pg_text));
    memcpy(result_copy, pg_text, VARSIZE(pg_text));
    
    /* Clean up */
    MemoryContextDelete(parsecontext);
    pfree(key);
    
    PG_RETURN_TEXT_P(result_copy);
}

/*
 * kjson_array_element - Extract array element as kjson
 *
 * Implements the -> operator for array element access
 */
PG_FUNCTION_INFO_V1(kjson_array_element);
Datum
kjson_array_element(PG_FUNCTION_ARGS)
{
    PGKJson      *pgkj = (PGKJson *) PG_DETOAST_DATUM(PG_GETARG_DATUM(0));
    int32         index = PG_GETARG_INT32(1);
    kjson_value  *value;
    kjson_value  *elem_value;
    PGKJson      *result;
    size_t        array_size;
    MemoryContext oldcontext;
    MemoryContext parsecontext;
    StringInfo binary;
    
    /* Create temporary memory context */
    parsecontext = AllocSetContextCreate(CurrentMemoryContext,
                                         "kJSON decode context",
                                         ALLOCSET_DEFAULT_SIZES);
    
    oldcontext = MemoryContextSwitchTo(parsecontext);
    
    /* Decode the kjson value */
    value = pg_kjson_decode_binary(PGKJSON_DATA(pgkj), PGKJSON_DATA_SIZE(pgkj), parsecontext);
    if (value == NULL)
    {
        MemoryContextSwitchTo(oldcontext);
        MemoryContextDelete(parsecontext);
        ereport(ERROR,
                (errcode(ERRCODE_INVALID_BINARY_REPRESENTATION),
                 errmsg("corrupt kjson binary data")));
    }
    
    /* Check if it's an array */
    if (value->type != KJSON_TYPE_ARRAY)
    {
        MemoryContextSwitchTo(oldcontext);
        MemoryContextDelete(parsecontext);
        PG_RETURN_NULL();
    }
    
    /* Get array size */
    array_size = value->u.array.count;
    
    /* Handle negative indices (count from end) */
    if (index < 0)
        index = array_size + index;
    
    /* Check bounds */
    if (index < 0 || index >= array_size)
    {
        MemoryContextSwitchTo(oldcontext);
        MemoryContextDelete(parsecontext);
        PG_RETURN_NULL();
    }
    
    /* Get the element value */
    elem_value = value->u.array.items[index];
    if (elem_value == NULL)
    {
        MemoryContextSwitchTo(oldcontext);
        MemoryContextDelete(parsecontext);
        PG_RETURN_NULL();
    }
    
    /* Encode the element value to binary */
    binary = pg_kjson_encode_binary(elem_value);
    
    /* Switch back to original context */
    MemoryContextSwitchTo(oldcontext);
    
    /* Create result */
    result = (PGKJson *) palloc(VARHDRSZ + binary->len);
    SET_VARSIZE(result, VARHDRSZ + binary->len);
    memcpy(PGKJSON_DATA(result), binary->data, binary->len);
    
    /* Clean up */
    MemoryContextDelete(parsecontext);
    
    PG_RETURN_POINTER(result);
}

/*
 * kjson_array_element_text - Extract array element as text
 *
 * Implements the ->> operator for array element access
 */
PG_FUNCTION_INFO_V1(kjson_array_element_text);
Datum
kjson_array_element_text(PG_FUNCTION_ARGS)
{
    PGKJson      *pgkj = (PGKJson *) PG_DETOAST_DATUM(PG_GETARG_DATUM(0));
    int32         index = PG_GETARG_INT32(1);
    kjson_value  *value;
    kjson_value  *elem_value;
    char         *text_result;
    text         *pg_text;
    size_t        array_size;
    MemoryContext oldcontext;
    MemoryContext parsecontext;
    text *result_copy;
    
    /* Create temporary memory context */
    parsecontext = AllocSetContextCreate(CurrentMemoryContext,
                                         "kJSON decode context",
                                         ALLOCSET_DEFAULT_SIZES);
    
    oldcontext = MemoryContextSwitchTo(parsecontext);
    
    /* Decode the kjson value */
    value = pg_kjson_decode_binary(PGKJSON_DATA(pgkj), PGKJSON_DATA_SIZE(pgkj), parsecontext);
    if (value == NULL)
    {
        MemoryContextSwitchTo(oldcontext);
        MemoryContextDelete(parsecontext);
        ereport(ERROR,
                (errcode(ERRCODE_INVALID_BINARY_REPRESENTATION),
                 errmsg("corrupt kjson binary data")));
    }
    
    /* Check if it's an array */
    if (value->type != KJSON_TYPE_ARRAY)
    {
        MemoryContextSwitchTo(oldcontext);
        MemoryContextDelete(parsecontext);
        PG_RETURN_NULL();
    }
    
    /* Get array size */
    array_size = value->u.array.count;
    
    /* Handle negative indices */
    if (index < 0)
        index = array_size + index;
    
    /* Check bounds */
    if (index < 0 || index >= array_size)
    {
        MemoryContextSwitchTo(oldcontext);
        MemoryContextDelete(parsecontext);
        PG_RETURN_NULL();
    }
    
    /* Get the element value */
    elem_value = value->u.array.items[index];
    if (elem_value == NULL)
    {
        MemoryContextSwitchTo(oldcontext);
        MemoryContextDelete(parsecontext);
        PG_RETURN_NULL();
    }
    
    /* Convert to text based on type */
    if (elem_value->type == KJSON_TYPE_STRING)
    {
        /* Return string value directly */
        pg_text = cstring_to_text(elem_value->u.string);
    }
    else
    {
        /* Stringify other types */
        kjson_write_options opts = {
            .indent = 0,
            .quote_keys = false,
            .bigint_suffix = true,
            .decimal_suffix = true,
            .escape_unicode = false,
            .use_single_quotes = false
        };
        
        text_result = pg_kjson_stringify(elem_value, &opts);
        pg_text = cstring_to_text(text_result);
        pfree(text_result);
    }
    
    /* Switch back to original context and copy result */
    MemoryContextSwitchTo(oldcontext);
    result_copy = (text *) palloc(VARSIZE(pg_text));
    memcpy(result_copy, pg_text, VARSIZE(pg_text));
    
    /* Clean up */
    MemoryContextDelete(parsecontext);
    
    PG_RETURN_TEXT_P(result_copy);
}

/*
 * kjson_pretty - Pretty print kjson with indentation
 */
PG_FUNCTION_INFO_V1(kjson_pretty);
Datum
kjson_pretty(PG_FUNCTION_ARGS)
{
    PGKJson      *pgkj = (PGKJson *) PG_DETOAST_DATUM(PG_GETARG_DATUM(0));
    int32         indent = PG_GETARG_INT32(1);
    kjson_value  *value;
    char         *result;
    text         *pg_text;
    MemoryContext oldcontext;
    MemoryContext parsecontext;
    kjson_write_options opts;
    text *result_copy;
    
    /* Create temporary memory context */
    parsecontext = AllocSetContextCreate(CurrentMemoryContext,
                                         "kJSON decode context",
                                         ALLOCSET_DEFAULT_SIZES);
    
    oldcontext = MemoryContextSwitchTo(parsecontext);
    
    /* Decode from binary format */
    value = pg_kjson_decode_binary(PGKJSON_DATA(pgkj), PGKJSON_DATA_SIZE(pgkj), parsecontext);
    if (value == NULL)
    {
        MemoryContextSwitchTo(oldcontext);
        MemoryContextDelete(parsecontext);
        ereport(ERROR,
                (errcode(ERRCODE_INVALID_BINARY_REPRESENTATION),
                 errmsg("corrupt kjson binary data")));
    }
    
    /* Stringify with pretty printing */
    opts.indent = indent;
    opts.quote_keys = false;
    opts.bigint_suffix = true;
    opts.decimal_suffix = true;
    opts.escape_unicode = false;
    opts.use_single_quotes = false;
    
    result = pg_kjson_stringify(value, &opts);
    if (result == NULL)
    {
        MemoryContextSwitchTo(oldcontext);
        MemoryContextDelete(parsecontext);
        ereport(ERROR,
                (errcode(ERRCODE_OUT_OF_MEMORY),
                 errmsg("failed to stringify kjson")));
    }
    
    /* Create PostgreSQL text */
    pg_text = cstring_to_text(result);
    
    /* Switch back to original context and copy result */
    MemoryContextSwitchTo(oldcontext);
    result_copy = (text *) palloc(VARSIZE(pg_text));
    memcpy(result_copy, pg_text, VARSIZE(pg_text));
    
    /* Clean up */
    MemoryContextDelete(parsecontext);
    pfree(result);
    
    PG_RETURN_TEXT_P(result_copy);
}

/*
 * kjson_compact - Convert kjson to compact text format
 */
PG_FUNCTION_INFO_V1(kjson_compact);
Datum
kjson_compact(PG_FUNCTION_ARGS)
{
    PGKJson      *pgkj = (PGKJson *) PG_DETOAST_DATUM(PG_GETARG_DATUM(0));
    kjson_value  *value;
    char         *result;
    text         *pg_text;
    MemoryContext oldcontext;
    MemoryContext parsecontext;
    kjson_write_options opts;
    text *result_copy;
    
    /* Create temporary memory context */
    parsecontext = AllocSetContextCreate(CurrentMemoryContext,
                                         "kJSON decode context",
                                         ALLOCSET_DEFAULT_SIZES);
    
    oldcontext = MemoryContextSwitchTo(parsecontext);
    
    /* Decode from binary format */
    value = pg_kjson_decode_binary(PGKJSON_DATA(pgkj), PGKJSON_DATA_SIZE(pgkj), parsecontext);
    if (value == NULL)
    {
        MemoryContextSwitchTo(oldcontext);
        MemoryContextDelete(parsecontext);
        ereport(ERROR,
                (errcode(ERRCODE_INVALID_BINARY_REPRESENTATION),
                 errmsg("corrupt kjson binary data")));
    }
    
    /* Stringify with compact format (no indentation) */
    opts.indent = 0;           /* No indentation for compact output */
    opts.quote_keys = false;
    opts.bigint_suffix = true;
    opts.decimal_suffix = true;
    opts.escape_unicode = false;
    opts.use_single_quotes = false;
    
    result = pg_kjson_stringify(value, &opts);
    if (result == NULL)
    {
        MemoryContextSwitchTo(oldcontext);
        MemoryContextDelete(parsecontext);
        ereport(ERROR,
                (errcode(ERRCODE_OUT_OF_MEMORY),
                 errmsg("failed to stringify kjson")));
    }
    
    /* Create PostgreSQL text */
    pg_text = cstring_to_text(result);
    
    /* Switch back to original context and copy result */
    MemoryContextSwitchTo(oldcontext);
    result_copy = (text *) palloc(VARSIZE(pg_text));
    memcpy(result_copy, pg_text, VARSIZE(pg_text));
    
    /* Clean up */
    MemoryContextDelete(parsecontext);
    pfree(result);
    
    PG_RETURN_TEXT_P(result_copy);
}

/*
 * kjson_typeof - Get the type of a kjson value
 */
PG_FUNCTION_INFO_V1(kjson_typeof);
Datum
kjson_typeof(PG_FUNCTION_ARGS)
{
    PGKJson      *pgkj = (PGKJson *) PG_DETOAST_DATUM(PG_GETARG_DATUM(0));
    kjson_value  *value;
    const char   *type_name;
    MemoryContext oldcontext;
    MemoryContext parsecontext;
    text *result;
    
    /* Create temporary memory context */
    parsecontext = AllocSetContextCreate(CurrentMemoryContext,
                                         "kJSON decode context",
                                         ALLOCSET_DEFAULT_SIZES);
    
    oldcontext = MemoryContextSwitchTo(parsecontext);
    
    /* Decode from binary format */
    value = pg_kjson_decode_binary(PGKJSON_DATA(pgkj), PGKJSON_DATA_SIZE(pgkj), parsecontext);
    if (value == NULL)
    {
        MemoryContextSwitchTo(oldcontext);
        MemoryContextDelete(parsecontext);
        ereport(ERROR,
                (errcode(ERRCODE_INVALID_BINARY_REPRESENTATION),
                 errmsg("corrupt kjson binary data")));
    }
    
    /* Map to type name */
    switch (value->type)
    {
        case KJSON_TYPE_NULL:
            type_name = "null";
            break;
        case KJSON_TYPE_BOOLEAN:
            type_name = "boolean";
            break;
        case KJSON_TYPE_NUMBER:
            type_name = "number";
            break;
        case KJSON_TYPE_BIGINT:
            type_name = "bigint";
            break;
        case KJSON_TYPE_DECIMAL128:
            type_name = "decimal128";
            break;
        case KJSON_TYPE_STRING:
            type_name = "string";
            break;
        case KJSON_TYPE_UUID:
            type_name = "uuid";
            break;
        case KJSON_TYPE_INSTANT:
            type_name = "instant";
            break;
        case KJSON_TYPE_ARRAY:
            type_name = "array";
            break;
        case KJSON_TYPE_OBJECT:
            type_name = "object";
            break;
        case KJSON_TYPE_BINARY:
            type_name = "binary";
            break;
        default:
            type_name = "unknown";
            break;
    }
    
    /* Switch back to original context */
    MemoryContextSwitchTo(oldcontext);
    
    /* Create result */
    result = cstring_to_text(type_name);
    
    /* Clean up */
    MemoryContextDelete(parsecontext);
    
    PG_RETURN_TEXT_P(result);
}