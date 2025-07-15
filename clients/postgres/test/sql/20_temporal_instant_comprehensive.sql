-- Comprehensive tests for new temporal instant functionality
-- Tests kjson_now, kjson_duration, kjson_add_duration, kjson_extract_instant

\echo 'Testing new temporal instant functionality...'

-- Test kjson_now() function
\echo 'Test 1: kjson_now() function'
SELECT 
    kjson_now() IS NOT NULL as has_now,
    kjson_typeof(kjson_now()) = 'instant' as correct_type,
    kjson_now()::text ~ '^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}\.\d{9}Z$' as correct_format;

-- Test nanosecond precision preservation
\echo 'Test 2: Nanosecond precision parsing and formatting'
SELECT 
    '{"precise": "2024-01-01T12:00:00.123456789Z"}'::kjson->>'precise' = '2024-01-01T12:00:00.123456789Z' as preserves_nanoseconds,
    kjson_typeof('{"instant": "2024-01-01T12:00:00.123456789Z"}'::kjson->'instant') = 'instant' as parses_as_instant;

-- Test kjson_extract_instant function
\echo 'Test 3: kjson_extract_instant function'
SELECT 
    kjson_extract_instant('{"timestamp": "2024-01-15T10:30:45.123456789Z"}'::kjson, 'timestamp') IS NOT NULL as can_extract,
    kjson_extract_instant('{"nested": {"time": "2024-01-01T00:00:00.000000001Z"}}'::kjson, 'nested', 'time') IS NOT NULL as can_extract_nested;

-- Test kjson_duration function with various ISO formats
\echo 'Test 4: kjson_duration function with various ISO formats'
SELECT 
    kjson_typeof(kjson_duration('PT1H30M')) = 'duration' as simple_duration,
    kjson_typeof(kjson_duration('P1Y2M3DT4H5M6.123456789S')) = 'duration' as complex_duration,
    kjson_typeof(kjson_duration('PT30M')) = 'duration' as minutes_only,
    kjson_typeof(kjson_duration('P1D')) = 'duration' as days_only,
    kjson_typeof(kjson_duration('-PT1H')) = 'duration' as negative_duration;

-- Test duration formatting
\echo 'Test 5: Duration formatting'
SELECT 
    kjson_duration('PT1H30M')::text ~ '^PT1H30M$' as formats_correctly,
    kjson_duration('P1DT2H30M')::text ~ '^P1DT2H30M$' as formats_complex,
    kjson_duration('-PT30M')::text ~ '^-PT30M$' as formats_negative;

-- Test kjson_add_duration function
\echo 'Test 6: kjson_add_duration function'
WITH test_data AS (
    SELECT 
        '{"time": "2024-01-01T12:00:00.000000000Z"}'::kjson->'time' as base_instant,
        kjson_duration('PT1H') as one_hour,
        kjson_duration('PT30M') as thirty_minutes,
        kjson_duration('-PT15M') as minus_fifteen_minutes
)
SELECT 
    kjson_typeof(kjson_add_duration(base_instant, one_hour)) = 'instant' as adds_positive_duration,
    kjson_typeof(kjson_add_duration(base_instant, minus_fifteen_minutes)) = 'instant' as adds_negative_duration,
    kjson_add_duration(base_instant, one_hour) IS NOT NULL as addition_works,
    kjson_add_duration(base_instant, thirty_minutes) != base_instant as produces_different_result
FROM test_data;

-- Test temporal arithmetic with complex durations
\echo 'Test 7: Complex temporal arithmetic'
WITH test_data AS (
    SELECT 
        kjson_now() as now_instant,
        kjson_duration('P1Y2M3DT4H5M6.123456789S') as complex_duration
)
SELECT 
    kjson_add_duration(now_instant, complex_duration) IS NOT NULL as complex_addition_works,
    kjson_add_duration(now_instant, complex_duration) != now_instant as complex_produces_different_result
FROM test_data;

-- Test error handling
\echo 'Test 8: Error handling'
\set ON_ERROR_STOP off

-- Test invalid duration format
SELECT kjson_duration('invalid');

-- Test adding duration to non-instant
SELECT kjson_add_duration('{"not": "instant"}'::kjson, kjson_duration('PT1H'));

-- Test adding non-duration to instant
SELECT kjson_add_duration(kjson_now(), '{"not": "duration"}'::kjson);

\set ON_ERROR_STOP on

-- Test instant comparison and ordering
\echo 'Test 9: Instant comparison and ordering'
WITH test_data AS (
    SELECT 
        '{"early": "2024-01-01T00:00:00.000000000Z"}'::kjson->'early' as early_instant,
        '{"late": "2024-01-01T23:59:59.999999999Z"}'::kjson->'late' as late_instant
)
SELECT 
    early_instant < late_instant as comparison_works,
    early_instant != late_instant as inequality_works,
    early_instant = early_instant as equality_works
FROM test_data;

-- Test instant extraction to PostgreSQL timestamps
\echo 'Test 10: Instant extraction to PostgreSQL timestamps'
SELECT 
    kjson_extract_instant('{"time": "2024-01-01T12:00:00.000000000Z"}'::kjson, 'time') IS NOT NULL as extraction_works,
    kjson_extract_instant('{"time": "2024-01-01T12:00:00.123456Z"}'::kjson, 'time')::text ~ '2024-01-01' as contains_date;

-- Test duration components
\echo 'Test 11: Duration with different components'
SELECT 
    kjson_duration('PT0S')::text = 'PT0S' as zero_duration,
    kjson_duration('P1Y')::text = 'P1Y' as year_only,
    kjson_duration('P1M')::text = 'P1M' as month_only,
    kjson_duration('P7D')::text = 'P7D' as week_as_days,
    kjson_duration('PT1.5S')::text ~ 'PT1\.5\d*S' as fractional_seconds;

-- Test chaining operations
\echo 'Test 12: Chaining temporal operations'
WITH test_data AS (
    SELECT 
        kjson_now() as base_time,
        kjson_duration('PT1H') as hour,
        kjson_duration('PT30M') as half_hour
)
SELECT 
    kjson_add_duration(kjson_add_duration(base_time, hour), half_hour) IS NOT NULL as chaining_works,
    kjson_add_duration(kjson_add_duration(base_time, hour), half_hour) != base_time as chaining_produces_change
FROM test_data;

-- Test pretty printing with instants and durations
\echo 'Test 13: Pretty printing temporal types'
SELECT 
    kjson_pretty(kjson_now()) ~ '\d{4}-\d{2}-\d{2}T' as pretty_instant_format,
    kjson_pretty(kjson_duration('P1DT2H30M')) ~ 'P1DT2H30M' as pretty_duration_format;

-- Test performance with multiple operations
\echo 'Test 14: Performance test with multiple temporal operations'
WITH RECURSIVE time_series AS (
    SELECT 
        1 as n,
        kjson_now() as current_time,
        kjson_duration('PT1M') as minute_interval
    UNION ALL
    SELECT 
        n + 1,
        kjson_add_duration(current_time, minute_interval),
        minute_interval
    FROM time_series
    WHERE n < 5
)
SELECT 
    COUNT(*) = 5 as generated_series,
    MAX(current_time) > MIN(current_time) as time_progresses
FROM time_series;

-- Test aggregation with temporal types
\echo 'Test 15: Aggregation with temporal types'
SELECT 
    kjson_agg(kjson_now()) IS NOT NULL as can_aggregate_instants,
    kjson_agg(kjson_duration('PT1H')) IS NOT NULL as can_aggregate_durations,
    kjson_typeof(kjson_agg(kjson_now())) = 'array' as aggregates_to_array;

-- Test object building with temporal types
\echo 'Test 16: Object building with temporal types'
SELECT 
    kjson_build_object('now', kjson_now(), 'duration', kjson_duration('PT1H')) IS NOT NULL as can_build_object,
    kjson_typeof(kjson_build_object('now', kjson_now())) = 'object' as builds_object_type;

-- Test temporal data in arrays
\echo 'Test 17: Temporal data in arrays'
SELECT 
    kjson_build_array(kjson_now(), kjson_duration('PT1H')) IS NOT NULL as can_build_array,
    kjson_typeof(kjson_build_array(kjson_now(), kjson_duration('PT1H'))) = 'array' as builds_array_type;

-- Test round-trip consistency
\echo 'Test 18: Round-trip consistency'
WITH test_data AS (
    SELECT 
        '{"instant": "2024-01-01T12:00:00.123456789Z", "duration": "PT1H30M45.123456789S"}'::kjson as original
)
SELECT 
    original::text::kjson = original as round_trip_consistent,
    (original->'instant')::text::kjson = (original->'instant') as instant_round_trip,
    (original->'duration')::text::kjson = (original->'duration') as duration_round_trip
FROM test_data;

\echo 'All temporal instant tests completed!'