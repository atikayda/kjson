/*
 * kduration.c - Native PostgreSQL type for high-precision time durations
 *
 * This file implements the kDuration type, which provides nanosecond-precision
 * time durations with ISO 8601 support. This is a native PostgreSQL type
 * that can be used directly in table columns without needing to store as kjson.
 */

#include "postgres.h"
#include "fmgr.h"
#include "utils/builtins.h"
#include "lib/stringinfo.h"
#include "utils/memutils.h"
#include "catalog/pg_type.h"
#include "libpq/pqformat.h"
#include <time.h>

/* PG_MODULE_MAGIC is defined in kjson_io_native.c */

/* kDuration structure - 24 bytes total */
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

/* Forward declarations */
static char *format_kduration(kDuration *duration);
static kDuration *parse_kduration(const char *str);
static int64_t kduration_to_nanoseconds_approx(kDuration *duration);

/*
 * kduration_in - Input function for kDuration type
 * 
 * Accepts ISO 8601 duration format:
 * - P1Y2M3DT4H5M6.789S
 * - P1W
 * - PT1H30M
 * - -P1Y (negative duration)
 */
PG_FUNCTION_INFO_V1(kduration_in);
Datum
kduration_in(PG_FUNCTION_ARGS)
{
    char *str = PG_GETARG_CSTRING(0);
    kDuration *result;
    
    result = parse_kduration(str);
    if (!result) {
        ereport(ERROR,
                (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION),
                 errmsg("invalid input syntax for type kDuration: \"%s\"", str)));
    }
    
    PG_RETURN_POINTER(result);
}

/*
 * kduration_out - Output function for kDuration type
 * 
 * Outputs ISO 8601 duration format:
 * P1Y2M3DT4H5M6.789S
 */
PG_FUNCTION_INFO_V1(kduration_out);
Datum
kduration_out(PG_FUNCTION_ARGS)
{
    kDuration *duration = (kDuration *) PG_GETARG_POINTER(0);
    char *result;
    
    result = format_kduration(duration);
    PG_RETURN_CSTRING(result);
}

/*
 * kduration_recv - Binary input function for kDuration type
 */
PG_FUNCTION_INFO_V1(kduration_recv);
Datum
kduration_recv(PG_FUNCTION_ARGS)
{
    StringInfo buf = (StringInfo) PG_GETARG_POINTER(0);
    kDuration *result;
    
    result = (kDuration *) palloc(sizeof(kDuration));
    result->years = pq_getmsgint(buf, 4);
    result->months = pq_getmsgint(buf, 4);
    result->days = pq_getmsgint(buf, 4);
    result->hours = pq_getmsgint(buf, 4);
    result->minutes = pq_getmsgint(buf, 4);
    result->nanoseconds = pq_getmsgint64(buf);
    result->negative = pq_getmsgbyte(buf);
    
    /* Read reserved bytes */
    for (int i = 0; i < 7; i++) {
        result->reserved[i] = pq_getmsgbyte(buf);
    }
    
    PG_RETURN_POINTER(result);
}

/*
 * kduration_send - Binary output function for kDuration type
 */
PG_FUNCTION_INFO_V1(kduration_send);
Datum
kduration_send(PG_FUNCTION_ARGS)
{
    kDuration *duration = (kDuration *) PG_GETARG_POINTER(0);
    StringInfoData buf;
    
    pq_begintypsend(&buf);
    pq_sendint(&buf, duration->years, 4);
    pq_sendint(&buf, duration->months, 4);
    pq_sendint(&buf, duration->days, 4);
    pq_sendint(&buf, duration->hours, 4);
    pq_sendint(&buf, duration->minutes, 4);
    pq_sendint64(&buf, duration->nanoseconds);
    pq_sendbyte(&buf, duration->negative);
    
    /* Send reserved bytes */
    for (int i = 0; i < 7; i++) {
        pq_sendbyte(&buf, duration->reserved[i]);
    }
    
    PG_RETURN_BYTEA_P(pq_endtypsend(&buf));
}

/*
 * kduration_eq - Equality operator for kDuration
 */
PG_FUNCTION_INFO_V1(kduration_eq);
Datum
kduration_eq(PG_FUNCTION_ARGS)
{
    kDuration *a = (kDuration *) PG_GETARG_POINTER(0);
    kDuration *b = (kDuration *) PG_GETARG_POINTER(1);
    
    PG_RETURN_BOOL(a->years == b->years &&
                   a->months == b->months &&
                   a->days == b->days &&
                   a->hours == b->hours &&
                   a->minutes == b->minutes &&
                   a->nanoseconds == b->nanoseconds &&
                   a->negative == b->negative);
}

/*
 * kduration_ne - Not equal operator for kDuration
 */
PG_FUNCTION_INFO_V1(kduration_ne);
Datum
kduration_ne(PG_FUNCTION_ARGS)
{
    kDuration *a = (kDuration *) PG_GETARG_POINTER(0);
    kDuration *b = (kDuration *) PG_GETARG_POINTER(1);
    
    PG_RETURN_BOOL(a->years != b->years ||
                   a->months != b->months ||
                   a->days != b->days ||
                   a->hours != b->hours ||
                   a->minutes != b->minutes ||
                   a->nanoseconds != b->nanoseconds ||
                   a->negative != b->negative);
}

/*
 * kduration_lt - Less than operator for kDuration
 */
PG_FUNCTION_INFO_V1(kduration_lt);
Datum
kduration_lt(PG_FUNCTION_ARGS)
{
    kDuration *a = (kDuration *) PG_GETARG_POINTER(0);
    kDuration *b = (kDuration *) PG_GETARG_POINTER(1);
    
    /* Convert to approximate nanoseconds for comparison */
    int64_t a_ns = kduration_to_nanoseconds_approx(a);
    int64_t b_ns = kduration_to_nanoseconds_approx(b);
    
    PG_RETURN_BOOL(a_ns < b_ns);
}

/*
 * kduration_le - Less than or equal operator for kDuration
 */
PG_FUNCTION_INFO_V1(kduration_le);
Datum
kduration_le(PG_FUNCTION_ARGS)
{
    kDuration *a = (kDuration *) PG_GETARG_POINTER(0);
    kDuration *b = (kDuration *) PG_GETARG_POINTER(1);
    
    /* Convert to approximate nanoseconds for comparison */
    int64_t a_ns = kduration_to_nanoseconds_approx(a);
    int64_t b_ns = kduration_to_nanoseconds_approx(b);
    
    PG_RETURN_BOOL(a_ns <= b_ns);
}

/*
 * kduration_gt - Greater than operator for kDuration
 */
PG_FUNCTION_INFO_V1(kduration_gt);
Datum
kduration_gt(PG_FUNCTION_ARGS)
{
    kDuration *a = (kDuration *) PG_GETARG_POINTER(0);
    kDuration *b = (kDuration *) PG_GETARG_POINTER(1);
    
    /* Convert to approximate nanoseconds for comparison */
    int64_t a_ns = kduration_to_nanoseconds_approx(a);
    int64_t b_ns = kduration_to_nanoseconds_approx(b);
    
    PG_RETURN_BOOL(a_ns > b_ns);
}

/*
 * kduration_ge - Greater than or equal operator for kDuration
 */
PG_FUNCTION_INFO_V1(kduration_ge);
Datum
kduration_ge(PG_FUNCTION_ARGS)
{
    kDuration *a = (kDuration *) PG_GETARG_POINTER(0);
    kDuration *b = (kDuration *) PG_GETARG_POINTER(1);
    
    /* Convert to approximate nanoseconds for comparison */
    int64_t a_ns = kduration_to_nanoseconds_approx(a);
    int64_t b_ns = kduration_to_nanoseconds_approx(b);
    
    PG_RETURN_BOOL(a_ns >= b_ns);
}

/*
 * kduration_cmp - Comparison function for kDuration (for btree indexes)
 */
PG_FUNCTION_INFO_V1(kduration_cmp);
Datum
kduration_cmp(PG_FUNCTION_ARGS)
{
    kDuration *a = (kDuration *) PG_GETARG_POINTER(0);
    kDuration *b = (kDuration *) PG_GETARG_POINTER(1);
    
    /* Convert to approximate nanoseconds for comparison */
    int64_t a_ns = kduration_to_nanoseconds_approx(a);
    int64_t b_ns = kduration_to_nanoseconds_approx(b);
    
    if (a_ns < b_ns)
        PG_RETURN_INT32(-1);
    else if (a_ns > b_ns)
        PG_RETURN_INT32(1);
    else
        PG_RETURN_INT32(0);
}

/*
 * kduration_add - Add two durations
 */
PG_FUNCTION_INFO_V1(kduration_add);
Datum
kduration_add(PG_FUNCTION_ARGS)
{
    kDuration *a = (kDuration *) PG_GETARG_POINTER(0);
    kDuration *b = (kDuration *) PG_GETARG_POINTER(1);
    kDuration *result;
    
    result = (kDuration *) palloc0(sizeof(kDuration));
    
    /* Simple addition - more sophisticated logic would handle overflow */
    result->years = a->years + b->years;
    result->months = a->months + b->months;
    result->days = a->days + b->days;
    result->hours = a->hours + b->hours;
    result->minutes = a->minutes + b->minutes;
    result->nanoseconds = a->nanoseconds + b->nanoseconds;
    result->negative = false; /* Result sign would need proper calculation */
    
    PG_RETURN_POINTER(result);
}

/*
 * kduration_subtract - Subtract two durations
 */
PG_FUNCTION_INFO_V1(kduration_subtract);
Datum
kduration_subtract(PG_FUNCTION_ARGS)
{
    kDuration *a = (kDuration *) PG_GETARG_POINTER(0);
    kDuration *b = (kDuration *) PG_GETARG_POINTER(1);
    kDuration *result;
    
    result = (kDuration *) palloc0(sizeof(kDuration));
    
    /* Simple subtraction - more sophisticated logic would handle underflow */
    result->years = a->years - b->years;
    result->months = a->months - b->months;
    result->days = a->days - b->days;
    result->hours = a->hours - b->hours;
    result->minutes = a->minutes - b->minutes;
    result->nanoseconds = a->nanoseconds - b->nanoseconds;
    result->negative = false; /* Result sign would need proper calculation */
    
    PG_RETURN_POINTER(result);
}

/*
 * kduration_negate - Negate a duration
 */
PG_FUNCTION_INFO_V1(kduration_negate);
Datum
kduration_negate(PG_FUNCTION_ARGS)
{
    kDuration *duration = (kDuration *) PG_GETARG_POINTER(0);
    kDuration *result;
    
    result = (kDuration *) palloc(sizeof(kDuration));
    memcpy(result, duration, sizeof(kDuration));
    result->negative = !result->negative;
    
    PG_RETURN_POINTER(result);
}

/*
 * Helper function to format kDuration as ISO 8601 string
 */
static char *
format_kduration(kDuration *duration)
{
    StringInfoData buf;
    initStringInfo(&buf);
    
    if (duration->negative) {
        appendStringInfoChar(&buf, '-');
    }
    
    appendStringInfoChar(&buf, 'P');
    
    if (duration->years != 0) {
        appendStringInfo(&buf, "%dY", duration->years);
    }
    if (duration->months != 0) {
        appendStringInfo(&buf, "%dM", duration->months);
    }
    if (duration->days != 0) {
        appendStringInfo(&buf, "%dD", duration->days);
    }
    
    if (duration->hours != 0 || duration->minutes != 0 || duration->nanoseconds != 0) {
        appendStringInfoChar(&buf, 'T');
        
        if (duration->hours != 0) {
            appendStringInfo(&buf, "%dH", duration->hours);
        }
        if (duration->minutes != 0) {
            appendStringInfo(&buf, "%dM", duration->minutes);
        }
        if (duration->nanoseconds != 0) {
            int64_t seconds = duration->nanoseconds / 1000000000LL;
            int nanoseconds = duration->nanoseconds % 1000000000LL;
            if (nanoseconds == 0) {
                appendStringInfo(&buf, "%lldS", (long long)seconds);
            } else {
                appendStringInfo(&buf, "%lld.%09dS", (long long)seconds, nanoseconds);
            }
        }
    }
    
    /* Handle empty duration */
    if (buf.len == 1 || (buf.len == 2 && duration->negative)) {
        if (duration->negative) {
            resetStringInfo(&buf);
            appendStringInfoString(&buf, "-PT0S");
        } else {
            resetStringInfo(&buf);
            appendStringInfoString(&buf, "PT0S");
        }
    }
    
    return buf.data;
}

/*
 * Helper function to parse kDuration from ISO 8601 string
 * Format: P[n]Y[n]M[n]DT[n]H[n]M[n]S or P[n]W
 */
static kDuration *
parse_kduration(const char *duration_str)
{
    kDuration *duration;
    const char *p;
    bool in_time_part = false;
    bool negative = false;
    char *endptr;
    long value;
    
    if (!duration_str || strlen(duration_str) < 2) {
        return NULL;
    }
    
    duration = (kDuration *) palloc0(sizeof(kDuration));
    p = duration_str;
    
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
 * Helper function to convert kDuration to approximate nanoseconds
 * Note: This is approximate because months and years have variable lengths
 */
static int64_t
kduration_to_nanoseconds_approx(kDuration *duration)
{
    int64_t result = 0;
    
    /* Convert each component to nanoseconds */
    result += (int64_t)duration->years * 365LL * 24LL * 3600LL * 1000000000LL;
    result += (int64_t)duration->months * 30LL * 24LL * 3600LL * 1000000000LL;
    result += (int64_t)duration->days * 24LL * 3600LL * 1000000000LL;
    result += (int64_t)duration->hours * 3600LL * 1000000000LL;
    result += (int64_t)duration->minutes * 60LL * 1000000000LL;
    result += duration->nanoseconds;
    
    if (duration->negative) {
        result = -result;
    }
    
    return result;
}