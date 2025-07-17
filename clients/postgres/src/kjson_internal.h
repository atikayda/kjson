/*
 * kjson_internal.h - PostgreSQL-native kJSON implementation
 *
 * This header defines the native PostgreSQL implementation of kJSON
 * that doesn't depend on the external C library.
 */

#ifndef PG_KJSON_INTERNAL_H
#define PG_KJSON_INTERNAL_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>

/* Memory context for parser */
struct MemoryContextData;
typedef struct MemoryContextData *MemoryContext;

/* StringInfo structure forward declaration */
struct StringInfoData;
typedef struct StringInfoData *StringInfo;

/* Type enumeration */
typedef enum {
    KJSON_TYPE_NULL,
    KJSON_TYPE_BOOLEAN,
    KJSON_TYPE_NUMBER,
    KJSON_TYPE_STRING,
    KJSON_TYPE_ARRAY,
    KJSON_TYPE_OBJECT,
    KJSON_TYPE_BIGINT,
    KJSON_TYPE_DECIMAL128,
    KJSON_TYPE_UUID,
    KJSON_TYPE_INSTANT,
    KJSON_TYPE_DURATION,
    KJSON_TYPE_BINARY
} kjson_type;

/* Type definitions */
typedef struct kjson_value kjson_value;
typedef struct kjson_member kjson_member;

/* BigInt type */
typedef struct {
    bool negative;
    char *digits;
} kjson_bigint;

/* Decimal128 type */
typedef struct {
    bool negative;
    int32_t exp;
    char *digits;
} kjson_decimal128;

/* UUID type */
typedef struct {
    unsigned char bytes[16];
} kjson_uuid;

/* kInstant structure - 16 bytes total */
typedef struct {
    int64_t nanoseconds;    /* Nanoseconds since epoch (8 bytes) */
    int16_t tz_offset;      /* Timezone offset in minutes (2 bytes) */
    int16_t reserved;       /* Reserved for future use (2 bytes) */
    int32_t reserved2;      /* Reserved for future use (4 bytes) */
} kInstant;

/* kDuration structure - 32 bytes total */
typedef struct {
    int32_t years;          /* Years component (4 bytes) */
    int32_t months;         /* Months component (4 bytes) */
    int32_t days;           /* Days component (4 bytes) */
    int32_t hours;          /* Hours component (4 bytes) */
    int32_t minutes;        /* Minutes component (4 bytes) */
    int64_t nanoseconds;    /* Seconds + fractional seconds as nanoseconds (8 bytes) */
    bool negative;          /* True for negative durations (1 byte) */
    char reserved[7];       /* Reserved for future use (7 bytes) */
} kDuration;

/* Use kInstant and kDuration directly for kjson temporal types */
typedef kInstant kjson_instant;
typedef kDuration kjson_duration;

/* Object member */
struct kjson_member {
    char *key;
    kjson_value *value;
    kjson_member *next;
};

/* Value structure */
struct kjson_value {
    kjson_type type;
    union {
        bool boolean;
        double number;
        char *string;
        kjson_bigint *bigint;
        kjson_decimal128 *decimal;
        kjson_uuid *uuid;
        kjson_instant *instant;
        kjson_duration *duration;
        struct {
            kjson_value **items;
            size_t count;
            size_t capacity;
        } array;
        struct {
            kjson_member *first;
            size_t count;
        } object;
        uint8_t *binary;
    } u;
};

/* Write options */
typedef struct {
    int indent;             /* Indent size (spaces) */
    bool quote_keys;        /* Always quote object keys */
    bool bigint_suffix;     /* Add 'n' suffix to BigInt */
    bool decimal_suffix;    /* Add 'm' suffix to Decimal128 */
    bool escape_unicode;    /* Escape non-ASCII as \uXXXX */
    bool use_single_quotes; /* Use single quotes for strings (kJSON style) */
} kjson_write_options;

/* Parser functions */
kjson_value *pg_kjson_parse(const char *input, size_t length, MemoryContext memctx);

/* Stringifier functions */
char *pg_kjson_stringify(const kjson_value *value, const kjson_write_options *opts);

/* Binary format functions */
StringInfo pg_kjson_encode_binary(const kjson_value *value);
kjson_value *pg_kjson_decode_binary(const uint8_t *data, size_t size, MemoryContext memctx);

/* Temporal format functions */
char *format_iso_duration(kjson_duration *duration);

/* Builder helper functions */
kjson_value *sql_value_to_kjson(Datum value, Oid typoid, bool isnull, MemoryContext memctx);

#endif /* PG_KJSON_INTERNAL_H */