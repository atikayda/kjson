-- Comprehensive tests for kDuration native PostgreSQL type
-- Tests kDuration type with ISO 8601 duration parsing and formatting

\echo 'Testing kDuration native PostgreSQL type...'

-- Test 1: Basic kDuration input/output
\echo 'Test 1: Basic kDuration input/output'
SELECT 
    'PT1H'::kDuration::text = 'PT1H' as basic_hour,
    'PT30M'::kDuration::text = 'PT30M' as basic_minutes,
    'PT45S'::kDuration::text = 'PT45S' as basic_seconds,
    'P1D'::kDuration::text = 'P1D' as basic_day,
    'P1Y'::kDuration::text = 'P1Y' as basic_year;

-- Test 2: Complex duration formats
\echo 'Test 2: Complex duration formats'
SELECT 
    'P1Y2M3DT4H5M6S'::kDuration::text = 'P1Y2M3DT4H5M6S' as complex_full,
    'P1DT1H30M'::kDuration::text = 'P1DT1H30M' as day_hour_minute,
    'P1Y6M'::kDuration::text = 'P1Y6M' as year_month_only,
    'PT2H30M45S'::kDuration::text = 'PT2H30M45S' as time_only;

-- Test 3: Negative durations
\echo 'Test 3: Negative durations'
SELECT 
    '-PT1H'::kDuration::text = '-PT1H' as negative_hour,
    '-P1D'::kDuration::text = '-P1D' as negative_day,
    '-P1Y2M3DT4H5M6S'::kDuration::text = '-P1Y2M3DT4H5M6S' as negative_complex;

-- Test 4: Zero duration
\echo 'Test 4: Zero duration'
SELECT 
    'PT0S'::kDuration::text = 'PT0S' as zero_seconds,
    'P0D'::kDuration::text = 'P0D' as zero_days,
    'P0Y'::kDuration::text = 'P0Y' as zero_years;

-- Test 5: Duration with nanoseconds (fractional seconds)
\echo 'Test 5: Duration with fractional seconds'
SELECT 
    'PT1.5S'::kDuration::text ~ 'PT1\\.5.*S' as fractional_seconds,
    'PT0.123456789S'::kDuration::text ~ 'PT0\\.123456789S' as nanosecond_precision,
    'PT30.75S'::kDuration::text ~ 'PT30\\.75.*S' as longer_fractional;

-- Test 6: Duration equality comparisons
\echo 'Test 6: Duration equality comparisons'
SELECT 
    'PT1H'::kDuration = 'PT1H'::kDuration as equal_hours,
    'PT60M'::kDuration = 'PT60M'::kDuration as equal_minutes,
    'PT1H'::kDuration != 'PT30M'::kDuration as not_equal_different,
    'PT1H'::kDuration <> 'PT2H'::kDuration as not_equal_operator;

-- Test 7: Duration ordering and comparisons
\echo 'Test 7: Duration ordering and comparisons'
SELECT 
    'PT30M'::kDuration < 'PT1H'::kDuration as thirty_min_less_than_hour,
    'PT1H'::kDuration > 'PT30M'::kDuration as hour_greater_than_thirty_min,
    'PT1H'::kDuration <= 'PT1H'::kDuration as equal_less_or_equal,
    'PT2H'::kDuration >= 'PT1H'::kDuration as two_hours_greater_equal;

-- Test 8: Duration arithmetic - addition
\echo 'Test 8: Duration arithmetic - addition'
SELECT 
    ('PT1H'::kDuration + 'PT30M'::kDuration) IS NOT NULL as addition_works,
    ('PT1H'::kDuration + 'PT30M'::kDuration)::text ~ 'PT1H30M|PT90M' as addition_result_format,
    ('P1D'::kDuration + 'PT12H'::kDuration) IS NOT NULL as day_plus_hours,
    ('P1Y'::kDuration + 'P6M'::kDuration) IS NOT NULL as year_plus_months;

-- Test 9: Duration arithmetic - subtraction
\echo 'Test 9: Duration arithmetic - subtraction'
SELECT 
    ('PT2H'::kDuration - 'PT1H'::kDuration) IS NOT NULL as subtraction_works,
    ('PT2H'::kDuration - 'PT30M'::kDuration) IS NOT NULL as hour_minus_minutes,
    ('P1D'::kDuration - 'PT6H'::kDuration) IS NOT NULL as day_minus_hours;

-- Test 10: Duration negation
\echo 'Test 10: Duration negation'
SELECT 
    (- 'PT1H'::kDuration) IS NOT NULL as negation_works,
    (- 'PT1H'::kDuration)::text ~ '^-PT1H$' as negation_format,
    (- 'P1D'::kDuration)::text ~ '^-P1D$' as negation_day,
    (- (- 'PT1H'::kDuration)) IS NOT NULL as double_negation;

-- Test 11: Duration conversion to seconds
\echo 'Test 11: Duration conversion to seconds'
SELECT 
    kduration_to_seconds('PT1H'::kDuration) = 3600.0 as hour_to_seconds,
    kduration_to_seconds('PT30M'::kDuration) = 1800.0 as thirty_min_to_seconds,
    kduration_to_seconds('PT1.5S'::kDuration) = 1.5 as fractional_to_seconds,
    kduration_to_seconds('P1D'::kDuration) = 86400.0 as day_to_seconds;

-- Test 12: Duration creation from seconds
\echo 'Test 12: Duration creation from seconds'
SELECT 
    kduration_from_seconds(3600.0) IS NOT NULL as from_hour_seconds,
    kduration_from_seconds(1800.0) IS NOT NULL as from_thirty_min_seconds,
    kduration_from_seconds(1.5) IS NOT NULL as from_fractional_seconds,
    kduration_from_seconds(-3600.0) IS NOT NULL as from_negative_seconds;

-- Test 13: Duration round-trip conversion
\echo 'Test 13: Duration round-trip conversion'
WITH test_durations AS (
    SELECT 
        'PT1H'::kDuration as hour_duration,
        'PT30M'::kDuration as thirty_min_duration,
        'PT1.5S'::kDuration as fractional_duration
)
SELECT 
    kduration_from_seconds(kduration_to_seconds(hour_duration)) IS NOT NULL as hour_round_trip,
    kduration_from_seconds(kduration_to_seconds(thirty_min_duration)) IS NOT NULL as thirty_min_round_trip,
    kduration_from_seconds(kduration_to_seconds(fractional_duration)) IS NOT NULL as fractional_round_trip
FROM test_durations;

-- Test 14: Duration edge cases
\echo 'Test 14: Duration edge cases'
SELECT 
    'P0Y0M0DT0H0M0S'::kDuration IS NOT NULL as zero_full_format,
    'P1W'::kDuration IS NOT NULL as week_format,
    'P1000Y'::kDuration IS NOT NULL as large_year,
    'PT999999999S'::kDuration IS NOT NULL as large_seconds;

-- Test 15: Duration with various time components
\echo 'Test 15: Duration with various time components'
SELECT 
    'P1Y2M3DT4H5M6.123456789S'::kDuration IS NOT NULL as full_precision,
    'P1Y2M3DT4H5M6.123456789S'::kDuration::text ~ 'P1Y2M3DT4H5M6\\.123456789S' as full_precision_format,
    'P365D'::kDuration IS NOT NULL as year_as_days,
    'PT8760H'::kDuration IS NOT NULL as year_as_hours;

-- Test 16: Duration comparison with different units
\echo 'Test 16: Duration comparison with different units'
SELECT 
    'P1D'::kDuration = 'PT24H'::kDuration as day_equals_24_hours,
    'PT1H'::kDuration = 'PT60M'::kDuration as hour_equals_60_minutes,
    'PT1M'::kDuration = 'PT60S'::kDuration as minute_equals_60_seconds;

-- Test 17: Duration aggregation and array operations
\echo 'Test 17: Duration aggregation and array operations'
WITH duration_data AS (
    SELECT unnest(ARRAY['PT1H'::kDuration, 'PT30M'::kDuration, 'PT45M'::kDuration]) as dur
)
SELECT 
    COUNT(*) = 3 as has_three_durations,
    MIN(dur) IS NOT NULL as min_duration_works,
    MAX(dur) IS NOT NULL as max_duration_works,
    MIN(dur) < MAX(dur) as min_less_than_max
FROM duration_data;

-- Test 18: Duration with large values
\echo 'Test 18: Duration with large values'
SELECT 
    'P100Y'::kDuration IS NOT NULL as hundred_years,
    'P10000D'::kDuration IS NOT NULL as ten_thousand_days,
    'PT100000H'::kDuration IS NOT NULL as hundred_thousand_hours,
    'PT999999999.999999999S'::kDuration IS NOT NULL as large_precise_seconds;

-- Test 19: Duration error handling
\echo 'Test 19: Duration error handling'
\set ON_ERROR_STOP off

-- Test invalid duration formats
SELECT 'invalid_duration'::kDuration;
SELECT 'P'::kDuration;
SELECT 'PT'::kDuration;
SELECT 'P1X'::kDuration;
SELECT 'PT1Z'::kDuration;

\set ON_ERROR_STOP on

-- Test 20: Duration format validation
\echo 'Test 20: Duration format validation'
SELECT 
    'PT1H'::kDuration::text ~ '^PT1H$' as hour_exact_format,
    'PT30M'::kDuration::text ~ '^PT30M$' as minute_exact_format,
    'PT45S'::kDuration::text ~ '^PT45S$' as second_exact_format,
    'P1D'::kDuration::text ~ '^P1D$' as day_exact_format,
    'P1Y'::kDuration::text ~ '^P1Y$' as year_exact_format;

\echo 'kDuration comprehensive tests completed!'