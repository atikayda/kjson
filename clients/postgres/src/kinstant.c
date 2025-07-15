/*
 * kinstant.c - Native PostgreSQL type for nanosecond-precision timestamps
 *
 * This file implements the kInstant type, which provides nanosecond-precision
 * timestamps with timezone offset support. This is a native PostgreSQL type
 * that can be used directly in table columns without needing to store as kjson.
 */

#include "postgres.h"
#include "fmgr.h"
#include "utils/builtins.h"
#include "utils/timestamp.h"
#include "utils/datetime.h"
#include "lib/stringinfo.h"
#include "utils/memutils.h"
#include "catalog/pg_type.h"
#include "libpq/pqformat.h"
#include <time.h>
#include <sys/time.h>

/* PG_MODULE_MAGIC is defined in kjson_io_native.c */

/* kInstant structure - 16 bytes total */
typedef struct {
    int64_t nanoseconds;    /* Nanoseconds since epoch (8 bytes) */
    int16_t tz_offset;      /* Timezone offset in minutes (2 bytes) */
    int16_t reserved;       /* Reserved for future use (2 bytes) */
    int32_t reserved2;      /* Reserved for future use (4 bytes) */
} kInstant;

/* Forward declarations */
static int64_t get_nanosecond_timestamp(void);
static char *format_kinstant(kInstant *instant);
static kInstant *parse_kinstant(const char *str);

/*
 * kinstant_in - Input function for kInstant type
 * 
 * Accepts ISO 8601 timestamps with optional timezone:
 * - 2023-12-25T10:30:45.123456789Z
 * - 2023-12-25T10:30:45.123456789+05:30
 * - 2023-12-25T10:30:45.123456789
 */
PG_FUNCTION_INFO_V1(kinstant_in);
Datum
kinstant_in(PG_FUNCTION_ARGS)
{
    char *str = PG_GETARG_CSTRING(0);
    kInstant *result;
    
    result = parse_kinstant(str);
    if (!result) {
        ereport(ERROR,
                (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION),
                 errmsg("invalid input syntax for type kInstant: \"%s\"", str)));
    }
    
    PG_RETURN_POINTER(result);
}

/*
 * kinstant_out - Output function for kInstant type
 * 
 * Outputs ISO 8601 format with nanosecond precision:
 * 2023-12-25T10:30:45.123456789Z
 */
PG_FUNCTION_INFO_V1(kinstant_out);
Datum
kinstant_out(PG_FUNCTION_ARGS)
{
    kInstant *instant = (kInstant *) PG_GETARG_POINTER(0);
    char *result;
    
    result = format_kinstant(instant);
    PG_RETURN_CSTRING(result);
}

/*
 * kinstant_recv - Binary input function for kInstant type
 */
PG_FUNCTION_INFO_V1(kinstant_recv);
Datum
kinstant_recv(PG_FUNCTION_ARGS)
{
    StringInfo buf = (StringInfo) PG_GETARG_POINTER(0);
    kInstant *result;
    
    result = (kInstant *) palloc(sizeof(kInstant));
    result->nanoseconds = pq_getmsgint64(buf);
    result->tz_offset = pq_getmsgint(buf, 2);
    result->reserved = pq_getmsgint(buf, 2);
    result->reserved2 = pq_getmsgint(buf, 4);
    
    PG_RETURN_POINTER(result);
}

/*
 * kinstant_send - Binary output function for kInstant type
 */
PG_FUNCTION_INFO_V1(kinstant_send);
Datum
kinstant_send(PG_FUNCTION_ARGS)
{
    kInstant *instant = (kInstant *) PG_GETARG_POINTER(0);
    StringInfoData buf;
    
    pq_begintypsend(&buf);
    pq_sendint64(&buf, instant->nanoseconds);
    pq_sendint(&buf, instant->tz_offset, 2);
    pq_sendint(&buf, instant->reserved, 2);
    pq_sendint(&buf, instant->reserved2, 4);
    
    PG_RETURN_BYTEA_P(pq_endtypsend(&buf));
}

/*
 * kinstant_now - Get current instant with nanosecond precision
 */
PG_FUNCTION_INFO_V1(kinstant_now);
Datum
kinstant_now(PG_FUNCTION_ARGS)
{
    kInstant *result;
    
    result = (kInstant *) palloc(sizeof(kInstant));
    result->nanoseconds = get_nanosecond_timestamp();
    result->tz_offset = 0; /* UTC */
    result->reserved = 0;
    result->reserved2 = 0;
    
    PG_RETURN_POINTER(result);
}

/*
 * kinstant_from_timestamp - Convert PostgreSQL timestamp to kInstant
 */
PG_FUNCTION_INFO_V1(kinstant_from_timestamp);
Datum
kinstant_from_timestamp(PG_FUNCTION_ARGS)
{
    Timestamp ts = PG_GETARG_TIMESTAMP(0);
    kInstant *result;
    int64_t pg_epoch_offset_microseconds = 946684800000000LL; /* 2000-01-01 00:00:00 UTC */
    int64_t unix_microseconds;
    
    result = (kInstant *) palloc(sizeof(kInstant));
    
    /* Convert PostgreSQL timestamp to nanoseconds since epoch */
    /* PostgreSQL timestamps are microseconds since 2000-01-01, convert to nanoseconds since 1970-01-01 */
    unix_microseconds = ts + pg_epoch_offset_microseconds;
    result->nanoseconds = unix_microseconds * 1000LL;
    result->tz_offset = 0; /* UTC */
    result->reserved = 0;
    result->reserved2 = 0;
    
    PG_RETURN_POINTER(result);
}

/*
 * kinstant_to_timestamp - Convert kInstant to PostgreSQL timestamp
 */
PG_FUNCTION_INFO_V1(kinstant_to_timestamp);
Datum
kinstant_to_timestamp(PG_FUNCTION_ARGS)
{
    kInstant *instant = (kInstant *) PG_GETARG_POINTER(0);
    Timestamp result;
    
    /* Convert nanoseconds since epoch to PostgreSQL timestamp */
    int64_t unix_microseconds = instant->nanoseconds / 1000LL;
    int64_t pg_epoch_offset_microseconds = 946684800000000LL;
    result = unix_microseconds - pg_epoch_offset_microseconds;
    
    PG_RETURN_TIMESTAMP(result);
}

/*
 * kinstant_eq - Equality operator for kInstant
 */
PG_FUNCTION_INFO_V1(kinstant_eq);
Datum
kinstant_eq(PG_FUNCTION_ARGS)
{
    kInstant *a = (kInstant *) PG_GETARG_POINTER(0);
    kInstant *b = (kInstant *) PG_GETARG_POINTER(1);
    
    PG_RETURN_BOOL(a->nanoseconds == b->nanoseconds && a->tz_offset == b->tz_offset);
}

/*
 * kinstant_ne - Not equal operator for kInstant
 */
PG_FUNCTION_INFO_V1(kinstant_ne);
Datum
kinstant_ne(PG_FUNCTION_ARGS)
{
    kInstant *a = (kInstant *) PG_GETARG_POINTER(0);
    kInstant *b = (kInstant *) PG_GETARG_POINTER(1);
    
    PG_RETURN_BOOL(a->nanoseconds != b->nanoseconds || a->tz_offset != b->tz_offset);
}

/*
 * kinstant_lt - Less than operator for kInstant
 */
PG_FUNCTION_INFO_V1(kinstant_lt);
Datum
kinstant_lt(PG_FUNCTION_ARGS)
{
    kInstant *a = (kInstant *) PG_GETARG_POINTER(0);
    kInstant *b = (kInstant *) PG_GETARG_POINTER(1);
    
    /* Convert to UTC for comparison */
    int64_t a_utc = a->nanoseconds - ((int64_t)a->tz_offset * 60LL * 1000000000LL);
    int64_t b_utc = b->nanoseconds - ((int64_t)b->tz_offset * 60LL * 1000000000LL);
    
    PG_RETURN_BOOL(a_utc < b_utc);
}

/*
 * kinstant_le - Less than or equal operator for kInstant
 */
PG_FUNCTION_INFO_V1(kinstant_le);
Datum
kinstant_le(PG_FUNCTION_ARGS)
{
    kInstant *a = (kInstant *) PG_GETARG_POINTER(0);
    kInstant *b = (kInstant *) PG_GETARG_POINTER(1);
    
    /* Convert to UTC for comparison */
    int64_t a_utc = a->nanoseconds - ((int64_t)a->tz_offset * 60LL * 1000000000LL);
    int64_t b_utc = b->nanoseconds - ((int64_t)b->tz_offset * 60LL * 1000000000LL);
    
    PG_RETURN_BOOL(a_utc <= b_utc);
}

/*
 * kinstant_gt - Greater than operator for kInstant
 */
PG_FUNCTION_INFO_V1(kinstant_gt);
Datum
kinstant_gt(PG_FUNCTION_ARGS)
{
    kInstant *a = (kInstant *) PG_GETARG_POINTER(0);
    kInstant *b = (kInstant *) PG_GETARG_POINTER(1);
    
    /* Convert to UTC for comparison */
    int64_t a_utc = a->nanoseconds - ((int64_t)a->tz_offset * 60LL * 1000000000LL);
    int64_t b_utc = b->nanoseconds - ((int64_t)b->tz_offset * 60LL * 1000000000LL);
    
    PG_RETURN_BOOL(a_utc > b_utc);
}

/*
 * kinstant_ge - Greater than or equal operator for kInstant
 */
PG_FUNCTION_INFO_V1(kinstant_ge);
Datum
kinstant_ge(PG_FUNCTION_ARGS)
{
    kInstant *a = (kInstant *) PG_GETARG_POINTER(0);
    kInstant *b = (kInstant *) PG_GETARG_POINTER(1);
    
    /* Convert to UTC for comparison */
    int64_t a_utc = a->nanoseconds - ((int64_t)a->tz_offset * 60LL * 1000000000LL);
    int64_t b_utc = b->nanoseconds - ((int64_t)b->tz_offset * 60LL * 1000000000LL);
    
    PG_RETURN_BOOL(a_utc >= b_utc);
}

/*
 * kinstant_cmp - Comparison function for kInstant (for btree indexes)
 */
PG_FUNCTION_INFO_V1(kinstant_cmp);
Datum
kinstant_cmp(PG_FUNCTION_ARGS)
{
    kInstant *a = (kInstant *) PG_GETARG_POINTER(0);
    kInstant *b = (kInstant *) PG_GETARG_POINTER(1);
    
    /* Convert to UTC for comparison */
    int64_t a_utc = a->nanoseconds - ((int64_t)a->tz_offset * 60LL * 1000000000LL);
    int64_t b_utc = b->nanoseconds - ((int64_t)b->tz_offset * 60LL * 1000000000LL);
    
    if (a_utc < b_utc)
        PG_RETURN_INT32(-1);
    else if (a_utc > b_utc)
        PG_RETURN_INT32(1);
    else
        PG_RETURN_INT32(0);
}

/*
 * Helper function to get current instant with nanosecond precision
 */
static int64_t
get_nanosecond_timestamp(void)
{
    struct timespec ts;
    
    /* Try to get nanosecond precision time */
    if (clock_gettime(CLOCK_REALTIME, &ts) == 0) {
        /* Convert to nanoseconds since epoch */
        return ((int64_t)ts.tv_sec * 1000000000LL) + (int64_t)ts.tv_nsec;
    } else {
        /* Fallback to microsecond precision */
        struct timeval tv;
        gettimeofday(&tv, NULL);
        return ((int64_t)tv.tv_sec * 1000000000LL) + ((int64_t)tv.tv_usec * 1000LL);
    }
}

/*
 * Helper function to format kInstant as ISO 8601 string
 */
static char *
format_kinstant(kInstant *instant)
{
    StringInfoData buf;
    time_t seconds;
    struct tm *tm;
    int nanoseconds;
    int hours, minutes;
    char sign;
    
    initStringInfo(&buf);
    
    /* Extract seconds and nanoseconds */
    seconds = instant->nanoseconds / 1000000000LL;
    nanoseconds = instant->nanoseconds % 1000000000LL;
    
    /* Convert to UTC time */
    tm = gmtime(&seconds);
    if (!tm) {
        pfree(buf.data);
        return pstrdup("1970-01-01T00:00:00.000000000Z");
    }
    
    /* Format timestamp */
    appendStringInfo(&buf, "%04d-%02d-%02dT%02d:%02d:%02d.%09d",
                     tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
                     tm->tm_hour, tm->tm_min, tm->tm_sec,
                     nanoseconds);
    
    /* Add timezone */
    if (instant->tz_offset == 0) {
        appendStringInfoChar(&buf, 'Z');
    } else {
        hours = abs(instant->tz_offset) / 60;
        minutes = abs(instant->tz_offset) % 60;
        sign = (instant->tz_offset >= 0) ? '+' : '-';
        appendStringInfo(&buf, "%c%02d:%02d", sign, hours, minutes);
    }
    
    return buf.data;
}

/*
 * Helper function to parse kInstant from ISO 8601 string
 */
static kInstant *
parse_kinstant(const char *str)
{
    kInstant *result;
    struct tm tm;
    time_t seconds;
    int nanoseconds = 0;
    int16_t tz_offset = 0;
    const char *p;
    char frac_str[16];
    int frac_len;
    int sign, hours, minutes;
    
    if (!str || strlen(str) < 19) {
        return NULL;
    }
    
    result = (kInstant *) palloc(sizeof(kInstant));
    memset(&tm, 0, sizeof(tm));
    
    /* Parse basic timestamp: YYYY-MM-DDTHH:MM:SS */
    if (sscanf(str, "%04d-%02d-%02dT%02d:%02d:%02d",
               &tm.tm_year, &tm.tm_mon, &tm.tm_mday,
               &tm.tm_hour, &tm.tm_min, &tm.tm_sec) != 6) {
        pfree(result);
        return NULL;
    }
    
    /* Adjust tm structure */
    tm.tm_year -= 1900;
    tm.tm_mon -= 1;
    
    /* Find fractional seconds */
    p = strchr(str, '.');
    if (p) {
        p++; /* Skip dot */
        frac_len = 0;
        
        /* Copy up to 9 digits */
        while (frac_len < 9 && isdigit(p[frac_len])) {
            frac_str[frac_len] = p[frac_len];
            frac_len++;
        }
        
        /* Pad with zeros if needed */
        while (frac_len < 9) {
            frac_str[frac_len] = '0';
            frac_len++;
        }
        frac_str[9] = '\0';
        
        nanoseconds = (int)strtol(frac_str, NULL, 10);
        
        /* Find timezone after fractional seconds */
        p += frac_len;
        while (*p && isdigit(*p)) p++; /* Skip any remaining digits */
    } else {
        /* Find timezone after seconds */
        p = str + 19; /* Skip YYYY-MM-DDTHH:MM:SS */
    }
    
    /* Parse timezone */
    if (*p == 'Z') {
        tz_offset = 0;
    } else if (*p == '+' || *p == '-') {
        sign = (*p == '+') ? 1 : -1;
        
        if (sscanf(p + 1, "%02d:%02d", &hours, &minutes) == 2) {
            tz_offset = sign * (hours * 60 + minutes);
        } else if (sscanf(p + 1, "%02d%02d", &hours, &minutes) == 2) {
            tz_offset = sign * (hours * 60 + minutes);
        } else {
            pfree(result);
            return NULL;
        }
    }
    
    /* Convert to seconds since epoch */
    seconds = timegm(&tm);
    if (seconds == -1) {
        pfree(result);
        return NULL;
    }
    
    /* Combine into nanoseconds */
    result->nanoseconds = ((int64_t)seconds * 1000000000LL) + nanoseconds;
    result->tz_offset = tz_offset;
    result->reserved = 0;
    result->reserved2 = 0;
    
    return result;
}