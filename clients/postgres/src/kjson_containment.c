/*
 * kjson_containment.c - Containment operators for kjson
 */

#include "postgres.h"
#include "fmgr.h"
#include "utils/memutils.h"
#include "lib/stringinfo.h"
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

/* Forward declarations */
static bool kjson_contains_internal(kjson_value *container, kjson_value *contained);
static bool kjson_values_equal(kjson_value *a, kjson_value *b);
static kjson_value *strip_nulls_internal(kjson_value *value, MemoryContext memctx);

/*
 * Check if two kjson values are equal
 */
static bool
kjson_values_equal(kjson_value *a, kjson_value *b)
{
    if (!a && !b)
        return true;
    if (!a || !b)
        return false;
    if (a->type != b->type)
        return false;
        
    switch (a->type) {
        case KJSON_TYPE_NULL:
            return true;
            
        case KJSON_TYPE_BOOLEAN:
            return a->u.boolean == b->u.boolean;
            
        case KJSON_TYPE_NUMBER:
            return a->u.number == b->u.number;
            
        case KJSON_TYPE_STRING:
            return strcmp(a->u.string, b->u.string) == 0;
            
        case KJSON_TYPE_BIGINT:
            return a->u.bigint->negative == b->u.bigint->negative &&
                   strcmp(a->u.bigint->digits, b->u.bigint->digits) == 0;
                   
        case KJSON_TYPE_DECIMAL128:
            return a->u.decimal->negative == b->u.decimal->negative &&
                   a->u.decimal->exp == b->u.decimal->exp &&
                   strcmp(a->u.decimal->digits, b->u.decimal->digits) == 0;
                   
        case KJSON_TYPE_UUID:
            return memcmp(a->u.uuid->bytes, b->u.uuid->bytes, 16) == 0;
            
        case KJSON_TYPE_INSTANT:
            return a->u.instant->nanoseconds == b->u.instant->nanoseconds &&
                   a->u.instant->tz_offset == b->u.instant->tz_offset;
                   
        case KJSON_TYPE_ARRAY:
            /* Arrays must have same length and elements */
            {
                size_t i;
                
                if (a->u.array.count != b->u.array.count)
                    return false;
                for (i = 0; i < a->u.array.count; i++) {
                    if (!kjson_values_equal(a->u.array.items[i], b->u.array.items[i]))
                        return false;
                }
                return true;
            }
            
        case KJSON_TYPE_OBJECT:
            /* Objects must have same keys and values */
            if (a->u.object.count != b->u.object.count)
                return false;
            
            /* Check each key in a exists in b with same value */
            {
                kjson_member *ma;
                kjson_member *mb;
                bool found;
                
                ma = a->u.object.first;
                while (ma) {
                    mb = b->u.object.first;
                    found = false;
                
                while (mb) {
                    if (strcmp(ma->key, mb->key) == 0) {
                        if (!kjson_values_equal(ma->value, mb->value))
                            return false;
                        found = true;
                        break;
                    }
                    mb = mb->next;
                }
                
                if (!found)
                    return false;
                    
                ma = ma->next;
                }
                return true;
            }
            
        default:
            return false;
    }
}

/*
 * Check if container contains all elements of contained
 */
static bool
kjson_contains_internal(kjson_value *container, kjson_value *contained)
{
    if (!contained)
        return true;
    if (!container)
        return false;
        
    /* If types don't match, check special cases */
    if (container->type != contained->type) {
        /* Only objects can contain non-objects in the sense of key/value containment */
        return false;
    }
    
    switch (contained->type) {
        case KJSON_TYPE_ARRAY:
            /* Container array must contain all elements of contained array */
            {
                size_t i;
                size_t j;
                bool found;
                
                for (i = 0; i < contained->u.array.count; i++) {
                    found = false;
                    for (j = 0; j < container->u.array.count; j++) {
                        if (kjson_values_equal(container->u.array.items[j], 
                                             contained->u.array.items[i])) {
                            found = true;
                            break;
                        }
                    }
                if (!found)
                    return false;
                }
                return true;
            }
            
        case KJSON_TYPE_OBJECT:
            /* Container object must have all key/value pairs of contained */
            {
                kjson_member *mc = contained->u.object.first;
                while (mc) {
                    kjson_member *ma = container->u.object.first;
                    bool found = false;
                    
                    while (ma) {
                        if (strcmp(ma->key, mc->key) == 0) {
                            /* For objects, recursively check containment */
                            if (mc->value->type == KJSON_TYPE_OBJECT ||
                                mc->value->type == KJSON_TYPE_ARRAY) {
                                if (!kjson_contains_internal(ma->value, mc->value))
                                    return false;
                            } else {
                                if (!kjson_values_equal(ma->value, mc->value))
                                    return false;
                            }
                            found = true;
                            break;
                        }
                        ma = ma->next;
                    }
                    
                    if (!found)
                        return false;
                        
                    mc = mc->next;
                }
                return true;
            }
            
        default:
            /* For scalar types, must be equal */
            return kjson_values_equal(container, contained);
    }
}

/*
 * kjson_contains - Check if left contains right
 */
PG_FUNCTION_INFO_V1(kjson_contains);
Datum
kjson_contains(PG_FUNCTION_ARGS)
{
    PGKJson *container_pg = (PGKJson *) PG_DETOAST_DATUM(PG_GETARG_DATUM(0));
    PGKJson *contained_pg = (PGKJson *) PG_DETOAST_DATUM(PG_GETARG_DATUM(1));
    kjson_value *container;
    kjson_value *contained;
    bool result;
    MemoryContext oldcontext;
    MemoryContext tmpcontext;
    
    /* Create temporary context */
    tmpcontext = AllocSetContextCreate(CurrentMemoryContext,
                                       "kjson_contains",
                                       ALLOCSET_DEFAULT_SIZES);
    oldcontext = MemoryContextSwitchTo(tmpcontext);
    
    /* Decode both values */
    container = pg_kjson_decode_binary(PGKJSON_DATA(container_pg), 
                                      PGKJSON_DATA_SIZE(container_pg), 
                                      tmpcontext);
    contained = pg_kjson_decode_binary(PGKJSON_DATA(contained_pg), 
                                      PGKJSON_DATA_SIZE(contained_pg), 
                                      tmpcontext);
    
    /* Check containment */
    result = kjson_contains_internal(container, contained);
    
    /* Clean up */
    MemoryContextSwitchTo(oldcontext);
    MemoryContextDelete(tmpcontext);
    
    PG_RETURN_BOOL(result);
}

/*
 * kjson_contained - Check if left is contained by right
 */
PG_FUNCTION_INFO_V1(kjson_contained);
Datum
kjson_contained(PG_FUNCTION_ARGS)
{
    PGKJson *contained_pg = (PGKJson *) PG_DETOAST_DATUM(PG_GETARG_DATUM(0));
    PGKJson *container_pg = (PGKJson *) PG_DETOAST_DATUM(PG_GETARG_DATUM(1));
    kjson_value *container;
    kjson_value *contained;
    bool result;
    MemoryContext oldcontext;
    MemoryContext tmpcontext;
    
    /* Create temporary context */
    tmpcontext = AllocSetContextCreate(CurrentMemoryContext,
                                       "kjson_contained",
                                       ALLOCSET_DEFAULT_SIZES);
    oldcontext = MemoryContextSwitchTo(tmpcontext);
    
    /* Decode both values */
    container = pg_kjson_decode_binary(PGKJSON_DATA(container_pg), 
                                      PGKJSON_DATA_SIZE(container_pg), 
                                      tmpcontext);
    contained = pg_kjson_decode_binary(PGKJSON_DATA(contained_pg), 
                                      PGKJSON_DATA_SIZE(contained_pg), 
                                      tmpcontext);
    
    /* Check containment (reversed args) */
    result = kjson_contains_internal(container, contained);
    
    /* Clean up */
    MemoryContextSwitchTo(oldcontext);
    MemoryContextDelete(tmpcontext);
    
    PG_RETURN_BOOL(result);
}

/*
 * Helper function to recursively strip null values
 */
static kjson_value *
strip_nulls_internal(kjson_value *value, MemoryContext memctx)
{
    kjson_value *result;
    
    if (!value)
        return NULL;
        
    switch (value->type) {
        case KJSON_TYPE_OBJECT:
            {
                kjson_member *member;
                kjson_member *last = NULL;
                
                result = palloc(sizeof(kjson_value));
                result->type = KJSON_TYPE_OBJECT;
                result->u.object.first = NULL;
                result->u.object.count = 0;
                
                member = value->u.object.first;
                
                while (member) {
                    /* Skip null values */
                    if (member->value && member->value->type != KJSON_TYPE_NULL) {
                        kjson_member *new_member = palloc(sizeof(kjson_member));
                        new_member->key = pstrdup(member->key);
                        new_member->value = strip_nulls_internal(member->value, memctx);
                        new_member->next = NULL;
                        
                        if (last) {
                            last->next = new_member;
                        } else {
                            result->u.object.first = new_member;
                        }
                        last = new_member;
                        result->u.object.count++;
                    }
                    member = member->next;
                }
            }
            break;
            
        case KJSON_TYPE_ARRAY:
            {
                result = palloc(sizeof(kjson_value));
                result->type = KJSON_TYPE_ARRAY;
                result->u.array.capacity = value->u.array.count;
                result->u.array.count = 0;
                result->u.array.items = palloc(sizeof(kjson_value *) * result->u.array.capacity);
                
                for (size_t i = 0; i < value->u.array.count; i++) {
                    if (value->u.array.items[i] && 
                        value->u.array.items[i]->type != KJSON_TYPE_NULL) {
                        result->u.array.items[result->u.array.count++] = 
                            strip_nulls_internal(value->u.array.items[i], memctx);
                    }
                }
            }
            break;
            
        default:
            /* For non-container types, just copy the value */
            result = value;
            break;
    }
    
    return result;
}

/*
 * kjson_strip_nulls - Remove all null values from kjson
 */
PG_FUNCTION_INFO_V1(kjson_strip_nulls);
Datum
kjson_strip_nulls(PG_FUNCTION_ARGS)
{
    PGKJson *pgkj = (PGKJson *) PG_DETOAST_DATUM(PG_GETARG_DATUM(0));
    kjson_value *value;
    kjson_value *result;
    MemoryContext oldcontext;
    MemoryContext tmpcontext;
    
    /* Create temporary context */
    tmpcontext = AllocSetContextCreate(CurrentMemoryContext,
                                       "kjson_strip_nulls",
                                       ALLOCSET_DEFAULT_SIZES);
    oldcontext = MemoryContextSwitchTo(tmpcontext);
    
    /* Decode from binary */
    value = pg_kjson_decode_binary(PGKJSON_DATA(pgkj), 
                                   PGKJSON_DATA_SIZE(pgkj), 
                                   tmpcontext);
    
    /* Strip nulls recursively */
    result = strip_nulls_internal(value, tmpcontext);
    
    /* Convert to binary */
    {
        StringInfo binary = pg_kjson_encode_binary(result);
    
        /* Create result */
        PGKJson *ret = (PGKJson *) palloc(VARHDRSZ + binary->len);
        SET_VARSIZE(ret, VARHDRSZ + binary->len);
        memcpy(VARDATA(ret), binary->data, binary->len);
        
        /* Clean up */
        MemoryContextSwitchTo(oldcontext);
        MemoryContextDelete(tmpcontext);
        
        PG_RETURN_POINTER(ret);
    }
}