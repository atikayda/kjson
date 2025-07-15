/*
 * kjson_io_simple.c - Simplified I/O functions for debugging
 */

#include "postgres.h"
#include "fmgr.h"
#include "utils/builtins.h"
#include "kjson.h"

PG_MODULE_MAGIC;

/*
 * Simplified input function - just stores the text as-is
 */
PG_FUNCTION_INFO_V1(kjson_in_simple);
Datum
kjson_in_simple(PG_FUNCTION_ARGS)
{
    char *str = PG_GETARG_CSTRING(0);
    text *result;
    
    /* Just validate it's parseable */
    kjson_error error;
    kjson_value *value = kjson_parse(str, strlen(str), &error);
    if (value == NULL) {
        ereport(ERROR,
                (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION),
                 errmsg("invalid kjson value"),
                 errdetail("Parse error: %s", kjson_error_string(error))));
    }
    kjson_free(value);
    
    /* Store as text */
    result = cstring_to_text(str);
    PG_RETURN_TEXT_P(result);
}

/*
 * Simplified output function
 */
PG_FUNCTION_INFO_V1(kjson_out_simple);
Datum
kjson_out_simple(PG_FUNCTION_ARGS)
{
    text *txt = PG_GETARG_TEXT_P(0);
    char *result = text_to_cstring(txt);
    PG_RETURN_CSTRING(result);
}