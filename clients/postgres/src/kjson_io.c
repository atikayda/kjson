/*
 * kjson_io.c - Input/Output functions for kjson type
 *
 * This file implements the core I/O functions that PostgreSQL
 * uses to convert between external text representation and
 * internal binary storage.
 * 
 * This is a thin wrapper around the core kJSON C library.
 */

#include "postgres.h"
#include "fmgr.h"
#include "lib/stringinfo.h"
#include "libpq/pqformat.h"
#include "utils/builtins.h"
#include "utils/memutils.h"
#include "kjson.h"

PG_MODULE_MAGIC;

/* PostgreSQL memory allocator for kJSON */
void *pg_kjson_malloc(size_t size)
{
    return palloc(size);
}

void pg_kjson_free(void *ptr)
{
    if (ptr)
        pfree(ptr);
}

static void *pg_kjson_realloc(void *ptr, size_t size)
{
    return repalloc(ptr, size);
}

/*
 * Setup kJSON parser options with PostgreSQL allocator
 */
kjson_options *
get_kjson_options(void)
{
    static kjson_options opts = {
        .allow_comments = true,
        .allow_trailing_commas = true,
        .allow_unquoted_keys = true,
        .parse_dates = true,
        .strict_numbers = false,
        .max_depth = 1000,
        .max_string_length = 1024 * 1024 * 1024,  /* 1GB */
        .allocator = pg_kjson_malloc,
        .deallocator = pg_kjson_free,
        .allocator_ctx = NULL
    };
    return &opts;
}

/*
 * kjson_in - Input function
 *
 * Parses kJSON text format and converts to internal binary format
 */
PG_FUNCTION_INFO_V1(kjson_in);
Datum
kjson_in(PG_FUNCTION_ARGS)
{
    char         *str = PG_GETARG_CSTRING(0);
    kjson_error   error;
    kjson_value  *value;
    PGKJson      *result;
    uint8_t      *binary;
    size_t        binary_size;

    value = kjson_parse_ex(str, strlen(str), get_kjson_options(), &error);
    
    if (value == NULL)
    {
        const char *error_msg = kjson_error_string(error);
        ereport(ERROR,
                (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION),
                 errmsg("invalid input syntax for type kjson: %s", error_msg)));
    }

    binary = kjson_encode_binary(value, &binary_size);
    if (binary == NULL)
    {
        kjson_free(value);
        ereport(ERROR,
                (errcode(ERRCODE_OUT_OF_MEMORY),
                 errmsg("failed to encode kjson to binary format")));
    }

    result = (PGKJson *) palloc(VARHDRSZ + binary_size);
    SET_VARSIZE(result, VARHDRSZ + binary_size);
    memcpy(PGKJSON_DATA(result), binary, binary_size);

    kjson_free(value);
    free(binary);  /* binary was allocated by kjson_encode_binary using standard malloc */

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
    kjson_error   error;
    char         *result;

    value = kjson_decode_binary(PGKJSON_DATA(pgkj), PGKJSON_DATA_SIZE(pgkj), &error);
    if (value == NULL)
    {
        const char *error_msg = kjson_error_string(error);
        ereport(ERROR,
                (errcode(ERRCODE_INVALID_BINARY_REPRESENTATION),
                 errmsg("corrupt kjson binary data: %s", error_msg)));
    }

    kjson_write_options write_opts = {
        .indent = 0,
        .quote_keys = false,
        .bigint_suffix = true,
        .decimal_suffix = true,
        .escape_unicode = false,
        .use_single_quotes = false
    };
    result = kjson_stringify_ex(value, &write_opts);
    if (result == NULL)
    {
        kjson_free(value);
        ereport(ERROR,
                (errcode(ERRCODE_OUT_OF_MEMORY),
                 errmsg("failed to stringify kjson")));
    }

    char *pg_result = pstrdup(result);
    kjson_free(value);
    free(result);  /* result was allocated by kjson_stringify using standard malloc */

    PG_RETURN_CSTRING(pg_result);
}

/*
 * kjson_recv - Binary receive function
 *
 * Receives kjson in binary format (for COPY BINARY)
 * We expect the raw kJSONB binary format
 */
PG_FUNCTION_INFO_V1(kjson_recv);
Datum
kjson_recv(PG_FUNCTION_ARGS)
{
    StringInfo    buf = (StringInfo) PG_GETARG_POINTER(0);
    PGKJson      *result;
    int           nbytes;
    kjson_value  *value;
    kjson_error   error;
    
    nbytes = buf->len - buf->cursor;
    
    /* Validate it's valid kJSONB by trying to decode */
    value = kjson_decode_binary((uint8_t *)(buf->data + buf->cursor), nbytes, &error);
    if (value == NULL)
    {
        ereport(ERROR,
                (errcode(ERRCODE_INVALID_BINARY_REPRESENTATION),
                 errmsg("invalid kJSONB binary format: %s", kjson_error_string(error))));
    }
    kjson_free(value);
    
    result = (PGKJson *) palloc(VARHDRSZ + nbytes);
    SET_VARSIZE(result, VARHDRSZ + nbytes);
    
    pq_copymsgbytes(buf, (char *) PGKJSON_DATA(result), nbytes);
    
    PG_RETURN_POINTER(result);
}

/*
 * kjson_send - Binary send function
 *
 * Sends kjson in binary format (for COPY BINARY)
 * We send the raw kJSONB binary data
 */
PG_FUNCTION_INFO_V1(kjson_send);
Datum
kjson_send(PG_FUNCTION_ARGS)
{
    PGKJson        *pgkj = (PGKJson *) PG_DETOAST_DATUM(PG_GETARG_DATUM(0));
    StringInfoData  buf;
    
    pq_begintypsend(&buf);
    
    pq_sendbytes(&buf, (char *) PGKJSON_DATA(pgkj), PGKJSON_DATA_SIZE(pgkj));
    
    PG_RETURN_BYTEA_P(pq_endtypsend(&buf));
}

