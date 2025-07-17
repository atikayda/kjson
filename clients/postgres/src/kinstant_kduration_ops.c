/*
 * kinstant_kduration_ops.c - Cross-type operations between kInstant and kDuration
 *
 * This file implements operations between kInstant and kDuration types,
 * including arithmetic operations and conversions.
 */

#include "postgres.h"
#include "fmgr.h"
#include "utils/builtins.h"
#include "lib/stringinfo.h"
#include "utils/memutils.h"
#include <time.h>


/* Structure definitions (must match the ones in individual type files) */
typedef struct {
    int64_t nanoseconds;    /* Nanoseconds since epoch */
    int16_t tz_offset;      /* Timezone offset in minutes */
    int16_t reserved;       /* Reserved for future use */
    int32_t reserved2;      /* Reserved for future use */
} kInstant;

typedef struct {
    int32_t years;          /* Years component */
    int32_t months;         /* Months component */
    int32_t days;           /* Days component */
    int32_t hours;          /* Hours component */
    int32_t minutes;        /* Minutes component */
    int64_t nanoseconds;    /* Seconds + fractional seconds as nanoseconds */
    bool negative;          /* True for negative durations */
    char reserved[7];       /* Reserved for future use */
} kDuration;

/*
 * kinstant_add_duration - Add a duration to an instant
 */
PG_FUNCTION_INFO_V1(kinstant_add_duration);
Datum
kinstant_add_duration(PG_FUNCTION_ARGS)
{
    kInstant *instant = (kInstant *) PG_GETARG_POINTER(0);
    kDuration *duration = (kDuration *) PG_GETARG_POINTER(1);
    kInstant *result;
    int64_t result_nanoseconds;
    
    result = (kInstant *) palloc(sizeof(kInstant));
    
    result_nanoseconds = instant->nanoseconds;
    
    if (duration->negative) {
        result_nanoseconds -= duration->nanoseconds;
        result_nanoseconds -= (int64_t)duration->minutes * 60LL * 1000000000LL;
        result_nanoseconds -= (int64_t)duration->hours * 3600LL * 1000000000LL;
        result_nanoseconds -= (int64_t)duration->days * 86400LL * 1000000000LL;
        result_nanoseconds -= (int64_t)duration->months * 30LL * 86400LL * 1000000000LL; /* Approximate */
        result_nanoseconds -= (int64_t)duration->years * 365LL * 86400LL * 1000000000LL; /* Approximate */
    } else {
        result_nanoseconds += duration->nanoseconds;
        result_nanoseconds += (int64_t)duration->minutes * 60LL * 1000000000LL;
        result_nanoseconds += (int64_t)duration->hours * 3600LL * 1000000000LL;
        result_nanoseconds += (int64_t)duration->days * 86400LL * 1000000000LL;
        result_nanoseconds += (int64_t)duration->months * 30LL * 86400LL * 1000000000LL; /* Approximate */
        result_nanoseconds += (int64_t)duration->years * 365LL * 86400LL * 1000000000LL; /* Approximate */
    }
    
    result->nanoseconds = result_nanoseconds;
    result->tz_offset = instant->tz_offset;
    result->reserved = 0;
    result->reserved2 = 0;
    
    PG_RETURN_POINTER(result);
}

/*
 * kinstant_subtract_duration - Subtract a duration from an instant
 */
PG_FUNCTION_INFO_V1(kinstant_subtract_duration);
Datum
kinstant_subtract_duration(PG_FUNCTION_ARGS)
{
    kInstant *instant = (kInstant *) PG_GETARG_POINTER(0);
    kDuration *duration = (kDuration *) PG_GETARG_POINTER(1);
    kInstant *result;
    int64_t result_nanoseconds;
    
    result = (kInstant *) palloc(sizeof(kInstant));
    
    result_nanoseconds = instant->nanoseconds;
    
    /* Subtract duration components (opposite sign logic) */
    if (duration->negative) {
        result_nanoseconds += duration->nanoseconds;
        result_nanoseconds += (int64_t)duration->minutes * 60LL * 1000000000LL;
        result_nanoseconds += (int64_t)duration->hours * 3600LL * 1000000000LL;
        result_nanoseconds += (int64_t)duration->days * 86400LL * 1000000000LL;
        result_nanoseconds += (int64_t)duration->months * 30LL * 86400LL * 1000000000LL; /* Approximate */
        result_nanoseconds += (int64_t)duration->years * 365LL * 86400LL * 1000000000LL; /* Approximate */
    } else {
        result_nanoseconds -= duration->nanoseconds;
        result_nanoseconds -= (int64_t)duration->minutes * 60LL * 1000000000LL;
        result_nanoseconds -= (int64_t)duration->hours * 3600LL * 1000000000LL;
        result_nanoseconds -= (int64_t)duration->days * 86400LL * 1000000000LL;
        result_nanoseconds -= (int64_t)duration->months * 30LL * 86400LL * 1000000000LL; /* Approximate */
        result_nanoseconds -= (int64_t)duration->years * 365LL * 86400LL * 1000000000LL; /* Approximate */
    }
    
    result->nanoseconds = result_nanoseconds;
    result->tz_offset = instant->tz_offset;
    result->reserved = 0;
    result->reserved2 = 0;
    
    PG_RETURN_POINTER(result);
}

/*
 * kinstant_subtract_instant - Subtract two instants to get a duration
 */
PG_FUNCTION_INFO_V1(kinstant_subtract_instant);
Datum
kinstant_subtract_instant(PG_FUNCTION_ARGS)
{
    kInstant *a = (kInstant *) PG_GETARG_POINTER(0);
    kInstant *b = (kInstant *) PG_GETARG_POINTER(1);
    kDuration *result;
    int64_t diff_nanoseconds;
    int64_t a_utc, b_utc;
    bool negative = false;
    
    result = (kDuration *) palloc0(sizeof(kDuration));
    
    a_utc = a->nanoseconds - ((int64_t)a->tz_offset * 60LL * 1000000000LL);
    b_utc = b->nanoseconds - ((int64_t)b->tz_offset * 60LL * 1000000000LL);
    
    diff_nanoseconds = a_utc - b_utc;
    
    if (diff_nanoseconds < 0) {
        negative = true;
        diff_nanoseconds = -diff_nanoseconds;
    }
    
    /* Store as nanoseconds - a more sophisticated implementation would break down into larger units */
    result->years = 0;
    result->months = 0;
    result->days = 0;
    result->hours = 0;
    result->minutes = 0;
    result->nanoseconds = diff_nanoseconds;
    result->negative = negative;
    
    PG_RETURN_POINTER(result);
}

/*
 * kinstant_extract_epoch - Extract Unix epoch seconds from kInstant
 */
PG_FUNCTION_INFO_V1(kinstant_extract_epoch);
Datum
kinstant_extract_epoch(PG_FUNCTION_ARGS)
{
    kInstant *instant = (kInstant *) PG_GETARG_POINTER(0);
    double result;
    
    result = (double)instant->nanoseconds / 1000000000.0;
    
    PG_RETURN_FLOAT8(result);
}

/*
 * kinstant_from_epoch - Create kInstant from Unix epoch seconds
 */
PG_FUNCTION_INFO_V1(kinstant_from_epoch);
Datum
kinstant_from_epoch(PG_FUNCTION_ARGS)
{
    double epoch_seconds = PG_GETARG_FLOAT8(0);
    kInstant *result;
    
    result = (kInstant *) palloc(sizeof(kInstant));
    result->nanoseconds = (int64_t)(epoch_seconds * 1000000000.0);
    result->tz_offset = 0;  /* UTC */
    result->reserved = 0;
    result->reserved2 = 0;
    
    PG_RETURN_POINTER(result);
}

/*
 * kduration_to_seconds - Convert kDuration to approximate total seconds
 */
PG_FUNCTION_INFO_V1(kduration_to_seconds);
Datum
kduration_to_seconds(PG_FUNCTION_ARGS)
{
    kDuration *duration = (kDuration *) PG_GETARG_POINTER(0);
    double result = 0.0;
    
    /* Convert each component to seconds */
    result += (double)duration->years * 365.0 * 24.0 * 3600.0;
    result += (double)duration->months * 30.0 * 24.0 * 3600.0;
    result += (double)duration->days * 24.0 * 3600.0;
    result += (double)duration->hours * 3600.0;
    result += (double)duration->minutes * 60.0;
    result += (double)duration->nanoseconds / 1000000000.0;
    
    if (duration->negative) {
        result = -result;
    }
    
    PG_RETURN_FLOAT8(result);
}

/*
 * kduration_from_seconds - Create kDuration from total seconds
 */
PG_FUNCTION_INFO_V1(kduration_from_seconds);
Datum
kduration_from_seconds(PG_FUNCTION_ARGS)
{
    double total_seconds = PG_GETARG_FLOAT8(0);
    kDuration *result;
    bool negative = false;
    int64_t nanoseconds;
    
    result = (kDuration *) palloc0(sizeof(kDuration));
    
    if (total_seconds < 0) {
        negative = true;
        total_seconds = -total_seconds;
    }
    
    nanoseconds = (int64_t)(total_seconds * 1000000000.0);
    
    result->years = 0;
    result->months = 0;
    result->days = 0;
    result->hours = 0;
    result->minutes = 0;
    result->nanoseconds = nanoseconds;
    result->negative = negative;
    
    PG_RETURN_POINTER(result);
}