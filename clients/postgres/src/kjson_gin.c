/*
 * kjson_gin.c - GIN index support for kjson type
 *
 * This file implements GIN (Generalized Inverted Index) support for kjson,
 * enabling fast lookups for existence (?), containment (@>), and other operations.
 */

#include "postgres.h"
#include "fmgr.h"
#include "access/gin.h"
#include "access/stratnum.h"
#include "utils/builtins.h"
#include "utils/array.h"
#include "utils/lsyscache.h"
#include "lib/stringinfo.h"
#include "utils/memutils.h"
#include "catalog/pg_type.h"
#include "utils/varlena.h"

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

/* GIN strategy numbers for kjson operators */
#define KJsonContainsStrategyNumber      7
#define KJsonExistsStrategyNumber        9
#define KJsonExistsAnyStrategyNumber     10
#define KJsonExistsAllStrategyNumber     11

/* Forward declarations */
static void extract_keys_recursive(kjson_value *value, const char *path_prefix, 
                                  Datum **entries, int *nentries, int *allocated);
/* static bool match_object_keys(kjson_value *obj, char **query_keys, int nkeys, bool match_all); // unused */
static bool contains_value(kjson_value *haystack, kjson_value *needle);

/*
 * kjson_gin_extract_value - Extract indexable entries from a kjson value
 *
 * This function extracts all keys and paths from a kjson value that should
 * be indexed for fast lookups.
 */
PG_FUNCTION_INFO_V1(kjson_gin_extract_value);
Datum
kjson_gin_extract_value(PG_FUNCTION_ARGS)
{
    PGKJson *kjson_val;
    int32 *nentries = (int32 *) PG_GETARG_POINTER(1);
    MemoryContext oldcontext;
    MemoryContext extractcontext;
    kjson_value *value;
    Datum *entries = NULL;
    int allocated = 0;
    text *copied_text;
    
    /* Validate input arguments */
    if (PG_ARGISNULL(0)) {
        elog(DEBUG1, "kjson_gin_extract_value: NULL input");
        *nentries = 0;
        PG_RETURN_POINTER(NULL);
    }
    
    /* Validate memory context */
    if (!CurrentMemoryContext) {
        elog(ERROR, "kjson_gin_extract_value: CurrentMemoryContext is NULL");
    }
    
    elog(DEBUG1, "kjson_gin_extract_value: Starting extraction");
    
    /* Safely detoast the input */
    PG_TRY();
    {
        kjson_val = (PGKJson *) PG_DETOAST_DATUM(PG_GETARG_DATUM(0));
        
        /* Validate detoasted data */
        if (!kjson_val) {
            elog(ERROR, "kjson_gin_extract_value: Failed to detoast kjson data");
        }
        
        /* Check varlena header */
        if (VARSIZE(kjson_val) < VARHDRSZ) {
            elog(ERROR, "kjson_gin_extract_value: Invalid varlena size: %d", VARSIZE(kjson_val));
        }
        
        elog(DEBUG1, "kjson_gin_extract_value: Detoasted successfully, size: %d", VARSIZE(kjson_val));
    }
    PG_CATCH();
    {
        elog(ERROR, "kjson_gin_extract_value: Exception during detoast");
        PG_RE_THROW();
    }
    PG_END_TRY();
    
    /* Create temporary memory context */
    extractcontext = AllocSetContextCreate(CurrentMemoryContext,
                                          "kJSON GIN extract context",
                                          ALLOCSET_DEFAULT_SIZES);
    if (!extractcontext) {
        elog(ERROR, "kjson_gin_extract_value: Failed to create memory context");
    }
    
    oldcontext = MemoryContextSwitchTo(extractcontext);
    elog(DEBUG1, "kjson_gin_extract_value: Switched to extract context");
    
    /* Decode the kjson value */
    PG_TRY();
    {
        value = pg_kjson_decode_binary(PGKJSON_DATA(kjson_val), 
                                      PGKJSON_DATA_SIZE(kjson_val), 
                                      extractcontext);
        
        if (!value) {
            elog(WARNING, "kjson_gin_extract_value: Failed to decode binary data");
            MemoryContextSwitchTo(oldcontext);
            MemoryContextDelete(extractcontext);
            *nentries = 0;
            PG_RETURN_POINTER(NULL);
        }
        
        elog(DEBUG1, "kjson_gin_extract_value: Decoded successfully, type: %d", value->type);
    }
    PG_CATCH();
    {
        elog(ERROR, "kjson_gin_extract_value: Exception during binary decode");
        MemoryContextSwitchTo(oldcontext);
        MemoryContextDelete(extractcontext);
        PG_RE_THROW();
    }
    PG_END_TRY();
    
    /* Initialize entries array */
    *nentries = 0;
    
    /* Extract keys recursively with error handling */
    PG_TRY();
    {
        extract_keys_recursive(value, "", &entries, nentries, &allocated);
        elog(DEBUG1, "kjson_gin_extract_value: Extracted %d keys", *nentries);
    }
    PG_CATCH();
    {
        elog(ERROR, "kjson_gin_extract_value: Exception during key extraction");
        MemoryContextSwitchTo(oldcontext);
        MemoryContextDelete(extractcontext);
        PG_RE_THROW();
    }
    PG_END_TRY();
    
    /* Switch back to original context */
    MemoryContextSwitchTo(oldcontext);
    elog(DEBUG1, "kjson_gin_extract_value: Switched back to original context");
    
    /* Copy entries to original context if we have any */
    if (*nentries > 0) {
        PG_TRY();
        {
            Datum *result_entries = (Datum *) palloc(*nentries * sizeof(Datum));
            for (int i = 0; i < *nentries; i++) {
                /* Entries are already text datums from extract_keys_recursive */
                text *key_text = DatumGetTextP(entries[i]);
                if (!key_text) {
                    elog(WARNING, "kjson_gin_extract_value: NULL key at index %d", i);
                    continue;
                }
                /* Copy the text to the original memory context */
                copied_text = (text *) palloc(VARSIZE(key_text));
                memcpy(copied_text, key_text, VARSIZE(key_text));
                result_entries[i] = PointerGetDatum(copied_text);
            }
            MemoryContextDelete(extractcontext);
            elog(DEBUG1, "kjson_gin_extract_value: Successfully copied %d entries", *nentries);
            PG_RETURN_POINTER(result_entries);
        }
        PG_CATCH();
        {
            elog(ERROR, "kjson_gin_extract_value: Exception during entry copying");
            MemoryContextDelete(extractcontext);
            PG_RE_THROW();
        }
        PG_END_TRY();
    }
    
    MemoryContextDelete(extractcontext);
    elog(DEBUG1, "kjson_gin_extract_value: Returning NULL (no entries)");
    PG_RETURN_POINTER(NULL);
}

/*
 * kjson_gin_extract_query - Extract query keys for index lookup
 *
 * This function extracts the keys to look for from the query condition.
 */
PG_FUNCTION_INFO_V1(kjson_gin_extract_query);
Datum
kjson_gin_extract_query(PG_FUNCTION_ARGS)
{
    PGKJson *query = (PGKJson *) PG_DETOAST_DATUM(PG_GETARG_DATUM(0));
    int32 *nentries = (int32 *) PG_GETARG_POINTER(1);
    StrategyNumber strategy = PG_GETARG_UINT16(2);
    bool **pmatch = (bool **) PG_GETARG_POINTER(3);
    Pointer **extra_data __attribute__((unused)) = (Pointer **) PG_GETARG_POINTER(4);
    MemoryContext oldcontext;
    MemoryContext querycontext;
    kjson_value *query_value;
    Datum *entries = NULL;
    bool *match_flags = NULL;
    int allocated = 0;
    
    /* Create temporary memory context */
    querycontext = AllocSetContextCreate(CurrentMemoryContext,
                                        "kJSON GIN query context",
                                        ALLOCSET_DEFAULT_SIZES);
    oldcontext = MemoryContextSwitchTo(querycontext);
    
    /* Decode the query value */
    query_value = pg_kjson_decode_binary(PGKJSON_DATA(query), 
                                        PGKJSON_DATA_SIZE(query), 
                                        querycontext);
    
    if (!query_value) {
        MemoryContextSwitchTo(oldcontext);
        MemoryContextDelete(querycontext);
        *nentries = 0;
        PG_RETURN_POINTER(NULL);
    }
    
    /* Initialize */
    *nentries = 0;
    
    switch (strategy) {
        case KJsonContainsStrategyNumber:  /* @> */
            /* For containment, extract all keys from the query object */
            extract_keys_recursive(query_value, "", &entries, nentries, &allocated);
            break;
            
        case KJsonExistsStrategyNumber:    /* ? */
            /* For single key existence, the query should be a string */
            if (query_value->type == KJSON_TYPE_STRING && query_value->u.string) {
                *nentries = 1;
                entries = (Datum *) palloc(sizeof(Datum));
                entries[0] = CStringGetDatum(pstrdup(query_value->u.string));
            }
            break;
            
        case KJsonExistsAnyStrategyNumber: /* ?| */
        case KJsonExistsAllStrategyNumber: /* ?& */
            /* For array of keys, extract each key */
            if (query_value->type == KJSON_TYPE_ARRAY) {
                *nentries = query_value->u.array.count;
                if (*nentries > 0) {
                    entries = (Datum *) palloc(*nentries * sizeof(Datum));
                    for (size_t i = 0; i < query_value->u.array.count; i++) {
                        kjson_value *item = query_value->u.array.items[i];
                        if (item && item->type == KJSON_TYPE_STRING && item->u.string) {
                            entries[i] = CStringGetDatum(pstrdup(item->u.string));
                        } else {
                            /* Invalid key type, reduce count */
                            (*nentries)--;
                            i--;
                        }
                    }
                }
            }
            break;
            
        default:
            /* Unknown strategy */
            break;
    }
    
    /* Switch back to original context */
    MemoryContextSwitchTo(oldcontext);
    
    /* Set up match flags for ?& (exists all) strategy */
    if (strategy == KJsonExistsAllStrategyNumber && *nentries > 0) {
        match_flags = (bool *) palloc(*nentries * sizeof(bool));
        for (int i = 0; i < *nentries; i++) {
            match_flags[i] = true;  /* All keys must match */
        }
        *pmatch = match_flags;
    } else {
        *pmatch = NULL;
    }
    
    /* Copy entries to original context if we have any */
    if (*nentries > 0) {
        Datum *result_entries = (Datum *) palloc(*nentries * sizeof(Datum));
        for (int i = 0; i < *nentries; i++) {
            char *key_str = DatumGetCString(entries[i]);
            result_entries[i] = CStringGetDatum(pstrdup(key_str));
        }
        MemoryContextDelete(querycontext);
        PG_RETURN_POINTER(result_entries);
    }
    
    MemoryContextDelete(querycontext);
    PG_RETURN_POINTER(NULL);
}

/*
 * kjson_gin_consistent - Check if index entry matches query
 *
 * This function determines whether the indexed keys satisfy the query.
 */
PG_FUNCTION_INFO_V1(kjson_gin_consistent);
Datum
kjson_gin_consistent(PG_FUNCTION_ARGS)
{
    bool *check = (bool *) PG_GETARG_POINTER(0);
    StrategyNumber strategy __attribute__((unused)) = PG_GETARG_UINT16(1);
    PGKJson *query __attribute__((unused)) = (PGKJson *) PG_DETOAST_DATUM(PG_GETARG_DATUM(2));
    int32 nkeys = PG_GETARG_INT32(3);
    Pointer *extra_data __attribute__((unused)) = (Pointer *) PG_GETARG_POINTER(4);
    bool *recheck = (bool *) PG_GETARG_POINTER(5);
    bool result = false;
    
    /* Always recheck for complex operations */
    *recheck = true;
    
    switch (strategy) {
        case KJsonContainsStrategyNumber:  /* @> */
            /* For containment, we need a more complex check */
            /* For now, if any key matches, we might contain the object */
            for (int i = 0; i < nkeys; i++) {
                if (check[i]) {
                    result = true;
                    break;
                }
            }
            break;
            
        case KJsonExistsStrategyNumber:    /* ? */
            /* For single key existence, check if the key is present */
            result = (nkeys > 0 && check[0]);
            *recheck = false;  /* Simple existence doesn't need recheck */
            break;
            
        case KJsonExistsAnyStrategyNumber: /* ?| */
            /* For "any key exists", if any key matches, return true */
            for (int i = 0; i < nkeys; i++) {
                if (check[i]) {
                    result = true;
                    break;
                }
            }
            *recheck = false;  /* Simple existence doesn't need recheck */
            break;
            
        case KJsonExistsAllStrategyNumber: /* ?& */
            /* For "all keys exist", all keys must match */
            result = true;
            for (int i = 0; i < nkeys; i++) {
                if (!check[i]) {
                    result = false;
                    break;
                }
            }
            *recheck = false;  /* Simple existence doesn't need recheck */
            break;
            
        default:
            result = false;
            break;
    }
    
    PG_RETURN_BOOL(result);
}

/*
 * kjson_gin_compare_partial - Compare partial keys for range queries
 *
 * This function is used for partial key matching in GIN indexes.
 */
PG_FUNCTION_INFO_V1(kjson_gin_compare_partial);
Datum
kjson_gin_compare_partial(PG_FUNCTION_ARGS)
{
    text *partial_key = PG_GETARG_TEXT_PP(0);
    text *key = PG_GETARG_TEXT_PP(1);
    StrategyNumber strategy __attribute__((unused)) = PG_GETARG_UINT16(2);
    Pointer extra_data __attribute__((unused)) = PG_GETARG_POINTER(3);
    
    char *partial_str = text_to_cstring(partial_key);
    char *key_str = text_to_cstring(key);
    int result;
    
    /* Simple string comparison for now */
    result = strcmp(partial_str, key_str);
    
    pfree(partial_str);
    pfree(key_str);
    
    PG_RETURN_INT32(result);
}

/*
 * Helper function to recursively extract keys from kjson values
 */
static void
extract_keys_recursive(kjson_value *value, const char *path_prefix, 
                       Datum **entries, int *nentries, int *allocated)
{
    kjson_member *member;
    char *full_key;
    size_t limit;
    int member_count;
    text *key_text;
    
    if (!value) {
        elog(DEBUG2, "extract_keys_recursive: NULL value");
        return;
    }
    
    elog(DEBUG2, "extract_keys_recursive: type %d, path_prefix '%s'", value->type, path_prefix ? path_prefix : "NULL");
    
    /* Validate current state */
    if (*nentries < 0 || *allocated < 0) {
        elog(ERROR, "extract_keys_recursive: Invalid state - nentries: %d, allocated: %d", *nentries, *allocated);
    }
    
    switch (value->type) {
        case KJSON_TYPE_OBJECT:
            {
                elog(DEBUG2, "extract_keys_recursive: Processing object with %zu members", value->u.object.count);
                member = value->u.object.first;
                member_count = 0;
                
                while (member) {
                    member_count++;
                    if (member_count > 1000) {  /* Prevent infinite loops */
                        elog(ERROR, "extract_keys_recursive: Too many object members (possible loop)");
                        break;
                    }
                    
                    if (member->key) {
                        elog(DEBUG2, "extract_keys_recursive: Processing key '%s'", member->key);
                        
                        /* Add the key itself */
                        if (*nentries >= *allocated) {
                            int new_allocated = (*allocated == 0) ? 8 : *allocated * 2;
                            if (new_allocated > 10000) {  /* Reasonable limit */
                                elog(ERROR, "extract_keys_recursive: Too many keys extracted (limit: 10000)");
                                break;
                            }
                            
                            elog(DEBUG2, "extract_keys_recursive: Reallocating from %d to %d", *allocated, new_allocated);
                            
                            if (*entries == NULL) {
                                *entries = (Datum *) palloc(new_allocated * sizeof(Datum));
                            } else {
                                *entries = (Datum *) repalloc(*entries, new_allocated * sizeof(Datum));
                            }
                            *allocated = new_allocated;
                        }
                        
                        /* Create full path key */
                        if (path_prefix && strlen(path_prefix) > 0) {
                            full_key = psprintf("%s.%s", path_prefix, member->key);
                        } else {
                            full_key = pstrdup(member->key);
                        }
                        
                        if (!full_key) {
                            elog(ERROR, "extract_keys_recursive: Failed to create full_key");
                            break;
                        }
                        
                        /* Create text datum instead of cstring datum */
                        key_text = cstring_to_text(full_key);
                        (*entries)[*nentries] = PointerGetDatum(key_text);
                        (*nentries)++;
                        
                        elog(DEBUG2, "extract_keys_recursive: Added key '%s' (total: %d)", full_key, *nentries);
                        
                        /* Recursively extract from nested objects/arrays - but limit depth */
                        if (member->value && strlen(full_key) < 200) {  /* Prevent too deep nesting */
                            extract_keys_recursive(member->value, full_key, entries, nentries, allocated);
                        }
                    }
                    member = member->next;
                }
                elog(DEBUG2, "extract_keys_recursive: Finished processing object (%d members)", member_count);
            }
            break;
            
        case KJSON_TYPE_ARRAY:
            {
                elog(DEBUG2, "extract_keys_recursive: Processing array with %zu elements", value->u.array.count);
                
                if (value->u.array.count > 1000) {  /* Reasonable limit for arrays */
                    elog(WARNING, "extract_keys_recursive: Large array (%zu elements), limiting to 100", value->u.array.count);
                }
                
                limit = (value->u.array.count > 100) ? 100 : value->u.array.count;
                for (size_t i = 0; i < limit; i++) {
                    if (value->u.array.items[i]) {
                        char *array_path = psprintf("%s[%zu]", path_prefix ? path_prefix : "", i);
                        if (!array_path) {
                            elog(ERROR, "extract_keys_recursive: Failed to create array_path");
                            break;
                        }
                        
                        if (strlen(array_path) < 200) {  /* Prevent too deep nesting */
                            extract_keys_recursive(value->u.array.items[i], array_path, 
                                                 entries, nentries, allocated);
                        }
                        pfree(array_path);
                    }
                }
                elog(DEBUG2, "extract_keys_recursive: Finished processing array");
            }
            break;
            
        default:
            elog(DEBUG2, "extract_keys_recursive: Scalar value type %d, no keys to extract", value->type);
            break;
    }
    
    elog(DEBUG2, "extract_keys_recursive: Exiting with %d total entries", *nentries);
}

/*
 * Helper function to check if an object contains specific keys
 */
/* static bool
match_object_keys(kjson_value *obj, char **query_keys, int nkeys, bool match_all) // unused
{
    int matched;
    
    if (!obj || obj->type != KJSON_TYPE_OBJECT) {
        return false;
    }
    
    matched = 0;
    
    for (int i = 0; i < nkeys; i++) {
        bool found = false;
        kjson_member *member = obj->u.object.first;
        
        while (member) {
            if (member->key && strcmp(member->key, query_keys[i]) == 0) {
                found = true;
                break;
            }
            member = member->next;
        }
        
        if (found) {
            matched++;
            if (!match_all) {
                return true;  // For "any" match, one is enough
            }
        } else if (match_all) {
            return false;  // For "all" match, missing one means failure
        }
    }
    
    return match_all ? (matched == nkeys) : (matched > 0);
} */

/*
 * Helper function to check if one kjson value contains another
 */
static bool __attribute__((used))
contains_value(kjson_value *haystack, kjson_value *needle)
{
    if (!haystack || !needle) {
        return false;
    }
    
    if (haystack->type != needle->type) {
        return false;
    }
    
    switch (needle->type) {
        case KJSON_TYPE_OBJECT:
            {
                /* Check if haystack object contains all key-value pairs from needle */
                kjson_member *needle_member = needle->u.object.first;
                while (needle_member) {
                    bool found = false;
                    kjson_member *hay_member = haystack->u.object.first;
                    
                    while (hay_member) {
                        if (hay_member->key && needle_member->key &&
                            strcmp(hay_member->key, needle_member->key) == 0) {
                            if (contains_value(hay_member->value, needle_member->value)) {
                                found = true;
                                break;
                            }
                        }
                        hay_member = hay_member->next;
                    }
                    
                    if (!found) {
                        return false;
                    }
                    needle_member = needle_member->next;
                }
                return true;
            }
            
        case KJSON_TYPE_ARRAY:
            {
                /* Check if haystack array contains all elements from needle */
                for (size_t i = 0; i < needle->u.array.count; i++) {
                    bool found = false;
                    for (size_t j = 0; j < haystack->u.array.count; j++) {
                        if (contains_value(haystack->u.array.items[j], 
                                         needle->u.array.items[i])) {
                            found = true;
                            break;
                        }
                    }
                    if (!found) {
                        return false;
                    }
                }
                return true;
            }
            
        default:
            /* For scalar values, use existing comparison logic */
            /* This is a simplified version - could use kjson_cmp for full comparison */
            return true;  /* Simplified for now */
    }
}