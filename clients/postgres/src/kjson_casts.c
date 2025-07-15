/*
 * kjson_casts.c - Cast functions between kjson and other types
 *
 * This file implements conversions between kjson and standard
 * PostgreSQL JSON types.
 */

#include "postgres.h"
#include "fmgr.h"
#include "utils/json.h"
#include "utils/jsonb.h"
#include "utils/builtins.h"
#include "utils/memutils.h"
#include "lib/stringinfo.h"

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
 * kjson_to_json - Cast kjson to json
 *
 * Converts kJSON to standard JSON format (loses extended types)
 */
PG_FUNCTION_INFO_V1(kjson_to_json);
Datum
kjson_to_json(PG_FUNCTION_ARGS)
{
    PGKJson      *pgkj = (PGKJson *) PG_DETOAST_DATUM(PG_GETARG_DATUM(0));
    kjson_value  *value;
    char         *json_text;
    text         *result;
    MemoryContext oldcontext;
    MemoryContext parsecontext;
    kjson_write_options opts;
    
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
    
    /* Convert to JSON-compatible text (no extended types) */
    opts = (kjson_write_options) {
        .indent = 0,
        .quote_keys = true,
        .bigint_suffix = false,     /* No 'n' suffix */
        .decimal_suffix = false,    /* No 'm' suffix */
        .escape_unicode = false,
        .use_single_quotes = false
    };
    
    json_text = pg_kjson_stringify(value, &opts);
    if (json_text == NULL)
    {
        MemoryContextSwitchTo(oldcontext);
        MemoryContextDelete(parsecontext);
        ereport(ERROR,
                (errcode(ERRCODE_OUT_OF_MEMORY),
                 errmsg("failed to stringify kjson to json")));
    }
    
    /* Switch back to original context */
    MemoryContextSwitchTo(oldcontext);
    
    /* Create PostgreSQL json type */
    result = cstring_to_text(json_text);
    
    /* Clean up */
    MemoryContextDelete(parsecontext);
    
    PG_RETURN_TEXT_P(result);
}

/*
 * kjson_to_jsonb - Cast kjson to jsonb
 *
 * Converts kJSON to JSONB format (loses extended types)
 */
PG_FUNCTION_INFO_V1(kjson_to_jsonb);
Datum
kjson_to_jsonb(PG_FUNCTION_ARGS)
{
    PGKJson      *pgkj = (PGKJson *) PG_DETOAST_DATUM(PG_GETARG_DATUM(0));
    kjson_value  *value;
    char         *json_text;
    Jsonb        *result;
    MemoryContext oldcontext;
    MemoryContext parsecontext;
    kjson_write_options opts;
    
    /* Create temporary memory context */
    parsecontext = AllocSetContextCreate(CurrentMemoryContext,
                                         "kJSON decode context",
                                         ALLOCSET_DEFAULT_SIZES);
    
    oldcontext = MemoryContextSwitchTo(parsecontext);
    
    /* First convert to JSON text using kjson_to_json logic */
    value = pg_kjson_decode_binary(PGKJSON_DATA(pgkj), PGKJSON_DATA_SIZE(pgkj), parsecontext);
    if (value == NULL)
    {
        MemoryContextSwitchTo(oldcontext);
        MemoryContextDelete(parsecontext);
        ereport(ERROR,
                (errcode(ERRCODE_INVALID_BINARY_REPRESENTATION),
                 errmsg("corrupt kjson binary data")));
    }
    
    /* Convert to JSON-compatible text */
    opts = (kjson_write_options) {
        .indent = 0,
        .quote_keys = true,
        .bigint_suffix = false,
        .decimal_suffix = false,
        .escape_unicode = false,
        .use_single_quotes = false
    };
    
    json_text = pg_kjson_stringify(value, &opts);
    if (json_text == NULL)
    {
        MemoryContextSwitchTo(oldcontext);
        MemoryContextDelete(parsecontext);
        ereport(ERROR,
                (errcode(ERRCODE_OUT_OF_MEMORY),
                 errmsg("failed to stringify kjson to json")));
    }
    
    /* Switch back to original context */
    MemoryContextSwitchTo(oldcontext);
    
    /* Parse as JSONB */
    result = DatumGetJsonbP(DirectFunctionCall1(jsonb_in, CStringGetDatum(json_text)));
    
    /* Clean up */
    MemoryContextDelete(parsecontext);
    
    PG_RETURN_JSONB_P(result);
}

/*
 * json_to_kjson - Cast json to kjson
 *
 * Converts standard JSON to kJSON format
 */
PG_FUNCTION_INFO_V1(json_to_kjson);
Datum
json_to_kjson(PG_FUNCTION_ARGS)
{
    text         *json = PG_GETARG_TEXT_PP(0);
    char         *json_str;
    kjson_value  *value;
    PGKJson      *result;
    MemoryContext oldcontext;
    MemoryContext parsecontext;
    StringInfo binary;
    
    /* Get JSON text */
    json_str = text_to_cstring(json);
    
    /* Create temporary memory context */
    parsecontext = AllocSetContextCreate(CurrentMemoryContext,
                                         "kJSON parse context",
                                         ALLOCSET_DEFAULT_SIZES);
    
    oldcontext = MemoryContextSwitchTo(parsecontext);
    
    /* Parse as kJSON (will handle standard JSON) */
    value = pg_kjson_parse(json_str, strlen(json_str), parsecontext);
    
    if (value == NULL)
    {
        MemoryContextSwitchTo(oldcontext);
        MemoryContextDelete(parsecontext);
        pfree(json_str);
        ereport(ERROR,
                (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION),
                 errmsg("invalid JSON for kjson")));
    }
    
    /* Convert to binary format */
    binary = pg_kjson_encode_binary(value);
    
    /* Switch back to original context */
    MemoryContextSwitchTo(oldcontext);
    
    /* Allocate PostgreSQL result */
    result = (PGKJson *) palloc(VARHDRSZ + binary->len);
    SET_VARSIZE(result, VARHDRSZ + binary->len);
    memcpy(PGKJSON_DATA(result), binary->data, binary->len);
    
    /* Clean up */
    MemoryContextDelete(parsecontext);
    pfree(json_str);
    
    PG_RETURN_POINTER(result);
}

/*
 * jsonb_to_kjson - Cast jsonb to kjson
 *
 * Converts JSONB to kJSON format
 */
PG_FUNCTION_INFO_V1(jsonb_to_kjson);
Datum
jsonb_to_kjson(PG_FUNCTION_ARGS)
{
    Jsonb        *jb = PG_GETARG_JSONB_P(0);
    char         *json_str;
    kjson_value  *value;
    PGKJson      *result;
    MemoryContext oldcontext;
    MemoryContext parsecontext;
    StringInfo binary;
    
    /* Convert JSONB to text first */
    json_str = JsonbToCString(NULL, &jb->root, VARSIZE(jb));
    
    /* Create temporary memory context */
    parsecontext = AllocSetContextCreate(CurrentMemoryContext,
                                         "kJSON parse context",
                                         ALLOCSET_DEFAULT_SIZES);
    
    oldcontext = MemoryContextSwitchTo(parsecontext);
    
    /* Parse as kJSON */
    value = pg_kjson_parse(json_str, strlen(json_str), parsecontext);
    
    if (value == NULL)
    {
        MemoryContextSwitchTo(oldcontext);
        MemoryContextDelete(parsecontext);
        pfree(json_str);
        ereport(ERROR,
                (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION),
                 errmsg("invalid JSONB for kjson")));
    }
    
    /* Convert to binary format */
    binary = pg_kjson_encode_binary(value);
    
    /* Switch back to original context */
    MemoryContextSwitchTo(oldcontext);
    
    /* Allocate PostgreSQL result */
    result = (PGKJson *) palloc(VARHDRSZ + binary->len);
    SET_VARSIZE(result, VARHDRSZ + binary->len);
    memcpy(PGKJSON_DATA(result), binary->data, binary->len);
    
    /* Clean up */
    MemoryContextDelete(parsecontext);
    pfree(json_str);
    
    PG_RETURN_POINTER(result);
}

/*
 * text_to_kjson - Cast text to kjson
 *
 * Parses text as kJSON format
 */
PG_FUNCTION_INFO_V1(text_to_kjson);
Datum
text_to_kjson(PG_FUNCTION_ARGS)
{
    text         *txt = PG_GETARG_TEXT_PP(0);
    char         *str;
    kjson_value  *value;
    PGKJson      *result;
    MemoryContext oldcontext;
    MemoryContext parsecontext;
    StringInfo binary;
    
    /* Get text string */
    str = text_to_cstring(txt);
    
    /* Create temporary memory context */
    parsecontext = AllocSetContextCreate(CurrentMemoryContext,
                                         "kJSON parse context",
                                         ALLOCSET_DEFAULT_SIZES);
    
    oldcontext = MemoryContextSwitchTo(parsecontext);
    
    /* Parse as kJSON */
    value = pg_kjson_parse(str, strlen(str), parsecontext);
    
    if (value == NULL)
    {
        MemoryContextSwitchTo(oldcontext);
        MemoryContextDelete(parsecontext);
        pfree(str);
        ereport(ERROR,
                (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION),
                 errmsg("invalid kJSON text")));
    }
    
    /* Convert to binary format */
    binary = pg_kjson_encode_binary(value);
    
    /* Switch back to original context */
    MemoryContextSwitchTo(oldcontext);
    
    /* Allocate PostgreSQL result */
    result = (PGKJson *) palloc(VARHDRSZ + binary->len);
    SET_VARSIZE(result, VARHDRSZ + binary->len);
    memcpy(PGKJSON_DATA(result), binary->data, binary->len);
    
    /* Clean up */
    MemoryContextDelete(parsecontext);
    pfree(str);
    
    PG_RETURN_POINTER(result);
}