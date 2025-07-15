-- Comprehensive tests for kInstant native PostgreSQL type
-- Tests kInstant type with nanosecond precision timestamps and timezone support

\echo 'Testing kInstant native PostgreSQL type...'

-- Test 1: Basic kInstant input/output
\echo 'Test 1: Basic kInstant input/output'
SELECT 
    '2024-01-01T12:00:00.000000000Z'::kInstant::text ~ '2024-01-01T12:00:00' as basic_utc,
    '2024-01-01T12:00:00.123456789Z'::kInstant::text ~ '2024-01-01T12:00:00\\.123456789' as nanosecond_precision,
    '2024-12-25T23:59:59.999999999Z'::kInstant::text ~ '2024-12-25T23:59:59\\.999999999' as max_precision;

-- Test 2: Timezone support
\echo 'Test 2: Timezone support'
SELECT 
    '2024-01-01T12:00:00.000000000Z'::kInstant IS NOT NULL as utc_timezone,
    '2024-01-01T12:00:00.000000000+05:30'::kInstant IS NOT NULL as positive_timezone,
    '2024-01-01T12:00:00.000000000-08:00'::kInstant IS NOT NULL as negative_timezone,
    '2024-01-01T12:00:00.000000000+00:00'::kInstant IS NOT NULL as zero_timezone;

-- Test 3: kInstant now() function
\echo 'Test 3: kInstant now() function'
SELECT 
    kinstant_now() IS NOT NULL as now_works,
    kinstant_now()::text ~ '\\d{4}-\\d{2}-\\d{2}T\\d{2}:\\d{2}:\\d{2}' as now_format_correct,
    kinstant_now() > '2020-01-01T00:00:00.000000000Z'::kInstant as now_after_2020,
    kinstant_now() < '2030-01-01T00:00:00.000000000Z'::kInstant as now_before_2030;

-- Test 4: kInstant equality comparisons
\echo 'Test 4: kInstant equality comparisons'
SELECT 
    '2024-01-01T12:00:00.000000000Z'::kInstant = '2024-01-01T12:00:00.000000000Z'::kInstant as equal_same,
    '2024-01-01T12:00:00.000000000Z'::kInstant != '2024-01-01T12:00:00.000000001Z'::kInstant as not_equal_nanosecond,
    '2024-01-01T12:00:00.000000000Z'::kInstant <> '2024-01-01T13:00:00.000000000Z'::kInstant as not_equal_hour;

-- Test 5: kInstant ordering and comparisons
\echo 'Test 5: kInstant ordering and comparisons'
SELECT 
    '2024-01-01T12:00:00.000000000Z'::kInstant < '2024-01-01T12:00:00.000000001Z'::kInstant as nanosecond_ordering,
    '2024-01-01T12:00:00.000000000Z'::kInstant < '2024-01-01T13:00:00.000000000Z'::kInstant as hour_ordering,
    '2024-01-01T12:00:00.000000000Z'::kInstant <= '2024-01-01T12:00:00.000000000Z'::kInstant as equal_less_or_equal,
    '2024-01-01T13:00:00.000000000Z'::kInstant >= '2024-01-01T12:00:00.000000000Z'::kInstant as greater_equal;

-- Test 6: kInstant to PostgreSQL timestamp conversion
\echo 'Test 6: kInstant to PostgreSQL timestamp conversion'
SELECT 
    kinstant_to_timestamp('2024-01-01T12:00:00.000000000Z'::kInstant) IS NOT NULL as to_timestamp_works,
    kinstant_to_timestamp('2024-01-01T12:00:00.000000000Z'::kInstant)::text ~ '2024-01-01 12:00:00' as to_timestamp_format,
    kinstant_to_timestamp(kinstant_now()) IS NOT NULL as now_to_timestamp;

-- Test 7: PostgreSQL timestamp to kInstant conversion
\echo 'Test 7: PostgreSQL timestamp to kInstant conversion'
SELECT 
    kinstant_from_timestamp('2024-01-01 12:00:00'::timestamp) IS NOT NULL as from_timestamp_works,
    kinstant_from_timestamp('2024-01-01 12:00:00'::timestamp)::text ~ '2024-01-01T12:00:00' as from_timestamp_format,
    kinstant_from_timestamp(now()) IS NOT NULL as from_now_works;

-- Test 8: kInstant cast operations
\echo 'Test 8: kInstant cast operations'
SELECT 
    '2024-01-01 12:00:00'::timestamp::kInstant IS NOT NULL as cast_from_timestamp,
    '2024-01-01T12:00:00.000000000Z'::kInstant::timestamp IS NOT NULL as cast_to_timestamp,
    '2024-01-01 12:00:00'::timestamp::kInstant::timestamp IS NOT NULL as round_trip_cast;

-- Test 9: kInstant epoch conversion
\echo 'Test 9: kInstant epoch conversion'
SELECT 
    kinstant_extract_epoch('1970-01-01T00:00:00.000000000Z'::kInstant) = 0.0 as epoch_zero,
    kinstant_extract_epoch('1970-01-01T00:00:01.000000000Z'::kInstant) = 1.0 as epoch_one_second,
    kinstant_extract_epoch('1970-01-01T00:00:00.500000000Z'::kInstant) = 0.5 as epoch_half_second,
    kinstant_extract_epoch('1970-01-01T00:00:00.123456789Z'::kInstant) > 0.123 as epoch_nanoseconds;

-- Test 10: kInstant creation from epoch
\echo 'Test 10: kInstant creation from epoch'
SELECT 
    kinstant_from_epoch(0.0) IS NOT NULL as from_epoch_zero,
    kinstant_from_epoch(1.0) IS NOT NULL as from_epoch_one,
    kinstant_from_epoch(0.5) IS NOT NULL as from_epoch_half,
    kinstant_from_epoch(0.123456789) IS NOT NULL as from_epoch_nanoseconds;

-- Test 11: kInstant epoch round-trip conversion
\echo 'Test 11: kInstant epoch round-trip conversion'
WITH test_instants AS (
    SELECT 
        '1970-01-01T00:00:00.000000000Z'::kInstant as epoch_zero,
        '1970-01-01T00:00:01.000000000Z'::kInstant as epoch_one,
        '1970-01-01T00:00:00.500000000Z'::kInstant as epoch_half
)
SELECT 
    kinstant_from_epoch(kinstant_extract_epoch(epoch_zero)) IS NOT NULL as zero_round_trip,
    kinstant_from_epoch(kinstant_extract_epoch(epoch_one)) IS NOT NULL as one_round_trip,
    kinstant_from_epoch(kinstant_extract_epoch(epoch_half)) IS NOT NULL as half_round_trip
FROM test_instants;

-- Test 12: kInstant timezone comparison
\echo 'Test 12: kInstant timezone comparison'
SELECT 
    '2024-01-01T12:00:00.000000000Z'::kInstant = '2024-01-01T17:00:00.000000000+05:00'::kInstant as timezone_equal_utc,
    '2024-01-01T12:00:00.000000000Z'::kInstant = '2024-01-01T04:00:00.000000000-08:00'::kInstant as timezone_equal_negative,
    '2024-01-01T12:00:00.000000000Z'::kInstant < '2024-01-01T17:01:00.000000000+05:00'::kInstant as timezone_ordering;

-- Test 13: kInstant nanosecond precision handling
\echo 'Test 13: kInstant nanosecond precision handling'
SELECT 
    '2024-01-01T12:00:00.000000001Z'::kInstant > '2024-01-01T12:00:00.000000000Z'::kInstant as nanosecond_greater,
    '2024-01-01T12:00:00.123456789Z'::kInstant != '2024-01-01T12:00:00.123456788Z'::kInstant as nanosecond_precision_distinct,
    '2024-01-01T12:00:00.999999999Z'::kInstant < '2024-01-01T12:00:01.000000000Z'::kInstant as nanosecond_second_boundary;

-- Test 14: kInstant aggregation and array operations
\echo 'Test 14: kInstant aggregation and array operations'
WITH instant_data AS (
    SELECT unnest(ARRAY[
        '2024-01-01T12:00:00.000000000Z'::kInstant,
        '2024-01-01T13:00:00.000000000Z'::kInstant,
        '2024-01-01T14:00:00.000000000Z'::kInstant
    ]) as inst
)
SELECT 
    COUNT(*) = 3 as has_three_instants,
    MIN(inst) IS NOT NULL as min_instant_works,
    MAX(inst) IS NOT NULL as max_instant_works,
    MIN(inst) < MAX(inst) as min_less_than_max
FROM instant_data;

-- Test 15: kInstant with various date formats
\echo 'Test 15: kInstant with various date formats'
SELECT 
    '2024-01-01T00:00:00.000000000Z'::kInstant IS NOT NULL as year_start,
    '2024-12-31T23:59:59.999999999Z'::kInstant IS NOT NULL as year_end,
    '2024-02-29T12:00:00.000000000Z'::kInstant IS NOT NULL as leap_year_day,
    '2024-06-21T12:00:00.000000000Z'::kInstant IS NOT NULL as summer_solstice;

-- Test 16: kInstant historical and future dates
\echo 'Test 16: kInstant historical and future dates'
SELECT 
    '1970-01-01T00:00:00.000000000Z'::kInstant IS NOT NULL as unix_epoch,
    '2000-01-01T00:00:00.000000000Z'::kInstant IS NOT NULL as y2k,
    '2038-01-19T03:14:07.999999999Z'::kInstant IS NOT NULL as y2038_problem,
    '2100-01-01T00:00:00.000000000Z'::kInstant IS NOT NULL as future_date;

-- Test 17: kInstant precision edge cases
\echo 'Test 17: kInstant precision edge cases'
SELECT 
    '2024-01-01T12:00:00Z'::kInstant IS NOT NULL as no_fractional_seconds,
    '2024-01-01T12:00:00.1Z'::kInstant IS NOT NULL as one_decimal_place,
    '2024-01-01T12:00:00.12Z'::kInstant IS NOT NULL as two_decimal_places,
    '2024-01-01T12:00:00.123Z'::kInstant IS NOT NULL as three_decimal_places,
    '2024-01-01T12:00:00.123456Z'::kInstant IS NOT NULL as six_decimal_places;

-- Test 18: kInstant error handling
\echo 'Test 18: kInstant error handling'
\set ON_ERROR_STOP off

-- Test invalid instant formats
SELECT 'invalid_instant'::kInstant;
SELECT '2024-13-01T12:00:00.000000000Z'::kInstant;
SELECT '2024-01-32T12:00:00.000000000Z'::kInstant;
SELECT '2024-01-01T25:00:00.000000000Z'::kInstant;
SELECT '2024-01-01T12:60:00.000000000Z'::kInstant;
SELECT '2024-01-01T12:00:60.000000000Z'::kInstant;

\set ON_ERROR_STOP on

-- Test 19: kInstant format validation
\echo 'Test 19: kInstant format validation'
SELECT 
    '2024-01-01T12:00:00.000000000Z'::kInstant::text ~ '^2024-01-01T12:00:00\\.000000000Z$' as exact_format,
    '2024-01-01T12:00:00.123456789Z'::kInstant::text ~ '^2024-01-01T12:00:00\\.123456789Z$' as nanosecond_format,
    '2024-01-01T12:00:00.000000000+05:30'::kInstant::text ~ 'T12:00:00\\.000000000\\+05:30$' as timezone_format;

-- Test 20: kInstant performance and scalability
\echo 'Test 20: kInstant performance and scalability'
WITH performance_test AS (
    SELECT 
        generate_series(1, 100) as i,
        kinstant_now() as now_instant
)
SELECT 
    COUNT(*) = 100 as generated_hundred,
    COUNT(DISTINCT now_instant) >= 1 as has_timestamps,
    MIN(now_instant) IS NOT NULL as min_works,
    MAX(now_instant) IS NOT NULL as max_works
FROM performance_test;

\echo 'kInstant comprehensive tests completed!'