/*
 * kjson_io_native.c - Native PostgreSQL I/O functions for kjson type
 *
 * This file implements the input/output functions using the native
 * PostgreSQL implementation instead of the external C library.
 */

#include "postgres.h"
#include "fmgr.h"
#include "utils/builtins.h"
#include "utils/memutils.h"
#include "lib/stringinfo.h"

/* Use only our internal definitions, not the C library */
#define PG_KJSON_INTERNAL_ONLY
#include "kjson_internal.h"

/* PostgreSQL kjson type structure */
typedef struct
{
    int32       vl_len_;        /* varlena header */
    uint8       data[FLEXIBLE_ARRAY_MEMBER];  /* kJSONB binary data */
} PGKJson;

/* Macros for accessing kjson data */
#define PGKJSON_DATA(x)       ((x)->data)
#define PGKJSON_DATA_SIZE(x)  (VARSIZE(x) - VARHDRSZ)

PG_MODULE_MAGIC;

/*
 * kjson_in - Input function
 *
 * Parses kJSON text and stores as binary format
 */
PG_FUNCTION_INFO_V1(kjson_in);
Datum
kjson_in(PG_FUNCTION_ARGS)
{
    char         *str = PG_GETARG_CSTRING(0);
    kjson_value  *value;
    PGKJson      *result;
    StringInfo    binary;
    MemoryContext oldcontext;
    MemoryContext parsecontext;
    
    /* Create temporary memory context for parsing */
    parsecontext = AllocSetContextCreate(CurrentMemoryContext,
                                         "kJSON parse context",
                                         ALLOCSET_DEFAULT_SIZES);
    
    oldcontext = MemoryContextSwitchTo(parsecontext);
    
    /* Parse the input */
    value = pg_kjson_parse(str, strlen(str), parsecontext);
    if (value == NULL)
    {
        MemoryContextSwitchTo(oldcontext);
        MemoryContextDelete(parsecontext);
        ereport(ERROR,
                (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION),
                 errmsg("invalid kjson value"),
                 errdetail("Input: \"%s\" (length %d)", str, (int)strlen(str))));
    }
    
    /* Convert to binary format */
    binary = pg_kjson_encode_binary(value);
    
    /* Switch back to original context before allocating result */
    MemoryContextSwitchTo(oldcontext);
    
    /* Allocate result in original context */
    result = (PGKJson *) palloc(VARHDRSZ + binary->len);
    SET_VARSIZE(result, VARHDRSZ + binary->len);
    memcpy(PGKJSON_DATA(result), binary->data, binary->len);
    
    /* Clean up */
    MemoryContextDelete(parsecontext);
    
    PG_RETURN_POINTER(result);
}

/*
 * kjson_out - Output function
 *
 * Converts internal binary format to kJSON text
 */
PG_FUNCTION_INFO_V1(kjson_out);
Datum
kjson_out(PG_FUNCTION_ARGS)
{
    PGKJson      *pgkj = (PGKJson *) PG_DETOAST_DATUM(PG_GETARG_DATUM(0));
    kjson_value  *value;
    char         *result;
    MemoryContext oldcontext;
    MemoryContext parsecontext;
    kjson_write_options write_opts;
    char         *text;
    
    /* Create temporary memory context */
    parsecontext = AllocSetContextCreate(CurrentMemoryContext,
                                         "kJSON decode context",
                                         ALLOCSET_DEFAULT_SIZES);
    
    oldcontext = MemoryContextSwitchTo(parsecontext);
    
    /* Decode from binary format */
    value = pg_kjson_decode_binary(PGKJSON_DATA(pgkj), 
                                   PGKJSON_DATA_SIZE(pgkj), 
                                   parsecontext);
    if (value == NULL)
    {
        MemoryContextSwitchTo(oldcontext);
        MemoryContextDelete(parsecontext);
        ereport(ERROR,
                (errcode(ERRCODE_INVALID_BINARY_REPRESENTATION),
                 errmsg("corrupt kjson binary data")));
    }
    
    /* Convert to text - compact format by default */
    
    write_opts.indent = 0;           /* No indentation for compact output */
    write_opts.quote_keys = false;
    write_opts.bigint_suffix = true;
    write_opts.decimal_suffix = true;
    write_opts.escape_unicode = false;
    write_opts.use_single_quotes = false;  /* Use double quotes in SQL context */
    
    text = pg_kjson_stringify(value, &write_opts);
    
    /* Switch back to original context */
    MemoryContextSwitchTo(oldcontext);
    
    /* Copy result to original context */
    result = pstrdup(text);
    
    /* Clean up */
    MemoryContextDelete(parsecontext);
    
    PG_RETURN_CSTRING(result);
}

/*
 * kjson_recv - Binary input function
 */
PG_FUNCTION_INFO_V1(kjson_recv);
Datum
kjson_recv(PG_FUNCTION_ARGS)
{
    /* For now, just error out - not implemented */
    ereport(ERROR,
            (errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
             errmsg("kjson_recv not implemented")));
    
    PG_RETURN_NULL();
}

/*
 * kjson_send - Binary output function
 */
PG_FUNCTION_INFO_V1(kjson_send);
Datum
kjson_send(PG_FUNCTION_ARGS)
{
    /* For now, just error out - not implemented */
    ereport(ERROR,
            (errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
             errmsg("kjson_send not implemented")));
    
    PG_RETURN_NULL();
}