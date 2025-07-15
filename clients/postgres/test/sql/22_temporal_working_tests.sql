-- Working tests for temporal functionality
-- These tests focus on functionality that is currently working

\echo 'Testing working temporal functionality...'

-- Test 1: kjson_now() function works
\echo 'Test 1: kjson_now() function'
SELECT 
    kjson_now() IS NOT NULL as has_now,
    kjson_typeof(kjson_now()) = 'instant' as correct_type,
    kjson_now() > '1970-01-01T00:00:00.000000000Z'::kjson as after_epoch;

-- Test 2: Basic duration parsing (working)
\echo 'Test 2: Basic duration parsing'
SELECT 
    kjson_typeof(kjson_duration('PT1H')) = 'duration' as hour_duration,
    kjson_typeof(kjson_duration('PT30M')) = 'duration' as minute_duration,
    kjson_typeof(kjson_duration('P1D')) = 'duration' as day_duration,
    kjson_typeof(kjson_duration('-PT1H')) = 'duration' as negative_duration;

-- Test 3: Duration formatting
\echo 'Test 3: Duration formatting'
SELECT 
    kjson_duration('PT1H')::text = 'PT1H' as formats_hour,
    kjson_duration('PT30M')::text = 'PT30M' as formats_minutes,
    kjson_duration('P1D')::text = 'P1D' as formats_day,
    kjson_duration('-PT1H')::text = '-PT1H' as formats_negative;

-- Test 4: Basic temporal arithmetic
\echo 'Test 4: Basic temporal arithmetic'
WITH test_data AS (
    SELECT 
        kjson_now() as base_time,
        kjson_duration('PT1H') as one_hour,
        kjson_duration('PT30M') as thirty_minutes
)
SELECT 
    kjson_typeof(kjson_add_duration(base_time, one_hour)) = 'instant' as addition_type_correct,
    kjson_add_duration(base_time, one_hour) > base_time as future_time,
    kjson_add_duration(base_time, thirty_minutes) > base_time as future_time_30m,
    kjson_add_duration(base_time, one_hour) > kjson_add_duration(base_time, thirty_minutes) as hour_greater_than_30m
FROM test_data;

-- Test 5: Negative duration arithmetic
\echo 'Test 5: Negative duration arithmetic'
WITH test_data AS (
    SELECT 
        kjson_now() as base_time,
        kjson_duration('-PT1H') as minus_one_hour
)
SELECT 
    kjson_add_duration(base_time, minus_one_hour) < base_time as past_time,
    kjson_typeof(kjson_add_duration(base_time, minus_one_hour)) = 'instant' as type_correct
FROM test_data;

-- Test 6: Duration with multiple components
\echo 'Test 6: Duration with multiple components'
SELECT 
    kjson_duration('P1DT1H30M')::text = 'P1DT1H30M' as complex_duration,
    kjson_typeof(kjson_duration('P1DT1H30M')) = 'duration' as complex_type,
    kjson_add_duration(kjson_now(), kjson_duration('P1DT1H30M')) > kjson_now() as complex_arithmetic;

-- Test 7: Zero duration
\echo 'Test 7: Zero duration'
SELECT 
    kjson_duration('PT0S')::text = 'PT0S' as zero_duration,
    kjson_add_duration(kjson_now(), kjson_duration('PT0S')) = kjson_now() as zero_addition;

-- Test 8: Large durations
\echo 'Test 8: Large durations'
SELECT 
    kjson_duration('P1Y')::text = 'P1Y' as year_duration,
    kjson_duration('P1M')::text = 'P1M' as month_duration,
    kjson_add_duration(kjson_now(), kjson_duration('P1Y')) > kjson_now() as year_addition;

-- Test 9: Chaining operations
\echo 'Test 9: Chaining operations'
WITH test_data AS (
    SELECT 
        kjson_now() as base_time,
        kjson_duration('PT1H') as hour,
        kjson_duration('PT30M') as half_hour
)
SELECT 
    kjson_add_duration(kjson_add_duration(base_time, hour), half_hour) > base_time as chained_greater,
    kjson_typeof(kjson_add_duration(kjson_add_duration(base_time, hour), half_hour)) = 'instant' as chained_type
FROM test_data;

-- Test 10: Duration aggregation
\echo 'Test 10: Duration aggregation'
SELECT 
    kjson_agg(kjson_duration('PT1H')) IS NOT NULL as can_aggregate,
    kjson_typeof(kjson_agg(kjson_duration('PT1H'))) = 'array' as aggregates_to_array;

-- Test 11: Object building with working temporal types
\echo 'Test 11: Object building with temporal types'
SELECT 
    kjson_build_object('now', kjson_now(), 'duration', kjson_duration('PT1H')) IS NOT NULL as builds_object,
    kjson_typeof(kjson_build_object('now', kjson_now())) = 'object' as correct_type;

-- Test 12: Array building with temporal types
\echo 'Test 12: Array building with temporal types'
SELECT 
    kjson_build_array(kjson_now(), kjson_duration('PT1H')) IS NOT NULL as builds_array,
    kjson_typeof(kjson_build_array(kjson_now(), kjson_duration('PT1H'))) = 'array' as correct_type;

-- Test 13: Instant comparison (using kjson_now)
\echo 'Test 13: Instant comparison'
WITH test_data AS (
    SELECT 
        kjson_now() as time1,
        kjson_add_duration(kjson_now(), kjson_duration('PT1S')) as time2
)
SELECT 
    time1 < time2 as comparison_works,
    time1 != time2 as inequality_works,
    time1 = time1 as equality_works
FROM test_data;

-- Test 14: Pretty printing
\echo 'Test 14: Pretty printing'
SELECT 
    kjson_pretty(kjson_now()) ~ '\d{4}-\d{2}-\d{2}T' as pretty_instant,
    kjson_pretty(kjson_duration('PT1H')) = '"PT1H"' as pretty_duration;

-- Test 15: Type identification
\echo 'Test 15: Type identification'
SELECT 
    kjson_typeof(kjson_now()) = 'instant' as now_is_instant,
    kjson_typeof(kjson_duration('PT1H')) = 'duration' as duration_is_duration;

\echo 'Working temporal functionality tests completed!'

-- Report known issues
\echo 'KNOWN ISSUES:'
\echo '1. Instant parsing from ISO strings defaults to epoch time'
\echo '2. kjson_extract_instant function returns NULL'
\echo '3. Duration parsing does not handle fractional seconds'
\echo '4. Nanosecond precision may not be fully preserved in all cases'