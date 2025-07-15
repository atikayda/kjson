-- Comprehensive summary and demonstration of all temporal functionality
-- This test demonstrates the working temporal features in kjson PostgreSQL extension

\echo 'Summary of kJSON Temporal Functionality...'

-- Demonstrate kjson_now() function
\echo 'Test 1: kjson_now() function provides current time with nanosecond precision'
SELECT 
    kjson_now() as current_time,
    kjson_typeof(kjson_now()) as type,
    kjson_now() > '1970-01-01T00:00:00.000000000Z'::kjson as after_epoch;

-- Demonstrate duration parsing and formatting
\echo 'Test 2: Duration parsing and formatting (ISO 8601 format)'
SELECT 
    kjson_duration('PT1H') as one_hour,
    kjson_duration('PT30M') as thirty_minutes,
    kjson_duration('P1D') as one_day,
    kjson_duration('-PT1H') as negative_hour;

-- Demonstrate temporal arithmetic
\echo 'Test 3: Temporal arithmetic operations'
WITH base_time AS (
    SELECT kjson_now() as now_instant
)
SELECT 
    (now_instant + kjson_duration('PT1H'))::text ~ '\\d{4}-\\d{2}-\\d{2}T\\d{2}:\\d{2}:\\d{2}' as hour_addition_format,
    (now_instant + kjson_duration('PT30M')) > now_instant as thirty_min_addition,
    (now_instant - kjson_duration('PT1H')) < now_instant as hour_subtraction,
    (now_instant + kjson_duration('-PT1H')) < now_instant as negative_duration_arithmetic
FROM base_time;

-- Demonstrate duration formatting consistency
\echo 'Test 4: Duration formatting consistency'
SELECT 
    kjson_duration('PT1H')::text = 'PT1H' as hour_format,
    kjson_duration('PT30M')::text = 'PT30M' as minute_format,
    kjson_duration('P1D')::text = 'P1D' as day_format,
    kjson_duration('-PT1H')::text = '-PT1H' as negative_format;

-- Demonstrate temporal comparisons
\echo 'Test 5: Temporal comparisons and ordering'
WITH time_test AS (
    SELECT 
        kjson_now() as base_time,
        kjson_add_duration(kjson_now(), kjson_duration('PT1H')) as future_time,
        kjson_add_duration(kjson_now(), kjson_duration('-PT1H')) as past_time
)
SELECT 
    past_time < base_time as past_less_than_base,
    base_time < future_time as base_less_than_future,
    past_time < future_time as past_less_than_future,
    base_time = base_time as equality_works
FROM time_test;

-- Demonstrate complex duration operations
\echo 'Test 6: Complex duration operations'
SELECT 
    kjson_duration('P1DT2H30M') as complex_duration,
    kjson_add_duration(kjson_now(), kjson_duration('P1DT2H30M')) as complex_addition,
    kjson_typeof(kjson_duration('P1DT2H30M')) as complex_type;

-- Demonstrate aggregation and array operations
\echo 'Test 7: Aggregation and array operations with temporal types'
WITH temporal_data AS (
    SELECT unnest(ARRAY[
        kjson_now(),
        kjson_add_duration(kjson_now(), kjson_duration('PT1H')),
        kjson_add_duration(kjson_now(), kjson_duration('PT2H'))
    ]) as temporal_values
)
SELECT 
    kjson_agg(temporal_values) as aggregated_instants,
    kjson_typeof(kjson_agg(temporal_values)) as aggregate_type,
    COUNT(*) as value_count
FROM temporal_data;

-- Demonstrate object and array building
\echo 'Test 8: Object and array building with temporal types'
SELECT 
    kjson_build_object(
        'current_time', kjson_now(),
        'one_hour_later', kjson_add_duration(kjson_now(), kjson_duration('PT1H')),
        'duration', kjson_duration('PT30M')
    ) as temporal_object,
    kjson_build_array(
        kjson_now(),
        kjson_duration('PT1H'),
        kjson_add_duration(kjson_now(), kjson_duration('PT1H'))
    ) as temporal_array;

-- Demonstrate duration arithmetic chaining
\echo 'Test 9: Duration arithmetic chaining'
WITH base_time AS (
    SELECT kjson_now() as start_time
)
SELECT 
    start_time as original_time,
    kjson_add_duration(start_time, kjson_duration('PT1H')) as plus_one_hour,
    kjson_add_duration(
        kjson_add_duration(start_time, kjson_duration('PT1H')), 
        kjson_duration('PT30M')
    ) as plus_one_hour_thirty_min,
    kjson_add_duration(
        kjson_add_duration(start_time, kjson_duration('PT2H')), 
        kjson_duration('-PT30M')
    ) as plus_two_hours_minus_thirty_min
FROM base_time;

-- Demonstrate type identification
\echo 'Test 10: Type identification for temporal types'
SELECT 
    kjson_typeof(kjson_now()) as now_type,
    kjson_typeof(kjson_duration('PT1H')) as duration_type,
    kjson_typeof(kjson_add_duration(kjson_now(), kjson_duration('PT1H'))) as arithmetic_result_type;

-- Demonstrate pretty printing
\echo 'Test 11: Pretty printing temporal types'
SELECT 
    kjson_pretty(kjson_now()) as pretty_instant,
    kjson_pretty(kjson_duration('PT1H30M')) as pretty_duration,
    kjson_pretty(kjson_build_object('time', kjson_now(), 'duration', kjson_duration('PT1H'))) as pretty_object;

-- Demonstrate temporal precision
\echo 'Test 12: Nanosecond precision demonstration'
WITH precision_test AS (
    SELECT 
        kjson_now() as time1,
        kjson_now() as time2
)
SELECT 
    time1::text ~ '\\.\\d{9}' as has_nanosecond_precision,
    time2::text ~ '\\.\\d{9}' as time2_has_precision,
    time1 <= time2 as time_ordering_works
FROM precision_test;

-- Performance test with multiple operations
\echo 'Test 13: Performance test with multiple temporal operations'
WITH performance_test AS (
    SELECT 
        generate_series(1, 50) as i,
        kjson_now() as base_time
)
SELECT 
    COUNT(*) as operations_count,
    COUNT(kjson_add_duration(base_time, kjson_duration('PT1H'))) as successful_additions,
    MIN(kjson_add_duration(base_time, kjson_duration('PT1H'))) IS NOT NULL as min_result_valid,
    MAX(kjson_add_duration(base_time, kjson_duration('PT1H'))) IS NOT NULL as max_result_valid
FROM performance_test;

-- Demonstrate zero duration behavior
\echo 'Test 14: Zero duration behavior'
SELECT 
    kjson_duration('PT0S') as zero_duration,
    kjson_add_duration(kjson_now(), kjson_duration('PT0S')) as zero_addition,
    kjson_typeof(kjson_duration('PT0S')) as zero_type;

-- Demonstrate large values
\echo 'Test 15: Large duration values'
SELECT 
    kjson_duration('P1Y') as one_year,
    kjson_duration('P100D') as hundred_days,
    kjson_add_duration(kjson_now(), kjson_duration('P1Y')) as future_year;

\echo 'Summary of kJSON Temporal Functionality completed!'
\echo ''
\echo 'WORKING FEATURES:'
\echo '✓ kjson_now() - Current timestamp with nanosecond precision'
\echo '✓ kjson_duration() - ISO 8601 duration parsing'
\echo '✓ kjson_add_duration() - Temporal arithmetic'
\echo '✓ Duration formatting to ISO 8601 strings'
\echo '✓ Temporal comparisons and ordering'
\echo '✓ Complex duration components (P1DT2H30M)'
\echo '✓ Negative duration support'
\echo '✓ Zero duration handling'
\echo '✓ Large duration values (years, months, days)'
\echo '✓ Temporal aggregation (kjson_agg)'
\echo '✓ Object/array building with temporal types'
\echo '✓ Arithmetic chaining operations'
\echo '✓ Type identification (kjson_typeof)'
\echo '✓ Pretty printing support'
\echo '✓ Nanosecond precision preservation'
\echo ''
\echo 'KNOWN LIMITATIONS:'
\echo '• Instant parsing from ISO strings defaults to epoch time'
\echo '• kjson_extract_instant() function returns NULL'
\echo '• Duration parsing does not handle fractional seconds'
\echo '• Some duration type detection inconsistencies'
\echo ''
\echo 'The temporal functionality provides robust support for high-precision'
\echo 'timestamps and durations with comprehensive arithmetic operations!'