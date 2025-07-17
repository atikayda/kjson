/*
 * kjson.h - PostgreSQL kJSON type definitions
 *
 * This header defines the internal structure of the kjson type
 * and common functions used across the implementation.
 * 
 * This builds on top of the core C kJSON library.
 */

#ifndef PG_KJSON_H
#define PG_KJSON_H

#include "postgres.h"
#include "fmgr.h"
#include "utils/builtins.h"
#include "utils/json.h"
#include "utils/jsonb.h"
#include "utils/uuid.h"
#include "utils/numeric.h"
#include "utils/timestamp.h"

/* Include the core C library */
#include "../../c/kjson.h"

/* 
 * PostgreSQL kjson type structure
 * 
 * We store the kJSONB binary format directly in the varlena.
 * The data is the raw kJSONB binary format from kjson_encode_binary().
 */
typedef struct
{
    int32       vl_len_;        /* varlena header */
    uint8       data[FLEXIBLE_ARRAY_MEMBER];  /* kJSONB binary data */
} PGKJson;

/* Macros for accessing kjson data */
#define PGKJSON_DATA(x)       ((x)->data)
#define PGKJSON_DATA_SIZE(x)  (VARSIZE(x) - VARHDRSZ)

/* Function declarations */

/* I/O functions */
Datum kjson_in(PG_FUNCTION_ARGS);
Datum kjson_out(PG_FUNCTION_ARGS);
Datum kjson_recv(PG_FUNCTION_ARGS);
Datum kjson_send(PG_FUNCTION_ARGS);

/* Cast functions */
Datum kjson_to_json(PG_FUNCTION_ARGS);
Datum kjson_to_jsonb(PG_FUNCTION_ARGS);
Datum json_to_kjson(PG_FUNCTION_ARGS);
Datum jsonb_to_kjson(PG_FUNCTION_ARGS);
Datum text_to_kjson(PG_FUNCTION_ARGS);

/* Operator functions */
Datum kjson_eq(PG_FUNCTION_ARGS);
Datum kjson_ne(PG_FUNCTION_ARGS);
Datum kjson_object_field(PG_FUNCTION_ARGS);
Datum kjson_object_field_text(PG_FUNCTION_ARGS);
Datum kjson_array_element(PG_FUNCTION_ARGS);
Datum kjson_array_element_text(PG_FUNCTION_ARGS);

/* Utility functions */
Datum kjson_pretty(PG_FUNCTION_ARGS);
Datum kjson_compact(PG_FUNCTION_ARGS);
Datum kjson_typeof(PG_FUNCTION_ARGS);
Datum kjson_extract_uuid(PG_FUNCTION_ARGS);
Datum kjson_extract_numeric(PG_FUNCTION_ARGS);
Datum kjson_extract_timestamp(PG_FUNCTION_ARGS);

/* Aggregate functions */
Datum kjson_agg_transfn(PG_FUNCTION_ARGS);
Datum kjson_agg_finalfn(PG_FUNCTION_ARGS);
Datum kjson_object_agg_transfn(PG_FUNCTION_ARGS);
Datum kjson_object_agg_finalfn(PG_FUNCTION_ARGS);

/* Set-returning functions */
Datum kjson_array_elements(PG_FUNCTION_ARGS);
Datum kjson_array_elements_text(PG_FUNCTION_ARGS);

/* Builder functions */
Datum kjson_build_object(PG_FUNCTION_ARGS);
Datum kjson_build_array(PG_FUNCTION_ARGS);
Datum row_to_kjson(PG_FUNCTION_ARGS);

/* Path and existence operators */
Datum kjson_extract_path(PG_FUNCTION_ARGS);
Datum kjson_extract_path_text(PG_FUNCTION_ARGS);
Datum kjson_exists(PG_FUNCTION_ARGS);
Datum kjson_exists_any(PG_FUNCTION_ARGS);
Datum kjson_exists_all(PG_FUNCTION_ARGS);
Datum kjson_contains(PG_FUNCTION_ARGS);
Datum kjson_contained(PG_FUNCTION_ARGS);

/* Additional utility functions */
Datum kjson_array_length(PG_FUNCTION_ARGS);
Datum kjson_object_keys(PG_FUNCTION_ARGS);
Datum kjson_strip_nulls(PG_FUNCTION_ARGS);

/* Unified temporal functions */
Datum kjson_build_instant(PG_FUNCTION_ARGS);
Datum kjson_build_duration(PG_FUNCTION_ARGS);
Datum kjson_extract_kinstant(PG_FUNCTION_ARGS);
Datum kjson_extract_kduration(PG_FUNCTION_ARGS);

/* Internal helper functions that bridge kJSON C library with PostgreSQL */
PGKJson *kjson_value_to_pg(kjson_value *value);
kjson_value *pg_to_kjson_value(PGKJson *pgkj);
PGKJson *jsonb_to_pgkjson(Jsonb *jb);
Jsonb *pgkjson_to_jsonb(PGKJson *pgkj);
text *pgkjson_to_text(PGKJson *pgkj, bool pretty, int indent);

/* PostgreSQL-specific allocator context */
typedef struct {
    MemoryContext memcxt;
} PGKJsonAllocator;

#endif /* PG_KJSON_H */