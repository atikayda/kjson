/*
 * kjson_path_ops.c - Path extraction and existence operators for kjson
 */

#include "postgres.h"
#include "fmgr.h"
#include "funcapi.h"
#include "utils/builtins.h"
#include "utils/array.h"
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

/* Helper function to extract value at path */
static kjson_value *
kjson_get_path_value(kjson_value *root, ArrayType *path_array)
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
            /* Search for key in object */
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
            /* Try to parse as array index */
            char *endptr;
            long idx;
            
            idx = strtol(key, &endptr, 10);
            
            if (*endptr != '\0' || idx < 0 || idx >= current->u.array.count) {
                pfree(key);
                return NULL;
            }
            
            current = current->u.array.items[idx];
        }
        else {
            /* Can't traverse non-container types */
            pfree(key);
            return NULL;
        }
        
        pfree(key);
    }
    
    return current;
}

/*
 * kjson_extract_path - Extract kjson value at path
 */
PG_FUNCTION_INFO_V1(kjson_extract_path);
Datum
kjson_extract_path(PG_FUNCTION_ARGS)
{
    PGKJson *pgkj = (PGKJson *) PG_DETOAST_DATUM(PG_GETARG_DATUM(0));
    ArrayType *path = PG_GETARG_ARRAYTYPE_P(1);
    kjson_value *value;
    kjson_value *result;
    MemoryContext oldcontext;
    MemoryContext tmpcontext;
    StringInfo binary;
    PGKJson *ret;
    
    /* Create temporary context */
    tmpcontext = AllocSetContextCreate(CurrentMemoryContext,
                                       "kjson_extract_path",
                                       ALLOCSET_DEFAULT_SIZES);
    oldcontext = MemoryContextSwitchTo(tmpcontext);
    
    /* Decode from binary */
    value = pg_kjson_decode_binary(PGKJSON_DATA(pgkj), 
                                   PGKJSON_DATA_SIZE(pgkj), 
                                   tmpcontext);
    if (!value) {
        MemoryContextSwitchTo(oldcontext);
        MemoryContextDelete(tmpcontext);
        PG_RETURN_NULL();
    }
    
    /* Extract value at path */
    result = kjson_get_path_value(value, path);
    if (!result) {
        MemoryContextSwitchTo(oldcontext);
        MemoryContextDelete(tmpcontext);
        PG_RETURN_NULL();
    }
    
    /* Convert result to binary */
    binary = pg_kjson_encode_binary(result);
    
    /* Create result */
    ret = (PGKJson *) palloc(VARHDRSZ + binary->len);
    SET_VARSIZE(ret, VARHDRSZ + binary->len);
    memcpy(VARDATA(ret), binary->data, binary->len);
    
    /* Clean up */
    MemoryContextSwitchTo(oldcontext);
    MemoryContextDelete(tmpcontext);
    
    PG_RETURN_POINTER(ret);
}

/*
 * kjson_extract_path_text - Extract text value at path
 */
PG_FUNCTION_INFO_V1(kjson_extract_path_text);
Datum
kjson_extract_path_text(PG_FUNCTION_ARGS)
{
    PGKJson *pgkj = (PGKJson *) PG_DETOAST_DATUM(PG_GETARG_DATUM(0));
    ArrayType *path = PG_GETARG_ARRAYTYPE_P(1);
    kjson_value *value;
    kjson_value *result;
    MemoryContext oldcontext;
    MemoryContext tmpcontext;
    char *text_result;
    text *ret;
    
    /* Create temporary context */
    tmpcontext = AllocSetContextCreate(CurrentMemoryContext,
                                       "kjson_extract_path_text",
                                       ALLOCSET_DEFAULT_SIZES);
    oldcontext = MemoryContextSwitchTo(tmpcontext);
    
    /* Decode from binary */
    value = pg_kjson_decode_binary(PGKJSON_DATA(pgkj), 
                                   PGKJSON_DATA_SIZE(pgkj), 
                                   tmpcontext);
    if (!value) {
        MemoryContextSwitchTo(oldcontext);
        MemoryContextDelete(tmpcontext);
        PG_RETURN_NULL();
    }
    
    /* Extract value at path */
    result = kjson_get_path_value(value, path);
    if (!result) {
        MemoryContextSwitchTo(oldcontext);
        MemoryContextDelete(tmpcontext);
        PG_RETURN_NULL();
    }
    
    /* Convert to text based on type */
    text_result = NULL;
    
    switch (result->type) {
        case KJSON_TYPE_STRING:
            text_result = pstrdup(result->u.string);
            break;
            
        case KJSON_TYPE_NUMBER:
            {
                char buf[64];
                snprintf(buf, sizeof(buf), "%g", result->u.number);
                text_result = pstrdup(buf);
            }
            break;
            
        case KJSON_TYPE_BOOLEAN:
            text_result = pstrdup(result->u.boolean ? "true" : "false");
            break;
            
        case KJSON_TYPE_NULL:
            /* Return NULL for JSON null */
            MemoryContextSwitchTo(oldcontext);
            MemoryContextDelete(tmpcontext);
            PG_RETURN_NULL();
            
        default:
            /* For complex types, stringify them */
            {
                kjson_write_options opts = {
                    .indent = 0,
                    .quote_keys = false,
                    .bigint_suffix = true,
                    .decimal_suffix = true,
                    .escape_unicode = false,
                    .use_single_quotes = false
                };
                text_result = pg_kjson_stringify(result, &opts);
            }
            break;
    }
    
    /* Create text result */
    ret = cstring_to_text(text_result);
    
    /* Clean up */
    MemoryContextSwitchTo(oldcontext);
    MemoryContextDelete(tmpcontext);
    
    PG_RETURN_TEXT_P(ret);
}

/*
 * kjson_exists - Check if key exists in object
 */
PG_FUNCTION_INFO_V1(kjson_exists);
Datum
kjson_exists(PG_FUNCTION_ARGS)
{
    PGKJson *pgkj = (PGKJson *) PG_DETOAST_DATUM(PG_GETARG_DATUM(0));
    text *key_text = PG_GETARG_TEXT_PP(1);
    char *key = text_to_cstring(key_text);
    kjson_value *value;
    bool exists = false;
    MemoryContext oldcontext;
    MemoryContext tmpcontext;
    
    /* Create temporary context */
    tmpcontext = AllocSetContextCreate(CurrentMemoryContext,
                                       "kjson_exists",
                                       ALLOCSET_DEFAULT_SIZES);
    oldcontext = MemoryContextSwitchTo(tmpcontext);
    
    /* Decode from binary */
    value = pg_kjson_decode_binary(PGKJSON_DATA(pgkj), 
                                   PGKJSON_DATA_SIZE(pgkj), 
                                   tmpcontext);
    
    if (value && value->type == KJSON_TYPE_OBJECT) {
        kjson_member *member = value->u.object.first;
        while (member) {
            if (strcmp(member->key, key) == 0) {
                exists = true;
                break;
            }
            member = member->next;
        }
    }
    
    /* Clean up */
    MemoryContextSwitchTo(oldcontext);
    MemoryContextDelete(tmpcontext);
    pfree(key);
    
    PG_RETURN_BOOL(exists);
}

/*
 * kjson_exists_any - Check if any of the keys exist
 */
PG_FUNCTION_INFO_V1(kjson_exists_any);
Datum
kjson_exists_any(PG_FUNCTION_ARGS)
{
    PGKJson *pgkj = (PGKJson *) PG_DETOAST_DATUM(PG_GETARG_DATUM(0));
    ArrayType *keys_array = PG_GETARG_ARRAYTYPE_P(1);
    kjson_value *value;
    bool exists = false;
    int keys_len;
    text **keys;
    MemoryContext oldcontext;
    MemoryContext tmpcontext;
    
    /* Create temporary context */
    tmpcontext = AllocSetContextCreate(CurrentMemoryContext,
                                       "kjson_exists_any",
                                       ALLOCSET_DEFAULT_SIZES);
    oldcontext = MemoryContextSwitchTo(tmpcontext);
    
    /* Decode from binary */
    value = pg_kjson_decode_binary(PGKJSON_DATA(pgkj), 
                                   PGKJSON_DATA_SIZE(pgkj), 
                                   tmpcontext);
    
    if (value && value->type == KJSON_TYPE_OBJECT) {
        /* Get array of keys to check */
        deconstruct_array(keys_array, TEXTOID, -1, false, 'i',
                          (Datum **) &keys, NULL, &keys_len);
        
        /* Check each key */
        for (int i = 0; i < keys_len && !exists; i++) {
            char *key = text_to_cstring(keys[i]);
            kjson_member *member = value->u.object.first;
            
            while (member) {
                if (strcmp(member->key, key) == 0) {
                    exists = true;
                    break;
                }
                member = member->next;
            }
            
            pfree(key);
        }
    }
    
    /* Clean up */
    MemoryContextSwitchTo(oldcontext);
    MemoryContextDelete(tmpcontext);
    
    PG_RETURN_BOOL(exists);
}

/*
 * kjson_exists_all - Check if all keys exist
 */
PG_FUNCTION_INFO_V1(kjson_exists_all);
Datum
kjson_exists_all(PG_FUNCTION_ARGS)
{
    PGKJson *pgkj = (PGKJson *) PG_DETOAST_DATUM(PG_GETARG_DATUM(0));
    ArrayType *keys_array = PG_GETARG_ARRAYTYPE_P(1);
    kjson_value *value;
    bool all_exist = true;
    int keys_len;
    text **keys;
    MemoryContext oldcontext;
    MemoryContext tmpcontext;
    
    /* Create temporary context */
    tmpcontext = AllocSetContextCreate(CurrentMemoryContext,
                                       "kjson_exists_all",
                                       ALLOCSET_DEFAULT_SIZES);
    oldcontext = MemoryContextSwitchTo(tmpcontext);
    
    /* Decode from binary */
    value = pg_kjson_decode_binary(PGKJSON_DATA(pgkj), 
                                   PGKJSON_DATA_SIZE(pgkj), 
                                   tmpcontext);
    
    if (value && value->type == KJSON_TYPE_OBJECT) {
        /* Get array of keys to check */
        deconstruct_array(keys_array, TEXTOID, -1, false, 'i',
                          (Datum **) &keys, NULL, &keys_len);
        
        /* Check each key */
        for (int i = 0; i < keys_len && all_exist; i++) {
            char *key = text_to_cstring(keys[i]);
            kjson_member *member = value->u.object.first;
            bool found = false;
            
            while (member) {
                if (strcmp(member->key, key) == 0) {
                    found = true;
                    break;
                }
                member = member->next;
            }
            
            if (!found) {
                all_exist = false;
            }
            
            pfree(key);
        }
    } else {
        /* Not an object, so keys don't exist */
        all_exist = false;
    }
    
    /* Clean up */
    MemoryContextSwitchTo(oldcontext);
    MemoryContextDelete(tmpcontext);
    
    PG_RETURN_BOOL(all_exist);
}

/*
 * kjson_array_length - Get array length
 */
PG_FUNCTION_INFO_V1(kjson_array_length);
Datum
kjson_array_length(PG_FUNCTION_ARGS)
{
    PGKJson *pgkj = (PGKJson *) PG_DETOAST_DATUM(PG_GETARG_DATUM(0));
    kjson_value *value;
    int32 length = 0;
    MemoryContext oldcontext;
    MemoryContext tmpcontext;
    
    /* Create temporary context */
    tmpcontext = AllocSetContextCreate(CurrentMemoryContext,
                                       "kjson_array_length",
                                       ALLOCSET_DEFAULT_SIZES);
    oldcontext = MemoryContextSwitchTo(tmpcontext);
    
    /* Decode from binary */
    value = pg_kjson_decode_binary(PGKJSON_DATA(pgkj), 
                                   PGKJSON_DATA_SIZE(pgkj), 
                                   tmpcontext);
    
    if (value && value->type == KJSON_TYPE_ARRAY) {
        length = value->u.array.count;
    } else {
        /* Not an array - could error or return NULL */
        ereport(ERROR,
                (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                 errmsg("kjson_array_length() can only be applied to arrays")));
    }
    
    /* Clean up */
    MemoryContextSwitchTo(oldcontext);
    MemoryContextDelete(tmpcontext);
    
    PG_RETURN_INT32(length);
}

/*
 * kjson_object_keys - Return set of object keys
 */
PG_FUNCTION_INFO_V1(kjson_object_keys);
Datum
kjson_object_keys(PG_FUNCTION_ARGS)
{
    FuncCallContext *funcctx;
    kjson_value *value;
    kjson_member **keys_array;
    int call_cntr;
    int max_calls;
    
    if (SRF_IS_FIRSTCALL()) {
        MemoryContext oldcontext;
        PGKJson *pgkj = (PGKJson *) PG_DETOAST_DATUM(PG_GETARG_DATUM(0));
        
        funcctx = SRF_FIRSTCALL_INIT();
        oldcontext = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);
        
        /* Decode kjson value */
        value = pg_kjson_decode_binary(PGKJSON_DATA(pgkj), 
                                      PGKJSON_DATA_SIZE(pgkj), 
                                      funcctx->multi_call_memory_ctx);
        
        if (!value || value->type != KJSON_TYPE_OBJECT) {
            ereport(ERROR,
                    (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                     errmsg("kjson_object_keys() can only be applied to objects")));
        }
        
        /* Build array of members for iteration */
        funcctx->max_calls = value->u.object.count;
        if (funcctx->max_calls > 0) {
            kjson_member *member;
            int i;
            
            keys_array = (kjson_member **) palloc(sizeof(kjson_member *) * funcctx->max_calls);
            member = value->u.object.first;
            i = 0;
            
            while (member) {
                keys_array[i++] = member;
                member = member->next;
            }
            
            funcctx->user_fctx = keys_array;
        }
        
        MemoryContextSwitchTo(oldcontext);
    }
    
    funcctx = SRF_PERCALL_SETUP();
    call_cntr = funcctx->call_cntr;
    max_calls = funcctx->max_calls;
    
    if (call_cntr < max_calls) {
        text *key_text;
        
        keys_array = (kjson_member **) funcctx->user_fctx;
        key_text = cstring_to_text(keys_array[call_cntr]->key);
        
        SRF_RETURN_NEXT(funcctx, PointerGetDatum(key_text));
    } else {
        SRF_RETURN_DONE(funcctx);
    }
}