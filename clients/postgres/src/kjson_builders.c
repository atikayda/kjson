/*
 * kjson_builders.c - Builder functions for constructing kjson values
 */

#include "postgres.h"
#include "fmgr.h"
#include "funcapi.h"
#include "utils/builtins.h"
#include "utils/lsyscache.h"
#include "utils/typcache.h"
#include "utils/numeric.h"
#include "utils/timestamp.h"
#include "utils/uuid.h"
#include "catalog/pg_type.h"
#include "access/htup_details.h"
#include "utils/syscache.h"
#include <ctype.h>
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

/* UUID access macro */
#define DatumGetUuid(x) ((pg_uuid_t *) DatumGetPointer(x))

/* PostgreSQL epoch dates */
#define POSTGRES_EPOCH_JDATE 2451545
#define UNIX_EPOCH_JDATE 2440588

/* Helper to convert any SQL value to kjson */
kjson_value *
sql_value_to_kjson(Datum value, Oid typoid, bool isnull, MemoryContext memctx)
{
    kjson_value *result;
    
    if (isnull) {
        result = palloc(sizeof(kjson_value));
        result->type = KJSON_TYPE_NULL;
        return result;
    }
    
    /* Check if this is a kjson type by name */
    if (typoid != InvalidOid) {
        char *typename = format_type_be(typoid);
        if (typename && strcmp(typename, "kjson") == 0) {
            /* It's already a kjson value, decode it */
            PGKJson *pgkj = (PGKJson *) DatumGetPointer(value);
            result = pg_kjson_decode_binary(PGKJSON_DATA(pgkj), 
                                          PGKJSON_DATA_SIZE(pgkj), 
                                          memctx);
            if (!result) {
                ereport(ERROR,
                        (errcode(ERRCODE_INVALID_BINARY_REPRESENTATION),
                         errmsg("corrupt kjson binary data in nested value")));
            }
            pfree(typename);
            return result;
        }
        if (typename) pfree(typename);
    }
    
    switch (typoid) {
        case BOOLOID:
            result = palloc(sizeof(kjson_value));
            result->type = KJSON_TYPE_BOOLEAN;
            result->u.boolean = DatumGetBool(value);
            break;
            
        case INT2OID:
        case INT4OID:
            result = palloc(sizeof(kjson_value));
            result->type = KJSON_TYPE_NUMBER;
            if (typoid == INT2OID)
                result->u.number = (double) DatumGetInt16(value);
            else
                result->u.number = (double) DatumGetInt32(value);
            break;
            
        case INT8OID:
            {
                /* Store INT8 as BigInt to preserve precision */
                int64 val;
                char buf[32];
                
                val = DatumGetInt64(value);
                result = palloc(sizeof(kjson_value));
                result->type = KJSON_TYPE_BIGINT;
                result->u.bigint = palloc(sizeof(kjson_bigint));
                
                /* Convert to string */
                snprintf(buf, sizeof(buf), "%lld", (long long)val);
                
                result->u.bigint->negative = (val < 0);
                if (result->u.bigint->negative) {
                    result->u.bigint->digits = pstrdup(buf + 1); /* Skip minus sign */
                } else {
                    result->u.bigint->digits = pstrdup(buf);
                }
            }
            break;
            
        case FLOAT4OID:
        case FLOAT8OID:
            result = palloc(sizeof(kjson_value));
            result->type = KJSON_TYPE_NUMBER;
            if (typoid == FLOAT4OID)
                result->u.number = (double) DatumGetFloat4(value);
            else
                result->u.number = DatumGetFloat8(value);
            break;
            
        case NUMERICOID:
            {
                /* Convert numeric to BigInt or Decimal128 based on scale */
                Numeric num;
                char *str;
                
                num = DatumGetNumeric(value);
                str = DatumGetCString(DirectFunctionCall1(numeric_out, NumericGetDatum(num)));
                
                result = palloc(sizeof(kjson_value));
                
                /* If it has no decimal point, treat as BigInt */
                if (strchr(str, '.') == NULL && strchr(str, 'e') == NULL && strchr(str, 'E') == NULL) {
                    result->type = KJSON_TYPE_BIGINT;
                    result->u.bigint = palloc(sizeof(kjson_bigint));
                    result->u.bigint->negative = (str[0] == '-');
                    if (result->u.bigint->negative) {
                        result->u.bigint->digits = pstrdup(str + 1);
                    } else {
                        result->u.bigint->digits = pstrdup(str);
                    }
                } else {
                    /* Otherwise treat as Decimal128 */
                    char *digits_buf;
                    char *p;
                    char *d;
                    int exp;
                    bool found_dot;
                    int sci_exp;
                    
                    result->type = KJSON_TYPE_DECIMAL128;
                    result->u.decimal = palloc(sizeof(kjson_decimal128));
                    result->u.decimal->negative = (str[0] == '-');
                    
                    /* Parse the decimal to extract digits and exponent */
                    
                    digits_buf = palloc(strlen(str) + 1);
                    p = str;
                    d = digits_buf;
                    exp = 0;
                    found_dot = false;
                    
                    /* Skip minus sign */
                    if (*p == '-') p++;
                    
                    /* Copy digits and track decimal position */
                    while (*p) {
                        if (*p == '.') {
                            found_dot = true;
                        } else if (isdigit(*p)) {
                            *d++ = *p;
                            if (found_dot) exp--;
                        } else if (*p == 'e' || *p == 'E') {
                            /* Handle scientific notation */
                            p++;
                            sci_exp = atoi(p);
                            exp += sci_exp;
                            break;
                        }
                        p++;
                    }
                    *d = '\0';
                    
                    result->u.decimal->digits = pstrdup(digits_buf);
                    result->u.decimal->exp = exp;
                    pfree(digits_buf);
                }
                pfree(str);
            }
            break;
            
        case TEXTOID:
        case VARCHAROID:
        case BPCHAROID:
            result = palloc(sizeof(kjson_value));
            result->type = KJSON_TYPE_STRING;
            result->u.string = text_to_cstring(DatumGetTextPP(value));
            break;
            
        case UUIDOID:
            {
                pg_uuid_t *uuid;
                
                uuid = DatumGetUuid(value);
                result = palloc(sizeof(kjson_value));
                result->type = KJSON_TYPE_UUID;
                result->u.uuid = palloc(sizeof(kjson_uuid));
                memcpy(result->u.uuid->bytes, uuid->data, 16);
            }
            break;
            
        case TIMESTAMPOID:
        case TIMESTAMPTZOID:
            {
                Timestamp ts;
                
                int64 pg_epoch_offset_microseconds;
                int64 unix_microseconds;
                
                ts = DatumGetTimestamp(value);
                result = palloc(sizeof(kjson_value));
                result->type = KJSON_TYPE_INSTANT;
                result->u.instant = palloc(sizeof(kjson_instant));
                /* Convert PostgreSQL timestamp to nanoseconds since epoch */
                /* PostgreSQL timestamps are microseconds since 2000-01-01, convert to nanoseconds since 1970-01-01 */
                pg_epoch_offset_microseconds = 946684800000000LL;
                unix_microseconds = ts + pg_epoch_offset_microseconds;
                result->u.instant->nanoseconds = unix_microseconds * 1000LL;
                result->u.instant->tz_offset = 0; /* TODO: Handle timezone */
            }
            break;
            
        default:
            {
                /* For other types, convert to text */
                Oid output_func;
                bool is_varlena;
                char *str;
                
                getTypeOutputInfo(typoid, &output_func, &is_varlena);
                str = OidOutputFunctionCall(output_func, value);
                
                result = palloc(sizeof(kjson_value));
                result->type = KJSON_TYPE_STRING;
                result->u.string = str;
            }
            break;
    }
    
    return result;
}

/*
 * kjson_build_object - Build a kjson object from key/value pairs
 * 
 * Usage: kjson_build_object('key1', value1, 'key2', value2, ...)
 */
PG_FUNCTION_INFO_V1(kjson_build_object);
Datum
kjson_build_object(PG_FUNCTION_ARGS)
{
    int nargs = PG_NARGS();
    kjson_value *object;
    kjson_member *last_member = NULL;
    MemoryContext oldcontext;
    MemoryContext objcontext;
    StringInfo binary;
    PGKJson *result;
    
    /* Must have even number of arguments (key/value pairs) */
    if (nargs % 2 != 0) {
        ereport(ERROR,
                (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                 errmsg("kjson_build_object() requires an even number of arguments")));
    }
    
    /* Create memory context for building */
    objcontext = AllocSetContextCreate(CurrentMemoryContext,
                                       "kjson_build_object context",
                                       ALLOCSET_DEFAULT_SIZES);
    oldcontext = MemoryContextSwitchTo(objcontext);
    
    /* Create object */
    object = palloc(sizeof(kjson_value));
    object->type = KJSON_TYPE_OBJECT;
    object->u.object.first = NULL;
    object->u.object.count = 0;
    
    /* Process key/value pairs */
    for (int i = 0; i < nargs; i += 2) {
        char *key;
        kjson_value *value;
        kjson_member *member;
        Oid key_type;
        Oid val_type;
        
        /* Get key (must be text) */
        if (PG_ARGISNULL(i)) {
            ereport(ERROR,
                    (errcode(ERRCODE_NULL_VALUE_NOT_ALLOWED),
                     errmsg("kjson object keys cannot be null")));
        }
        
        /* Convert key to text if needed */
        key_type = get_fn_expr_argtype(fcinfo->flinfo, i);
        if (key_type == TEXTOID || key_type == VARCHAROID || key_type == BPCHAROID) {
            key = text_to_cstring(PG_GETARG_TEXT_PP(i));
        } else {
            Oid output_func;
            bool is_varlena;
            getTypeOutputInfo(key_type, &output_func, &is_varlena);
            key = OidOutputFunctionCall(output_func, PG_GETARG_DATUM(i));
        }
        
        /* Get value */
        val_type = get_fn_expr_argtype(fcinfo->flinfo, i + 1);
        value = sql_value_to_kjson(PG_GETARG_DATUM(i + 1), 
                                   val_type,
                                   PG_ARGISNULL(i + 1),
                                   objcontext);
        
        /* Create member */
        member = palloc(sizeof(kjson_member));
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
    }
    
    /* Convert to binary format */
    binary = pg_kjson_encode_binary(object);
    
    /* Create result */
    result = (PGKJson *) palloc(VARHDRSZ + binary->len);
    SET_VARSIZE(result, VARHDRSZ + binary->len);
    memcpy(VARDATA(result), binary->data, binary->len);
    
    /* Clean up */
    MemoryContextSwitchTo(oldcontext);
    MemoryContextDelete(objcontext);
    
    PG_RETURN_POINTER(result);
}

/*
 * kjson_build_array - Build a kjson array from values
 * 
 * Usage: kjson_build_array(value1, value2, ...)
 */
PG_FUNCTION_INFO_V1(kjson_build_array);
Datum
kjson_build_array(PG_FUNCTION_ARGS)
{
    int nargs = PG_NARGS();
    kjson_value *array;
    MemoryContext oldcontext;
    MemoryContext arrcontext;
    StringInfo binary;
    PGKJson *result;
    
    /* Create memory context for building */
    arrcontext = AllocSetContextCreate(CurrentMemoryContext,
                                       "kjson_build_array context",
                                       ALLOCSET_DEFAULT_SIZES);
    oldcontext = MemoryContextSwitchTo(arrcontext);
    
    /* Create array */
    array = palloc(sizeof(kjson_value));
    array->type = KJSON_TYPE_ARRAY;
    array->u.array.items = palloc(sizeof(kjson_value *) * nargs);
    array->u.array.count = nargs;
    array->u.array.capacity = nargs;
    
    /* Process values */
    for (int i = 0; i < nargs; i++) {
        Oid val_type = get_fn_expr_argtype(fcinfo->flinfo, i);
        array->u.array.items[i] = sql_value_to_kjson(PG_GETARG_DATUM(i),
                                                      val_type,
                                                      PG_ARGISNULL(i),
                                                      arrcontext);
    }
    
    /* Convert to binary format */
    binary = pg_kjson_encode_binary(array);
    
    /* Create result */
    result = (PGKJson *) palloc(VARHDRSZ + binary->len);
    SET_VARSIZE(result, VARHDRSZ + binary->len);
    memcpy(VARDATA(result), binary->data, binary->len);
    
    /* Clean up */
    MemoryContextSwitchTo(oldcontext);
    MemoryContextDelete(arrcontext);
    
    PG_RETURN_POINTER(result);
}

/*
 * row_to_kjson - Convert a row to kjson object
 */
PG_FUNCTION_INFO_V1(row_to_kjson);
Datum
row_to_kjson(PG_FUNCTION_ARGS)
{
    Datum record = PG_GETARG_DATUM(0);
    kjson_value *object;
    kjson_member *last_member = NULL;
    HeapTupleHeader td;
    Oid tupType;
    int32 tupTypmod;
    TupleDesc tupdesc;
    HeapTupleData tmptup;
    MemoryContext oldcontext;
    MemoryContext objcontext;
    StringInfo binary;
    PGKJson *result;
    
    /* Create memory context */
    objcontext = AllocSetContextCreate(CurrentMemoryContext,
                                       "row_to_kjson context",
                                       ALLOCSET_DEFAULT_SIZES);
    oldcontext = MemoryContextSwitchTo(objcontext);
    
    /* Get tuple descriptor */
    td = DatumGetHeapTupleHeader(record);
    tupType = HeapTupleHeaderGetTypeId(td);
    tupTypmod = HeapTupleHeaderGetTypMod(td);
    tupdesc = lookup_rowtype_tupdesc(tupType, tupTypmod);
    
    /* Build a temporary HeapTuple */
    tmptup.t_len = HeapTupleHeaderGetDatumLength(td);
    tmptup.t_data = td;
    
    /* Create object */
    object = palloc(sizeof(kjson_value));
    object->type = KJSON_TYPE_OBJECT;
    object->u.object.first = NULL;
    object->u.object.count = 0;
    
    /* Process each column */
    for (int i = 0; i < tupdesc->natts; i++) {
        Form_pg_attribute att = TupleDescAttr(tupdesc, i);
        Datum val;
        bool isnull;
        kjson_member *member;
        char *key;
        kjson_value *value;
        
        /* Skip dropped columns */
        if (att->attisdropped)
            continue;
            
        /* Get column name and value */
        key = NameStr(att->attname);
        val = heap_getattr(&tmptup, i + 1, tupdesc, &isnull);
        
        /* Convert value to kjson */
        value = sql_value_to_kjson(val, att->atttypid, isnull, objcontext);
        
        /* Create member */
        member = palloc(sizeof(kjson_member));
        member->key = pstrdup(key);
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
    }
    
    /* Release tuple descriptor */
    ReleaseTupleDesc(tupdesc);
    
    /* Convert to binary format */
    binary = pg_kjson_encode_binary(object);
    
    /* Create result */
    result = (PGKJson *) palloc(VARHDRSZ + binary->len);
    SET_VARSIZE(result, VARHDRSZ + binary->len);
    memcpy(VARDATA(result), binary->data, binary->len);
    
    /* Clean up */
    MemoryContextSwitchTo(oldcontext);
    MemoryContextDelete(objcontext);
    
    PG_RETURN_POINTER(result);
}