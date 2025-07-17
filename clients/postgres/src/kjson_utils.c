/*
 * kjson_utils.c - Utility functions for kjson type
 *
 * This file implements extraction functions for specific types,
 * aggregate functions, and other utilities.
 */

#include "postgres.h"
#include "fmgr.h"
#include "funcapi.h"
#include "utils/builtins.h"
#include "utils/uuid.h"
#include "utils/numeric.h"
#include "utils/timestamp.h"
#include "utils/memutils.h"
#include "utils/array.h"
#include "catalog/pg_type.h"
#include "lib/stringinfo.h"
#include "utils/lsyscache.h"
#include "utils/typcache.h"
#include "utils/syscache.h"
#include "access/htup_details.h"

/* Use only our internal definitions */
#define PG_KJSON_INTERNAL_ONLY
#include "kjson_internal.h"


/* PostgreSQL kjson type structure */
typedef struct
{
    int32       vl_len_;        /* varlena header */
    uint8_t     data[FLEXIBLE_ARRAY_MEMBER];  /* kJSONB binary data */
} PGKJson;

/* Macros for accessing kjson data */
#define PGKJSON_DATA(x)       ((x)->data)
#define PGKJSON_DATA_SIZE(x)  (VARSIZE(x) - VARHDRSZ)

/* Helper function to extract value at path */
static kjson_value *
kjson_get_path_value(kjson_value *root, struct ArrayType *path_array)
{
    kjson_value *current;
    int path_len;
    int i;
    Datum *path_datums;
    bool *path_nulls;
    char *key;
    
    current = root;
    
    if (!root || !path_array)
        return NULL;
        
    deconstruct_array(path_array, TEXTOID, -1, false, 'i',
                      &path_datums, &path_nulls, &path_len);
    
    for (i = 0; i < path_len; i++) {
        if (path_nulls && path_nulls[i])
            return NULL;
            
        key = TextDatumGetCString(path_datums[i]);
        
        if (current->type == KJSON_TYPE_OBJECT) {
            kjson_member *member;
            bool found;
            
            member = current->u.object.first;
            found = false;
            
            while (member) {
                if (strcmp(member->key, key) == 0) {
                    current = member->value;
                    found = true;
                    break;
                }
                member = member->next;
            }
            
            if (!found) {
                pfree(key);
                return NULL;
            }
        }
        else if (current->type == KJSON_TYPE_ARRAY) {
            int index = atoi(key);
            if (index >= 0 && index < current->u.array.count)
                current = current->u.array.items[index];
            else {
                pfree(key);
                return NULL;
            }
        }
        else {
            pfree(key);
            return NULL;
        }
        
        pfree(key);
    }
    
    return current;
}

/* Internal aggregate state */
typedef struct {
    MemoryContext context;
    kjson_value *result;
    bool is_object_agg;
} KJsonAggState;

/*
 * kjson_extract_uuid - Extract UUID value from kjson path
 *
 * Usage: kjson_extract_uuid(kjson_value, 'field1', 'field2', ...)
 */
PG_FUNCTION_INFO_V1(kjson_extract_uuid);
Datum
kjson_extract_uuid(PG_FUNCTION_ARGS)
{
    PGKJson      *pgkj = (PGKJson *) PG_DETOAST_DATUM(PG_GETARG_DATUM(0));
    kjson_value  *value;
    kjson_value  *current;
    int           nargs = PG_NARGS();
    int           i;
    MemoryContext oldcontext;
    MemoryContext parsecontext;
    pg_uuid_t    *uuid;
    
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
    
    /* Navigate path */
    current = value;
    for (i = 1; i < nargs && current != NULL; i++)
    {
        text *key_text = PG_GETARG_TEXT_PP(i);
        char *key = text_to_cstring(key_text);
        
        if (current->type == KJSON_TYPE_OBJECT)
        {
            kjson_member *member = current->u.object.first;
            current = NULL;
            while (member != NULL)
            {
                if (strcmp(member->key, key) == 0)
                {
                    current = member->value;
                    break;
                }
                member = member->next;
            }
        }
        else
        {
            current = NULL;
        }
        
        pfree(key);
    }
    
    /* Check if we found a UUID */
    if (current == NULL || current->type != KJSON_TYPE_UUID)
    {
        MemoryContextSwitchTo(oldcontext);
        MemoryContextDelete(parsecontext);
        PG_RETURN_NULL();
    }
    
    /* Switch back to original context */
    
    MemoryContextSwitchTo(oldcontext);
    
    /* Convert to PostgreSQL UUID */
    uuid = (pg_uuid_t *) palloc(sizeof(pg_uuid_t));
    memcpy(uuid->data, current->u.uuid->bytes, 16);
    
    MemoryContextDelete(parsecontext);
    PG_RETURN_UUID_P(uuid);
}

/*
 * kjson_extract_numeric - Extract numeric value from kjson path
 *
 * Usage: kjson_extract_numeric(kjson_value, 'field1', 'field2', ...)
 * Works for Number, BigInt, and Decimal128 types
 */
PG_FUNCTION_INFO_V1(kjson_extract_numeric);
Datum
kjson_extract_numeric(PG_FUNCTION_ARGS)
{
    PGKJson      *pgkj = (PGKJson *) PG_DETOAST_DATUM(PG_GETARG_DATUM(0));
    kjson_value  *value;
    kjson_value  *current;
    int           nargs = PG_NARGS();
    int           i;
    MemoryContext oldcontext;
    MemoryContext parsecontext;
    Numeric       result;
    double        d;
    char          buf[64];
    char         *num_str;
    
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
    
    /* Navigate path */
    current = value;
    for (i = 1; i < nargs && current != NULL; i++)
    {
        text *key_text = PG_GETARG_TEXT_PP(i);
        char *key = text_to_cstring(key_text);
        
        if (current->type == KJSON_TYPE_OBJECT)
        {
            kjson_member *member = current->u.object.first;
            current = NULL;
            while (member != NULL)
            {
                if (strcmp(member->key, key) == 0)
                {
                    current = member->value;
                    break;
                }
                member = member->next;
            }
        }
        else
        {
            current = NULL;
        }
        
        pfree(key);
    }
    
    /* Check if we found a numeric type */
    if (current == NULL)
    {
        MemoryContextSwitchTo(oldcontext);
        MemoryContextDelete(parsecontext);
        PG_RETURN_NULL();
    }
    
    /* Switch back to original context for result creation */
    MemoryContextSwitchTo(oldcontext);
    
    result = NULL;
    
    if (current->type == KJSON_TYPE_NUMBER)
    {
        /* Convert double to numeric */
        d = current->u.number;
        snprintf(buf, sizeof(buf), "%.17g", d);
        result = DatumGetNumeric(DirectFunctionCall3(numeric_in,
                                                     CStringGetDatum(buf),
                                                     ObjectIdGetDatum(0),
                                                     Int32GetDatum(-1)));
    }
    else if (current->type == KJSON_TYPE_BIGINT)
    {
        /* Convert BigInt to numeric */
        num_str = psprintf("%s%s", 
                                current->u.bigint->negative ? "-" : "",
                                current->u.bigint->digits);
        result = DatumGetNumeric(DirectFunctionCall3(numeric_in,
                                                     CStringGetDatum(num_str),
                                                     ObjectIdGetDatum(0),
                                                     Int32GetDatum(-1)));
        pfree(num_str);
    }
    else if (current->type == KJSON_TYPE_DECIMAL128)
    {
        /* Convert Decimal128 to numeric */
        /* TODO: Handle exponent properly */
        num_str = psprintf("%s%s", 
                                current->u.decimal->negative ? "-" : "",
                                current->u.decimal->digits);
        result = DatumGetNumeric(DirectFunctionCall3(numeric_in,
                                                     CStringGetDatum(num_str),
                                                     ObjectIdGetDatum(0),
                                                     Int32GetDatum(-1)));
        pfree(num_str);
    }
    else
    {
        MemoryContextDelete(parsecontext);
        PG_RETURN_NULL();
    }
    
    MemoryContextDelete(parsecontext);
    PG_RETURN_NUMERIC(result);
}

/*
 * kjson_extract_instant - Extract instant value from kjson path
 *
 * Usage: kjson_extract_instant(kjson_value, 'field1', 'field2', ...)
 */
PG_FUNCTION_INFO_V1(kjson_extract_instant);
Datum
kjson_extract_instant(PG_FUNCTION_ARGS)
{
    PGKJson      *pgkj = (PGKJson *) PG_DETOAST_DATUM(PG_GETARG_DATUM(0));
    kjson_value  *value;
    kjson_value  *current;
    int           nargs = PG_NARGS();
    int           i;
    MemoryContext oldcontext;
    MemoryContext parsecontext;
    int64         microseconds;
    int64         pg_epoch_offset;
    TimestampTz   ts;
    
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
    
    /* Navigate path */
    current = value;
    for (i = 1; i < nargs && current != NULL; i++)
    {
        text *key_text = PG_GETARG_TEXT_PP(i);
        char *key = text_to_cstring(key_text);
        
        if (current->type == KJSON_TYPE_OBJECT)
        {
            kjson_member *member = current->u.object.first;
            current = NULL;
            while (member != NULL)
            {
                if (strcmp(member->key, key) == 0)
                {
                    current = member->value;
                    break;
                }
                member = member->next;
            }
        }
        else
        {
            current = NULL;
        }
        
        pfree(key);
    }
    
    /* Check if we found a date */
    if (current == NULL || current->type != KJSON_TYPE_INSTANT)
    {
        MemoryContextSwitchTo(oldcontext);
        MemoryContextDelete(parsecontext);
        PG_RETURN_NULL();
    }
    
    /* Convert to PostgreSQL instant */
    /* Convert nanoseconds to microseconds for PostgreSQL */
    microseconds = current->u.instant->nanoseconds / 1000;
    
    /* PostgreSQL timestamps are microseconds since 2000-01-01 00:00:00 */
    /* Unix epoch is 1970-01-01, PostgreSQL epoch is 2000-01-01 */
    /* Difference is 946684800 seconds = 946684800000000 microseconds */
    pg_epoch_offset = 946684800000000LL;
    
    ts = microseconds - pg_epoch_offset;
    
    MemoryContextSwitchTo(oldcontext);
    MemoryContextDelete(parsecontext);
    PG_RETURN_TIMESTAMPTZ(ts);
}

/*
 * kjson_agg_transfn - Transition function for kjson_agg
 *
 * Builds an array from input values
 */
PG_FUNCTION_INFO_V1(kjson_agg_transfn);
Datum
kjson_agg_transfn(PG_FUNCTION_ARGS)
{
    MemoryContext aggcontext;
    MemoryContext oldcontext;
    KJsonAggState *state;
    kjson_value  *value;
    Oid           valtype;
    
    if (!AggCheckCallContext(fcinfo, &aggcontext))
        elog(ERROR, "kjson_agg_transfn called in non-aggregate context");
    
    /* First call - initialize state */
    if (PG_ARGISNULL(0))
    {
        oldcontext = MemoryContextSwitchTo(aggcontext);
        
        state = (KJsonAggState *) palloc(sizeof(KJsonAggState));
        state->context = aggcontext;
        state->is_object_agg = false;
        
        /* Create empty array */
        state->result = (kjson_value *) palloc(sizeof(kjson_value));
        state->result->type = KJSON_TYPE_ARRAY;
        state->result->u.array.items = NULL;
        state->result->u.array.count = 0;
        state->result->u.array.capacity = 0;
        
        MemoryContextSwitchTo(oldcontext);
    }
    else
    {
        state = (KJsonAggState *) PG_GETARG_POINTER(0);
    }
    
    /* Skip NULL values */
    if (PG_ARGISNULL(1))
        PG_RETURN_POINTER(state);
    
    /* Get the input value and convert to kjson */
    oldcontext = MemoryContextSwitchTo(aggcontext);
    
    valtype = get_fn_expr_argtype(fcinfo->flinfo, 1);
    
    /* Check if it's already a kjson type */
    if (valtype != InvalidOid) {
        char *typename = format_type_be(valtype);
        if (typename && strcmp(typename, "kjson") == 0) {
            /* It's already kjson, decode it */
            PGKJson *input = (PGKJson *) PG_DETOAST_DATUM(PG_GETARG_DATUM(1));
            value = pg_kjson_decode_binary(PGKJSON_DATA(input), 
                                          PGKJSON_DATA_SIZE(input), 
                                          aggcontext);
            if (!value) {
                MemoryContextSwitchTo(oldcontext);
                pfree(typename);
                ereport(ERROR,
                        (errcode(ERRCODE_INVALID_BINARY_REPRESENTATION),
                         errmsg("invalid kJSON in aggregate")));
            }
        } else {
            /* Convert SQL type to kjson */
            value = sql_value_to_kjson(PG_GETARG_DATUM(1), valtype, false, aggcontext);
        }
        if (typename) pfree(typename);
    } else {
        /* Unknown type, try to convert */
        value = sql_value_to_kjson(PG_GETARG_DATUM(1), InvalidOid, false, aggcontext);
    }
    
    /* Grow array if needed */
    if (state->result->u.array.count >= state->result->u.array.capacity)
    {
        size_t new_capacity = state->result->u.array.capacity == 0 ? 
                              8 : state->result->u.array.capacity * 2;
        if (state->result->u.array.items == NULL) {
            state->result->u.array.items = palloc(new_capacity * sizeof(kjson_value *));
        } else {
            state->result->u.array.items = repalloc(state->result->u.array.items,
                                                    new_capacity * sizeof(kjson_value *));
        }
        state->result->u.array.capacity = new_capacity;
    }
    
    /* Add to array */
    state->result->u.array.items[state->result->u.array.count++] = value;
    
    MemoryContextSwitchTo(oldcontext);
    PG_RETURN_POINTER(state);
}

/*
 * kjson_agg_finalfn - Final function for kjson_agg
 *
 * Returns the built array
 */
PG_FUNCTION_INFO_V1(kjson_agg_finalfn);
Datum
kjson_agg_finalfn(PG_FUNCTION_ARGS)
{
    KJsonAggState *state;
    PGKJson *result;
    StringInfo binary;
    
    if (PG_ARGISNULL(0))
    {
        /* No rows - return empty array */
        kjson_value empty;
        empty.type = KJSON_TYPE_ARRAY;
        empty.u.array.items = NULL;
        empty.u.array.count = 0;
        empty.u.array.capacity = 0;
        
        binary = pg_kjson_encode_binary(&empty);
    }
    else
    {
        state = (KJsonAggState *) PG_GETARG_POINTER(0);
        
        /* Encode the result */
        binary = pg_kjson_encode_binary(state->result);
    }
    
    /* Create PostgreSQL result */
    result = (PGKJson *) palloc(VARHDRSZ + binary->len);
    SET_VARSIZE(result, VARHDRSZ + binary->len);
    memcpy(PGKJSON_DATA(result), binary->data, binary->len);
    
    PG_RETURN_POINTER(result);
}

/*
 * kjson_object_agg_transfn - Transition function for kjson_object_agg
 *
 * Builds an object from key/value pairs
 */
PG_FUNCTION_INFO_V1(kjson_object_agg_transfn);
Datum
kjson_object_agg_transfn(PG_FUNCTION_ARGS)
{
    MemoryContext aggcontext;
    MemoryContext oldcontext;
    KJsonAggState *state;
    text         *key_text;
    char         *key;
    kjson_value  *value;
    Oid           valtype;
    kjson_member *member;
    
    if (!AggCheckCallContext(fcinfo, &aggcontext))
        elog(ERROR, "kjson_object_agg_transfn called in non-aggregate context");
    
    /* First call - initialize state */
    if (PG_ARGISNULL(0))
    {
        oldcontext = MemoryContextSwitchTo(aggcontext);
        
        state = (KJsonAggState *) palloc(sizeof(KJsonAggState));
        state->context = aggcontext;
        state->is_object_agg = true;
        
        /* Create empty object */
        state->result = (kjson_value *) palloc(sizeof(kjson_value));
        state->result->type = KJSON_TYPE_OBJECT;
        state->result->u.object.first = NULL;
        state->result->u.object.count = 0;
        
        MemoryContextSwitchTo(oldcontext);
    }
    else
    {
        state = (KJsonAggState *) PG_GETARG_POINTER(0);
    }
    
    /* Skip NULL values */
    if (PG_ARGISNULL(1) || PG_ARGISNULL(2))
        PG_RETURN_POINTER(state);
    
    /* Get the key */
    key_text = PG_GETARG_TEXT_PP(1);
    key = text_to_cstring(key_text);
    
    /* Get the value and convert to kjson */
    oldcontext = MemoryContextSwitchTo(aggcontext);
    
    valtype = get_fn_expr_argtype(fcinfo->flinfo, 2);
    
    /* Check if it's already a kjson type */
    if (valtype != InvalidOid) {
        char *typename = format_type_be(valtype);
        if (typename && strcmp(typename, "kjson") == 0) {
            /* It's already kjson, decode it */
            PGKJson *value_kjson = (PGKJson *) PG_DETOAST_DATUM(PG_GETARG_DATUM(2));
            value = pg_kjson_decode_binary(PGKJSON_DATA(value_kjson), 
                                          PGKJSON_DATA_SIZE(value_kjson), 
                                          aggcontext);
            if (!value) {
                pfree(key);
                MemoryContextSwitchTo(oldcontext);
                pfree(typename);
                ereport(ERROR,
                        (errcode(ERRCODE_INVALID_BINARY_REPRESENTATION),
                         errmsg("invalid kJSON value in object aggregate")));
            }
        } else {
            /* Convert SQL type to kjson */
            value = sql_value_to_kjson(PG_GETARG_DATUM(2), valtype, false, aggcontext);
        }
        if (typename) pfree(typename);
    } else {
        /* Unknown type, try to convert */
        value = sql_value_to_kjson(PG_GETARG_DATUM(2), InvalidOid, false, aggcontext);
    }
    
    /* Create new member */
    member = (kjson_member *) palloc(sizeof(kjson_member));
    member->key = pstrdup(key);
    member->value = value;
    member->next = NULL;
    
    /* Add to object (at end to preserve order) */
    if (state->result->u.object.first == NULL)
    {
        state->result->u.object.first = member;
    }
    else
    {
        kjson_member *last = state->result->u.object.first;
        while (last->next != NULL)
            last = last->next;
        last->next = member;
    }
    state->result->u.object.count++;
    
    MemoryContextSwitchTo(oldcontext);
    pfree(key);
    PG_RETURN_POINTER(state);
}

/*
 * kjson_object_agg_finalfn - Final function for kjson_object_agg
 *
 * Returns the built object
 */
PG_FUNCTION_INFO_V1(kjson_object_agg_finalfn);
Datum
kjson_object_agg_finalfn(PG_FUNCTION_ARGS)
{
    KJsonAggState *state;
    PGKJson *result;
    StringInfo binary;
    
    if (PG_ARGISNULL(0))
    {
        /* No rows - return empty object */
        kjson_value empty;
        empty.type = KJSON_TYPE_OBJECT;
        empty.u.object.first = NULL;
        empty.u.object.count = 0;
        
        binary = pg_kjson_encode_binary(&empty);
    }
    else
    {
        state = (KJsonAggState *) PG_GETARG_POINTER(0);
        
        /* Encode the result */
        binary = pg_kjson_encode_binary(state->result);
    }
    
    /* Create PostgreSQL result */
    result = (PGKJson *) palloc(VARHDRSZ + binary->len);
    SET_VARSIZE(result, VARHDRSZ + binary->len);
    memcpy(PGKJSON_DATA(result), binary->data, binary->len);
    
    PG_RETURN_POINTER(result);
}
/*
 * kjson_array_elements - Extract array elements as a set
 */
PG_FUNCTION_INFO_V1(kjson_array_elements);
Datum
kjson_array_elements(PG_FUNCTION_ARGS)
{
    FuncCallContext *funcctx;
    kjson_value     *array;
    kjson_value     *elem;
    int              call_cntr;
    int              max_calls;
    MemoryContext    oldcontext;
    StringInfo       binary;
    PGKJson         *result;
    
    if (SRF_IS_FIRSTCALL()) {
        PGKJson *pgkj = (PGKJson *) PG_DETOAST_DATUM(PG_GETARG_DATUM(0));
        
        funcctx = SRF_FIRSTCALL_INIT();
        oldcontext = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);
        
        /* Decode kjson value */
        array = pg_kjson_decode_binary(PGKJSON_DATA(pgkj), 
                                      PGKJSON_DATA_SIZE(pgkj), 
                                      funcctx->multi_call_memory_ctx);
        
        if (!array || array->type != KJSON_TYPE_ARRAY) {
            ereport(ERROR,
                    (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                     errmsg("kjson_array_elements() can only be applied to arrays")));
        }
        
        funcctx->max_calls = array->u.array.count;
        funcctx->user_fctx = array;
        
        MemoryContextSwitchTo(oldcontext);
    }
    
    funcctx = SRF_PERCALL_SETUP();
    call_cntr = funcctx->call_cntr;
    max_calls = funcctx->max_calls;
    
    if (call_cntr < max_calls) {
        array = (kjson_value *) funcctx->user_fctx;
        elem = array->u.array.items[call_cntr];
        
        /* Encode element to binary */
        binary = pg_kjson_encode_binary(elem);
        
        /* Create result */
        result = (PGKJson *) palloc(VARHDRSZ + binary->len);
        SET_VARSIZE(result, VARHDRSZ + binary->len);
        memcpy(PGKJSON_DATA(result), binary->data, binary->len);
        
        SRF_RETURN_NEXT(funcctx, PointerGetDatum(result));
    } else {
        SRF_RETURN_DONE(funcctx);
    }
}

/*
 * kjson_array_elements_text - Extract array elements as text
 */
PG_FUNCTION_INFO_V1(kjson_array_elements_text);
Datum
kjson_array_elements_text(PG_FUNCTION_ARGS)
{
    FuncCallContext *funcctx;
    kjson_value     *array;
    kjson_value     *elem;
    int              call_cntr;
    int              max_calls;
    MemoryContext    oldcontext;
    text            *result;
    kjson_write_options opts;
    char            *text_result;
    
    if (SRF_IS_FIRSTCALL()) {
        PGKJson *pgkj = (PGKJson *) PG_DETOAST_DATUM(PG_GETARG_DATUM(0));
        
        funcctx = SRF_FIRSTCALL_INIT();
        oldcontext = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);
        
        /* Decode kjson value */
        array = pg_kjson_decode_binary(PGKJSON_DATA(pgkj), 
                                      PGKJSON_DATA_SIZE(pgkj), 
                                      funcctx->multi_call_memory_ctx);
        
        if (!array || array->type != KJSON_TYPE_ARRAY) {
            ereport(ERROR,
                    (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                     errmsg("kjson_array_elements_text() can only be applied to arrays")));
        }
        
        funcctx->max_calls = array->u.array.count;
        funcctx->user_fctx = array;
        
        MemoryContextSwitchTo(oldcontext);
    }
    
    funcctx = SRF_PERCALL_SETUP();
    call_cntr = funcctx->call_cntr;
    max_calls = funcctx->max_calls;
    
    if (call_cntr < max_calls) {
        array = (kjson_value *) funcctx->user_fctx;
        elem = array->u.array.items[call_cntr];
        
        /* Convert to text based on type */
        if (elem->type == KJSON_TYPE_STRING) {
            result = cstring_to_text(elem->u.string);
        } else {
            /* Stringify other types */
            opts.indent = 0;
            opts.quote_keys = false;
            opts.bigint_suffix = true;
            opts.decimal_suffix = true;
            opts.escape_unicode = false;
            opts.use_single_quotes = false;
            
            text_result = pg_kjson_stringify(elem, &opts);
            result = cstring_to_text(text_result);
        }
        
        SRF_RETURN_NEXT(funcctx, PointerGetDatum(result));
    } else {
        SRF_RETURN_DONE(funcctx);
    }
}

/*
 * kjson_build_instant - Build kjson instant from kInstant
 */
PG_FUNCTION_INFO_V1(kjson_build_instant);
Datum
kjson_build_instant(PG_FUNCTION_ARGS)
{
    kInstant *instant = (kInstant *) PG_GETARG_POINTER(0);
    MemoryContext oldcontext;
    MemoryContext buildcontext;
    kjson_value *instant_value;
    StringInfo binary;
    PGKJson *result;
    
    /* Create temporary memory context */
    buildcontext = AllocSetContextCreate(CurrentMemoryContext,
                                        "kJSON instant build context",
                                        ALLOCSET_DEFAULT_SIZES);
    oldcontext = MemoryContextSwitchTo(buildcontext);
    
    /* Create kjson instant value */
    instant_value = MemoryContextAlloc(buildcontext, sizeof(kjson_value));
    instant_value->type = KJSON_TYPE_INSTANT;
    instant_value->u.instant = MemoryContextAlloc(buildcontext, sizeof(kInstant));
    memcpy(instant_value->u.instant, instant, sizeof(kInstant));
    
    /* Encode to binary */
    binary = pg_kjson_encode_binary(instant_value);
    
    /* Switch back to original context */
    MemoryContextSwitchTo(oldcontext);
    
    /* Create result */
    result = (PGKJson *) palloc(VARHDRSZ + binary->len);
    SET_VARSIZE(result, VARHDRSZ + binary->len);
    memcpy(PGKJSON_DATA(result), binary->data, binary->len);
    
    /* Clean up */
    MemoryContextDelete(buildcontext);
    
    PG_RETURN_POINTER(result);
}

/*
 * kjson_build_duration - Build kjson duration from kDuration
 */
PG_FUNCTION_INFO_V1(kjson_build_duration);
Datum
kjson_build_duration(PG_FUNCTION_ARGS)
{
    kDuration *duration = (kDuration *) PG_GETARG_POINTER(0);
    MemoryContext oldcontext;
    MemoryContext buildcontext;
    kjson_value *duration_value;
    StringInfo binary;
    PGKJson *result;
    
    /* Create temporary memory context */
    buildcontext = AllocSetContextCreate(CurrentMemoryContext,
                                        "kJSON duration build context",
                                        ALLOCSET_DEFAULT_SIZES);
    oldcontext = MemoryContextSwitchTo(buildcontext);
    
    /* Create kjson duration value */
    duration_value = MemoryContextAlloc(buildcontext, sizeof(kjson_value));
    duration_value->type = KJSON_TYPE_DURATION;
    duration_value->u.duration = MemoryContextAlloc(buildcontext, sizeof(kDuration));
    memcpy(duration_value->u.duration, duration, sizeof(kDuration));
    
    /* Encode to binary */
    binary = pg_kjson_encode_binary(duration_value);
    
    /* Switch back to original context */
    MemoryContextSwitchTo(oldcontext);
    
    /* Create result */
    result = (PGKJson *) palloc(VARHDRSZ + binary->len);
    SET_VARSIZE(result, VARHDRSZ + binary->len);
    memcpy(PGKJSON_DATA(result), binary->data, binary->len);
    
    /* Clean up */
    MemoryContextDelete(buildcontext);
    
    PG_RETURN_POINTER(result);
}

/*
 * kjson_extract_kinstant - Extract kInstant value from kjson path
 *
 * Usage: kjson_extract_kinstant(kjson_value, 'field1', 'field2', ...)
 */
PG_FUNCTION_INFO_V1(kjson_extract_kinstant);
Datum
kjson_extract_kinstant(PG_FUNCTION_ARGS)
{
    PGKJson      *pgkj = (PGKJson *) PG_DETOAST_DATUM(PG_GETARG_DATUM(0));
    struct ArrayType    *path = PG_GETARG_ARRAYTYPE_P(1);
    kjson_value  *value;
    kjson_value  *result;
    MemoryContext oldcontext;
    MemoryContext parsecontext;
    kInstant     *instant_result;
    
    /* Create temporary memory context */
    parsecontext = AllocSetContextCreate(CurrentMemoryContext,
                                         "kjson_extract_kinstant",
                                         ALLOCSET_DEFAULT_SIZES);
    oldcontext = MemoryContextSwitchTo(parsecontext);
    
    /* Decode from binary */
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
    
    /* Navigate path */
    result = kjson_get_path_value(value, path);
    
    /* Check if we found an instant */
    if (result == NULL || result->type != KJSON_TYPE_INSTANT)
    {
        MemoryContextSwitchTo(oldcontext);
        MemoryContextDelete(parsecontext);
        PG_RETURN_NULL();
    }
    
    /* Switch back to original context */
    MemoryContextSwitchTo(oldcontext);
    
    /* Create result - use the unified types */
    instant_result = (kInstant *) palloc(sizeof(kInstant));
    memcpy(instant_result, result->u.instant, sizeof(kInstant));
    
    MemoryContextDelete(parsecontext);
    PG_RETURN_POINTER(instant_result);
}

/*
 * kjson_extract_kduration - Extract kDuration value from kjson path
 *
 * Usage: kjson_extract_kduration(kjson_value, 'field1', 'field2', ...)
 */
PG_FUNCTION_INFO_V1(kjson_extract_kduration);
Datum
kjson_extract_kduration(PG_FUNCTION_ARGS)
{
    PGKJson      *pgkj = (PGKJson *) PG_DETOAST_DATUM(PG_GETARG_DATUM(0));
    struct ArrayType    *path = PG_GETARG_ARRAYTYPE_P(1);
    kjson_value  *value;
    kjson_value  *result;
    MemoryContext oldcontext;
    MemoryContext parsecontext;
    kDuration    *duration_result;
    
    /* Create temporary memory context */
    parsecontext = AllocSetContextCreate(CurrentMemoryContext,
                                         "kjson_extract_kduration",
                                         ALLOCSET_DEFAULT_SIZES);
    oldcontext = MemoryContextSwitchTo(parsecontext);
    
    /* Decode from binary */
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
    
    /* Navigate path */
    result = kjson_get_path_value(value, path);
    
    /* Check if we found a duration */
    if (result == NULL || result->type != KJSON_TYPE_DURATION)
    {
        MemoryContextSwitchTo(oldcontext);
        MemoryContextDelete(parsecontext);
        PG_RETURN_NULL();
    }
    
    /* Switch back to original context */
    MemoryContextSwitchTo(oldcontext);
    
    /* Create result - use the unified types */
    duration_result = (kDuration *) palloc(sizeof(kDuration));
    memcpy(duration_result, result->u.duration, sizeof(kDuration));
    
    MemoryContextDelete(parsecontext);
    PG_RETURN_POINTER(duration_result);
}
