/*
 * kjson_cmp.c - Comparison functions for kjson type
 *
 * This file implements comparison operators needed for
 * ORDER BY, DISTINCT, and btree indexes.
 */

#include "postgres.h"
#include "fmgr.h"
#include "utils/builtins.h"
#include "lib/stringinfo.h"
#include "utils/memutils.h"

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

/* Forward declaration */
static int compare_kjson_values(kjson_value *a, kjson_value *b);

/*
 * Type ordering for comparison:
 * null < boolean < number < string < array < object < extended types
 */
static int
get_type_order(kjson_type type)
{
    switch (type) {
        case KJSON_TYPE_NULL:       return 0;
        case KJSON_TYPE_BOOLEAN:    return 1;
        case KJSON_TYPE_NUMBER:     return 2;
        case KJSON_TYPE_BIGINT:     return 3;
        case KJSON_TYPE_DECIMAL128: return 4;
        case KJSON_TYPE_STRING:     return 5;
        case KJSON_TYPE_UUID:       return 6;
        case KJSON_TYPE_INSTANT:    return 7;
        case KJSON_TYPE_ARRAY:      return 8;
        case KJSON_TYPE_OBJECT:     return 9;
        default:                    return 10;
    }
}

/*
 * Compare two kjson values recursively
 * Returns: -1 if a < b, 0 if a == b, 1 if a > b
 */
static int
compare_kjson_values(kjson_value *a, kjson_value *b)
{
    int type_order_a;
    int type_order_b;
    
    /* Handle NULL values */
    if (!a && !b) return 0;
    if (!a) return -1;
    if (!b) return 1;
    
    /* Compare types first */
    type_order_a = get_type_order(a->type);
    type_order_b = get_type_order(b->type);
    
    if (type_order_a != type_order_b)
        return (type_order_a < type_order_b) ? -1 : 1;
    
    /* Same type, compare values */
    switch (a->type) {
        case KJSON_TYPE_NULL:
            return 0;
            
        case KJSON_TYPE_BOOLEAN:
            if (a->u.boolean == b->u.boolean) return 0;
            return a->u.boolean ? 1 : -1;
            
        case KJSON_TYPE_NUMBER:
            if (a->u.number < b->u.number) return -1;
            if (a->u.number > b->u.number) return 1;
            return 0;
            
        case KJSON_TYPE_STRING:
            if (!a->u.string && !b->u.string) return 0;
            if (!a->u.string) return -1;
            if (!b->u.string) return 1;
            return strcmp(a->u.string, b->u.string);
            
        case KJSON_TYPE_BIGINT:
            {
                int len_cmp;
                int result;
                int digit_cmp;
                
                /* Compare BigInt values */
                if (!a->u.bigint && !b->u.bigint) return 0;
                if (!a->u.bigint) return -1;
                if (!b->u.bigint) return 1;
                
                /* Compare signs */
                if (a->u.bigint->negative != b->u.bigint->negative)
                    return a->u.bigint->negative ? -1 : 1;
                
                /* Same sign, compare magnitudes */
                len_cmp = strlen(a->u.bigint->digits) - strlen(b->u.bigint->digits);
                if (len_cmp != 0) {
                    /* Different lengths, longer is bigger (considering sign) */
                    result = (len_cmp > 0) ? 1 : -1;
                    return a->u.bigint->negative ? -result : result;
                }
                
                /* Same length, compare digit by digit */
                digit_cmp = strcmp(a->u.bigint->digits, b->u.bigint->digits);
                return a->u.bigint->negative ? -digit_cmp : digit_cmp;
            }
            
        case KJSON_TYPE_DECIMAL128:
            {
                kjson_write_options opts = { .decimal_suffix = false };
                char *str_a;
                char *str_b;
                int result;
                
                /* Compare Decimal128 values */
                if (!a->u.decimal && !b->u.decimal) return 0;
                if (!a->u.decimal) return -1;
                if (!b->u.decimal) return 1;
                
                /* TODO: Proper decimal comparison considering exponent */
                str_a = pg_kjson_stringify(a, &opts);
                str_b = pg_kjson_stringify(b, &opts);
                result = strcmp(str_a, str_b);
                pfree(str_a);
                pfree(str_b);
                return result;
            }
            
        case KJSON_TYPE_UUID:
            if (!a->u.uuid && !b->u.uuid) return 0;
            if (!a->u.uuid) return -1;
            if (!b->u.uuid) return 1;
            return memcmp(a->u.uuid->bytes, b->u.uuid->bytes, 16);
            
        case KJSON_TYPE_INSTANT:
            if (!a->u.instant && !b->u.instant) return 0;
            if (!a->u.instant) return -1;
            if (!b->u.instant) return 1;
            if (a->u.instant->nanoseconds < b->u.instant->nanoseconds) return -1;
            if (a->u.instant->nanoseconds > b->u.instant->nanoseconds) return 1;
            return 0;
            
        case KJSON_TYPE_ARRAY:
            {
                /* Compare arrays element by element */
                size_t min_len = (a->u.array.count < b->u.array.count) ? 
                                a->u.array.count : b->u.array.count;
                
                for (size_t i = 0; i < min_len; i++) {
                    int elem_cmp = compare_kjson_values(a->u.array.items[i], 
                                                       b->u.array.items[i]);
                    if (elem_cmp != 0) return elem_cmp;
                }
                
                /* All common elements equal, shorter array is less */
                if (a->u.array.count < b->u.array.count) return -1;
                if (a->u.array.count > b->u.array.count) return 1;
                return 0;
            }
            
        case KJSON_TYPE_OBJECT:
            {
                /* Compare objects by converting to sorted key order */
                /* This is expensive but ensures consistent ordering */
                kjson_write_options opts = { .quote_keys = true };
                char *str_a = pg_kjson_stringify(a, &opts);
                char *str_b = pg_kjson_stringify(b, &opts);
                int result = strcmp(str_a, str_b);
                pfree(str_a);
                pfree(str_b);
                return result;
            }
            
        default:
            /* Unknown types are equal */
            return 0;
    }
}

/*
 * kjson_cmp - Main comparison function
 *
 * Returns: -1, 0, or 1 for less than, equal, or greater than
 */
PG_FUNCTION_INFO_V1(kjson_cmp);
Datum
kjson_cmp(PG_FUNCTION_ARGS)
{
    PGKJson *a = (PGKJson *) PG_DETOAST_DATUM(PG_GETARG_DATUM(0));
    PGKJson *b = (PGKJson *) PG_DETOAST_DATUM(PG_GETARG_DATUM(1));
    MemoryContext oldcontext;
    MemoryContext cmpcontext;
    int result;
    kjson_value *val_a;
    kjson_value *val_b;
    
    /* Create temporary memory context */
    cmpcontext = AllocSetContextCreate(CurrentMemoryContext,
                                       "kJSON comparison context",
                                       ALLOCSET_DEFAULT_SIZES);
    oldcontext = MemoryContextSwitchTo(cmpcontext);
    
    /* Decode both values */
    val_a = pg_kjson_decode_binary(PGKJSON_DATA(a), 
                                                PGKJSON_DATA_SIZE(a), 
                                                cmpcontext);
    val_b = pg_kjson_decode_binary(PGKJSON_DATA(b), 
                                                PGKJSON_DATA_SIZE(b), 
                                                cmpcontext);
    
    if (!val_a || !val_b) {
        MemoryContextSwitchTo(oldcontext);
        MemoryContextDelete(cmpcontext);
        ereport(ERROR,
                (errcode(ERRCODE_INVALID_BINARY_REPRESENTATION),
                 errmsg("corrupt kjson binary data in comparison")));
    }
    
    /* Compare the values */
    result = compare_kjson_values(val_a, val_b);
    
    /* Clean up */
    MemoryContextSwitchTo(oldcontext);
    MemoryContextDelete(cmpcontext);
    
    PG_RETURN_INT32(result);
}

/*
 * Comparison operators using kjson_cmp
 */
 
PG_FUNCTION_INFO_V1(kjson_lt);
Datum
kjson_lt(PG_FUNCTION_ARGS)
{
    int32 result = DatumGetInt32(DirectFunctionCall2(kjson_cmp,
                                                     PG_GETARG_DATUM(0),
                                                     PG_GETARG_DATUM(1)));
    PG_RETURN_BOOL(result < 0);
}

PG_FUNCTION_INFO_V1(kjson_le);
Datum
kjson_le(PG_FUNCTION_ARGS)
{
    int32 result = DatumGetInt32(DirectFunctionCall2(kjson_cmp,
                                                     PG_GETARG_DATUM(0),
                                                     PG_GETARG_DATUM(1)));
    PG_RETURN_BOOL(result <= 0);
}

PG_FUNCTION_INFO_V1(kjson_gt);
Datum
kjson_gt(PG_FUNCTION_ARGS)
{
    int32 result = DatumGetInt32(DirectFunctionCall2(kjson_cmp,
                                                     PG_GETARG_DATUM(0),
                                                     PG_GETARG_DATUM(1)));
    PG_RETURN_BOOL(result > 0);
}

PG_FUNCTION_INFO_V1(kjson_ge);
Datum
kjson_ge(PG_FUNCTION_ARGS)
{
    int32 result = DatumGetInt32(DirectFunctionCall2(kjson_cmp,
                                                     PG_GETARG_DATUM(0),
                                                     PG_GETARG_DATUM(1)));
    PG_RETURN_BOOL(result >= 0);
}