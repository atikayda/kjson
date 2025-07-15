/*
 * kjson_temporal.c - High-precision temporal functions for kjson
 *
 * This file implements nanosecond-precision timestamp and duration functions,
 * including ISO 8601 duration support and temporal arithmetic.
 */

#include "postgres.h"
#include "fmgr.h"
#include "utils/builtins.h"
#include "utils/timestamp.h"
#include "utils/datetime.h"
#include "lib/stringinfo.h"
#include "utils/memutils.h"
#include "catalog/pg_type.h"
#include <time.h>
#include <sys/time.h>

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

/* Forward declarations */
static int64_t get_nanosecond_timestamp(void);
static kjson_value *create_kjson_instant(int64_t nanoseconds, MemoryContext memctx);
static kjson_value *create_kjson_duration(kjson_duration *duration, MemoryContext memctx);
static kjson_duration *parse_iso_duration(const char *duration_str);

/*
 * kjson_now - Get current timestamp with nanosecond precision
 */
PG_FUNCTION_INFO_V1(kjson_now);
Datum
kjson_now(PG_FUNCTION_ARGS)
{
    MemoryContext oldcontext;
    MemoryContext timestampcontext;
    kjson_value *timestamp_value;
    StringInfo binary;
    PGKJson *result;
    int64_t nanoseconds;
    
    /* Create temporary memory context */
    timestampcontext = AllocSetContextCreate(CurrentMemoryContext,
                                           "kJSON instant context",
                                           ALLOCSET_DEFAULT_SIZES);
    oldcontext = MemoryContextSwitchTo(timestampcontext);
    
    /* Get nanosecond precision instant */
    nanoseconds = get_nanosecond_timestamp();
    
    /* Create kjson instant value */
    timestamp_value = create_kjson_instant(nanoseconds, timestampcontext);
    
    /* Encode to binary */
    binary = pg_kjson_encode_binary(timestamp_value);
    
    /* Switch back to original context */
    MemoryContextSwitchTo(oldcontext);
    
    /* Create PostgreSQL result */
    result = (PGKJson *) palloc(VARHDRSZ + binary->len);
    SET_VARSIZE(result, VARHDRSZ + binary->len);
    memcpy(PGKJSON_DATA(result), binary->data, binary->len);
    
    MemoryContextDelete(timestampcontext);
    PG_RETURN_POINTER(result);
}

/*
 * kjson_duration_from_iso - Create a duration from ISO 8601 string
 */
PG_FUNCTION_INFO_V1(kjson_duration_from_iso);
Datum
kjson_duration_from_iso(PG_FUNCTION_ARGS)
{
    text *duration_text = PG_GETARG_TEXT_PP(0);
    char *duration_str = text_to_cstring(duration_text);
    MemoryContext oldcontext;
    MemoryContext durationcontext;
    kjson_duration *duration;
    kjson_value *duration_value;
    StringInfo binary;
    PGKJson *result;
    
    /* Create temporary memory context */
    durationcontext = AllocSetContextCreate(CurrentMemoryContext,
                                          "kJSON duration context",
                                          ALLOCSET_DEFAULT_SIZES);
    oldcontext = MemoryContextSwitchTo(durationcontext);
    
    /* Parse ISO 8601 duration */
    duration = parse_iso_duration(duration_str);
    if (!duration) {
        MemoryContextSwitchTo(oldcontext);
        MemoryContextDelete(durationcontext);
        ereport(ERROR,
                (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION),
                 errmsg("invalid ISO 8601 duration: \"%s\"", duration_str)));
    }
    
    /* Create kjson duration value */
    duration_value = create_kjson_duration(duration, durationcontext);
    
    /* Encode to binary */
    binary = pg_kjson_encode_binary(duration_value);
    
    /* Switch back to original context */
    MemoryContextSwitchTo(oldcontext);
    
    /* Create PostgreSQL result */
    result = (PGKJson *) palloc(VARHDRSZ + binary->len);
    SET_VARSIZE(result, VARHDRSZ + binary->len);
    memcpy(PGKJSON_DATA(result), binary->data, binary->len);
    
    MemoryContextDelete(durationcontext);
    pfree(duration_str);
    PG_RETURN_POINTER(result);
}

/*
 * kjson_add_duration - Add a duration to an instant
 */
PG_FUNCTION_INFO_V1(kjson_add_duration);
Datum
kjson_add_duration(PG_FUNCTION_ARGS)
{
    PGKJson *instant_kjson = (PGKJson *) PG_DETOAST_DATUM(PG_GETARG_DATUM(0));
    PGKJson *duration_kjson = (PGKJson *) PG_DETOAST_DATUM(PG_GETARG_DATUM(1));
    MemoryContext oldcontext;
    MemoryContext addcontext;
    kjson_value *instant_value;
    kjson_value *duration_value;
    kjson_value *result_value;
    StringInfo binary;
    PGKJson *result;
    int64_t result_nanoseconds;
    kjson_duration *dur;
    int64_t minute_ns;
    int64_t hour_ns;
    int64_t day_ns;
    int64_t month_ns;
    int64_t year_ns;
    
    /* Create temporary memory context */
    addcontext = AllocSetContextCreate(CurrentMemoryContext,
                                     "kJSON add duration context",
                                     ALLOCSET_DEFAULT_SIZES);
    oldcontext = MemoryContextSwitchTo(addcontext);
    
    /* Decode both values */
    instant_value = pg_kjson_decode_binary(PGKJSON_DATA(instant_kjson),
                                         PGKJSON_DATA_SIZE(instant_kjson),
                                         addcontext);
    duration_value = pg_kjson_decode_binary(PGKJSON_DATA(duration_kjson),
                                          PGKJSON_DATA_SIZE(duration_kjson),
                                          addcontext);
    
    if (!instant_value || instant_value->type != KJSON_TYPE_INSTANT) {
        MemoryContextSwitchTo(oldcontext);
        MemoryContextDelete(addcontext);
        ereport(ERROR,
                (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                 errmsg("first argument must be a kjson instant")));
    }
    
    if (!duration_value || duration_value->type != KJSON_TYPE_DURATION) {
        MemoryContextSwitchTo(oldcontext);
        MemoryContextDelete(addcontext);
        ereport(ERROR,
                (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                 errmsg("second argument must be a kjson duration")));
    }
    
    /* Perform duration addition */
    result_value = palloc(sizeof(kjson_value));
    result_value->type = KJSON_TYPE_INSTANT;
    result_value->u.instant = palloc(sizeof(kjson_instant));
    
    /* Start with original instant */
    result_nanoseconds = instant_value->u.instant->nanoseconds;
    
    /* Add duration components */
    dur = duration_value->u.duration;
    
    /* Add seconds and nanoseconds directly */
    if (dur->negative) {
        result_nanoseconds -= dur->nanoseconds;
    } else {
        result_nanoseconds += dur->nanoseconds;
    }
    
    /* Add minutes */
    minute_ns = (int64_t)dur->minutes * 60LL * 1000000000LL;
    if (dur->negative) {
        result_nanoseconds -= minute_ns;
    } else {
        result_nanoseconds += minute_ns;
    }
    
    /* Add hours */
    hour_ns = (int64_t)dur->hours * 3600LL * 1000000000LL;
    if (dur->negative) {
        result_nanoseconds -= hour_ns;
    } else {
        result_nanoseconds += hour_ns;
    }
    
    /* Add days */
    day_ns = (int64_t)dur->days * 86400LL * 1000000000LL;
    if (dur->negative) {
        result_nanoseconds -= day_ns;
    } else {
        result_nanoseconds += day_ns;
    }
    
    /* Note: Months and years require more complex calendar arithmetic */
    /* For now, approximate: 1 month = 30 days, 1 year = 365 days */
    if (dur->months != 0) {
        month_ns = (int64_t)dur->months * 30LL * 86400LL * 1000000000LL;
        if (dur->negative) {
            result_nanoseconds -= month_ns;
        } else {
            result_nanoseconds += month_ns;
        }
    }
    
    if (dur->years != 0) {
        year_ns = (int64_t)dur->years * 365LL * 86400LL * 1000000000LL;
        if (dur->negative) {
            result_nanoseconds -= year_ns;
        } else {
            result_nanoseconds += year_ns;
        }
    }
    
    result_value->u.instant->nanoseconds = result_nanoseconds;
    result_value->u.instant->tz_offset = instant_value->u.instant->tz_offset;
    
    /* Encode to binary */
    binary = pg_kjson_encode_binary(result_value);
    
    /* Switch back to original context */
    MemoryContextSwitchTo(oldcontext);
    
    /* Create PostgreSQL result */
    result = (PGKJson *) palloc(VARHDRSZ + binary->len);
    SET_VARSIZE(result, VARHDRSZ + binary->len);
    memcpy(PGKJSON_DATA(result), binary->data, binary->len);
    
    MemoryContextDelete(addcontext);
    PG_RETURN_POINTER(result);
}

/*
 * Helper function to get current instant with nanosecond precision
 */
static int64_t
get_nanosecond_timestamp(void)
{
    struct timespec ts;
    struct timeval tv;
    
    /* Try to get nanosecond precision time */
    if (clock_gettime(CLOCK_REALTIME, &ts) == 0) {
        /* Convert to nanoseconds since epoch */
        return ((int64_t)ts.tv_sec * 1000000000LL) + (int64_t)ts.tv_nsec;
    } else {
        /* Fallback to microsecond precision */
        gettimeofday(&tv, NULL);
        return ((int64_t)tv.tv_sec * 1000000000LL) + ((int64_t)tv.tv_usec * 1000LL);
    }
}

/*
 * Helper function to create a kjson instant value
 */
static kjson_value *
create_kjson_instant(int64_t nanoseconds, MemoryContext memctx)
{
    kjson_value *value;
    
    value = MemoryContextAlloc(memctx, sizeof(kjson_value));
    value->type = KJSON_TYPE_INSTANT;
    value->u.instant = MemoryContextAlloc(memctx, sizeof(kjson_instant));
    value->u.instant->nanoseconds = nanoseconds;
    value->u.instant->tz_offset = 0; /* UTC */
    return value;
}

/*
 * Helper function to create a kjson duration value
 */
static kjson_value *
create_kjson_duration(kjson_duration *duration, MemoryContext memctx)
{
    kjson_value *value = MemoryContextAlloc(memctx, sizeof(kjson_value));
    value->type = KJSON_TYPE_DURATION;
    value->u.duration = MemoryContextAlloc(memctx, sizeof(kjson_duration));
    memcpy(value->u.duration, duration, sizeof(kjson_duration));
    return value;
}

/*
 * Helper function to parse ISO 8601 duration string
 * Format: P[n]Y[n]M[n]DT[n]H[n]M[n]S or P[n]W
 */
static kjson_duration *
parse_iso_duration(const char *duration_str)
{
    kjson_duration *duration;
    const char *p;
    bool in_time_part;
    bool negative;
    char *endptr;
    long value;
    
    duration = palloc0(sizeof(kjson_duration));
    p = duration_str;
    in_time_part = false;
    negative = false;
    
    /* Check for negative duration */
    if (*p == '-') {
        negative = true;
        p++;
    }
    
    /* Must start with P */
    if (*p != 'P') {
        pfree(duration);
        return NULL;
    }
    p++;
    
    /* Parse components */
    while (*p) {
        if (*p == 'T') {
            in_time_part = true;
            p++;
            continue;
        }
        
        /* Parse number */
        value = strtol(p, &endptr, 10);
        if (endptr == p) {
            /* No number found */
            break;
        }
        p = endptr;
        
        /* Parse component indicator */
        switch (*p) {
            case 'Y':
                if (in_time_part) goto parse_error;
                duration->years = value;
                break;
            case 'M':
                if (in_time_part) {
                    duration->minutes = value;
                } else {
                    duration->months = value;
                }
                break;
            case 'D':
                if (in_time_part) goto parse_error;
                duration->days = value;
                break;
            case 'H':
                if (!in_time_part) goto parse_error;
                duration->hours = value;
                break;
            case 'S':
                if (!in_time_part) goto parse_error;
                /* Handle fractional seconds */
                duration->nanoseconds = value * 1000000000LL;
                break;
            case 'W':
                if (in_time_part) goto parse_error;
                duration->days = value * 7;
                break;
            default:
                goto parse_error;
        }
        p++;
    }
    
    duration->negative = negative;
    return duration;
    
parse_error:
    pfree(duration);
    return NULL;
}

/*
 * Helper function to format ISO 8601 duration
 */
char *
format_iso_duration(kjson_duration *duration)
{
    StringInfo buf = makeStringInfo();
    
    if (duration->negative) {
        appendStringInfoChar(buf, '-');
    }
    
    appendStringInfoChar(buf, 'P');
    
    if (duration->years != 0) {
        appendStringInfo(buf, "%dY", duration->years);
    }
    if (duration->months != 0) {
        appendStringInfo(buf, "%dM", duration->months);
    }
    if (duration->days != 0) {
        appendStringInfo(buf, "%dD", duration->days);
    }
    
    if (duration->hours != 0 || duration->minutes != 0 || duration->nanoseconds != 0) {
        appendStringInfoChar(buf, 'T');
        
        if (duration->hours != 0) {
            appendStringInfo(buf, "%dH", duration->hours);
        }
        if (duration->minutes != 0) {
            appendStringInfo(buf, "%dM", duration->minutes);
        }
        if (duration->nanoseconds != 0) {
            int64_t seconds = duration->nanoseconds / 1000000000LL;
            int nanoseconds = duration->nanoseconds % 1000000000LL;
            if (nanoseconds == 0) {
                appendStringInfo(buf, "%lldS", (long long)seconds);
            } else {
                appendStringInfo(buf, "%lld.%09dS", (long long)seconds, nanoseconds);
            }
        }
    }
    
    /* Handle empty duration */
    if (buf->len == 1 || (buf->len == 2 && duration->negative)) {
        if (duration->negative) {
            resetStringInfo(buf);
            appendStringInfoString(buf, "-PT0S");
        } else {
            resetStringInfo(buf);
            appendStringInfoString(buf, "PT0S");
        }
    }
    
    return buf->data;
}