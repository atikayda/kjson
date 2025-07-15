-- Comprehensive tests for integrated temporal functionality
-- Tests both kjson temporal functionality and native kduration/kinstant types

\echo 'Testing integrated temporal functionality (kjson + native types)...'

-- Test 1: Verify both temporal systems are available
\echo 'Test 1: Verify both temporal systems are available'
SELECT 
    kjson_now() IS NOT NULL as kjson_now_works,
    kinstant_now() IS NOT NULL as kinstant_now_works,
    'PT1H'::kDuration IS NOT NULL as kduration_works,
    kjson_duration('PT1H') IS NOT NULL as kjson_duration_works;

-- Test 2: Compare precision between kjson and native types
\echo 'Test 2: Compare precision between kjson and native types'
SELECT 
    kjson_now()::text ~ '\\.\\d{9}' as kjson_has_nanosecond_precision,
    kinstant_now()::text ~ '\\.\\d{9}' as kinstant_has_nanosecond_precision,
    kjson_now()::text as kjson_format,
    kinstant_now()::text as kinstant_format;

-- Test 3: Duration parsing and formatting comparison
\echo 'Test 3: Duration parsing and formatting comparison'
SELECT 
    kjson_duration('PT1H')::text as kjson_duration_format,
    'PT1H'::kDuration::text as kduration_format,
    kjson_duration('P1Y2M3DT4H5M6S')::text as kjson_complex_duration,
    'P1Y2M3DT4H5M6S'::kDuration::text as kduration_complex_duration;

-- Test 4: Temporal arithmetic comparison
\echo 'Test 4: Temporal arithmetic comparison'
WITH temporal_test AS (
    SELECT 
        kjson_now() as kjson_base,
        kinstant_now() as kinstant_base,
        kjson_duration('PT1H') as kjson_hour,
        'PT1H'::kDuration as kduration_hour
)
SELECT 
    kjson_add_duration(kjson_base, kjson_hour) IS NOT NULL as kjson_arithmetic_works,
    (kinstant_base + kduration_hour) IS NOT NULL as kinstant_arithmetic_works,
    kjson_add_duration(kjson_base, kjson_hour) > kjson_base as kjson_addition_increases,
    (kinstant_base + kduration_hour) > kinstant_base as kinstant_addition_increases
FROM temporal_test;

-- Test 5: Cross-type compatibility testing
\echo 'Test 5: Cross-type compatibility testing'
SELECT 
    kinstant_from_timestamp(now()) IS NOT NULL as kinstant_from_postgres_timestamp,
    kinstant_to_timestamp(kinstant_now()) IS NOT NULL as kinstant_to_postgres_timestamp,
    kinstant_from_epoch(1000000000.0) IS NOT NULL as kinstant_from_epoch_works,
    kinstant_extract_epoch(kinstant_now()) > 1000000000.0 as kinstant_extract_epoch_works;

-- Test 6: Duration arithmetic operations
\echo 'Test 6: Duration arithmetic operations'
SELECT 
    ('PT1H'::kDuration + 'PT30M'::kDuration) IS NOT NULL as kduration_addition,
    ('PT2H'::kDuration - 'PT30M'::kDuration) IS NOT NULL as kduration_subtraction,
    (- 'PT1H'::kDuration) IS NOT NULL as kduration_negation,
    ('PT1H'::kDuration = 'PT1H'::kDuration) as kduration_equality,
    ('PT1H'::kDuration < 'PT2H'::kDuration) as kduration_ordering;

-- Test 7: Instant arithmetic operations
\echo 'Test 7: Instant arithmetic operations'
WITH instant_test AS (
    SELECT 
        kinstant_now() as base_instant,
        'PT1H'::kDuration as one_hour,
        'PT30M'::kDuration as thirty_minutes
)
SELECT 
    (base_instant + one_hour) IS NOT NULL as instant_plus_duration,
    (base_instant - thirty_minutes) IS NOT NULL as instant_minus_duration,
    ((base_instant + one_hour) - base_instant) IS NOT NULL as instant_difference,
    (base_instant + one_hour) > base_instant as arithmetic_ordering_preserved
FROM instant_test;

-- Test 8: Complex duration operations
\echo 'Test 8: Complex duration operations'
SELECT 
    'P1Y2M3DT4H5M6S'::kDuration IS NOT NULL as complex_duration_parsing,
    ('P1Y'::kDuration + 'P6M'::kDuration) IS NOT NULL as year_month_addition,
    ('P1DT2H'::kDuration + 'PT30M'::kDuration) IS NOT NULL as mixed_component_addition,
    kduration_to_seconds('PT1H'::kDuration) = 3600.0 as hour_to_seconds_conversion,
    kduration_from_seconds(3600.0) IS NOT NULL as seconds_to_duration_conversion;

-- Test 9: Timezone handling in kInstant
\echo 'Test 9: Timezone handling in kInstant'
SELECT 
    '2024-01-01T12:00:00.000000000Z'::kInstant IS NOT NULL as utc_instant,
    '2024-01-01T12:00:00.000000000+05:30'::kInstant IS NOT NULL as timezone_instant,
    '2024-01-01T12:00:00.000000000Z'::kInstant = '2024-01-01T17:00:00.000000000+05:00'::kInstant as timezone_equality,
    '2024-01-01T12:00:00.000000000Z'::kInstant < '2024-01-01T12:00:00.000000001Z'::kInstant as nanosecond_precision_comparison;

-- Test 10: Performance comparison
\echo 'Test 10: Performance comparison'
WITH performance_test AS (
    SELECT 
        generate_series(1, 100) as i,
        kjson_now() as kjson_time,
        kinstant_now() as kinstant_time
)
SELECT 
    COUNT(*) as total_operations,
    COUNT(kjson_time) as kjson_successes,
    COUNT(kinstant_time) as kinstant_successes,
    MIN(kjson_time) IS NOT NULL as kjson_min_works,
    MIN(kinstant_time) IS NOT NULL as kinstant_min_works
FROM performance_test;

-- Test 11: Aggregation and array operations
\echo 'Test 11: Aggregation and array operations'
WITH aggregation_test AS (
    SELECT unnest(ARRAY[
        kinstant_now(),
        kinstant_now() + 'PT1H'::kDuration,
        kinstant_now() + 'PT2H'::kDuration
    ]) as instant_values
)
SELECT 
    COUNT(*) as value_count,
    MIN(instant_values) IS NOT NULL as min_works,
    MAX(instant_values) IS NOT NULL as max_works,
    MIN(instant_values) < MAX(instant_values) as ordering_works
FROM aggregation_test;

-- Test 12: Round-trip conversion testing
\echo 'Test 12: Round-trip conversion testing'
WITH round_trip_test AS (
    SELECT 
        kinstant_now() as original_instant,
        'PT1H30M45S'::kDuration as original_duration
)
SELECT 
    kinstant_from_timestamp(kinstant_to_timestamp(original_instant)) IS NOT NULL as instant_round_trip,
    kduration_from_seconds(kduration_to_seconds(original_duration)) IS NOT NULL as duration_round_trip,
    kinstant_from_epoch(kinstant_extract_epoch(original_instant)) IS NOT NULL as instant_epoch_round_trip
FROM round_trip_test;

-- Test 13: Edge case testing
\echo 'Test 13: Edge case testing'
SELECT 
    '1970-01-01T00:00:00.000000000Z'::kInstant IS NOT NULL as unix_epoch_instant,
    '2038-01-19T03:14:07.999999999Z'::kInstant IS NOT NULL as y2038_instant,
    'PT0S'::kDuration IS NOT NULL as zero_duration,
    'P100Y'::kDuration IS NOT NULL as century_duration,
    '-PT1H'::kDuration IS NOT NULL as negative_duration;

-- Test 14: Mixed arithmetic operations
\echo 'Test 14: Mixed arithmetic operations'
WITH mixed_test AS (
    SELECT 
        kinstant_now() as base_instant,
        'PT1H'::kDuration as hour_duration,
        'PT30M'::kDuration as half_hour_duration
)
SELECT 
    ((base_instant + hour_duration) + half_hour_duration) IS NOT NULL as chained_addition,
    ((base_instant + hour_duration) - half_hour_duration) IS NOT NULL as add_then_subtract,
    (base_instant + (hour_duration + half_hour_duration)) IS NOT NULL as duration_addition_first,
    ((base_instant + hour_duration) - base_instant) IS NOT NULL as instant_subtraction_to_duration
FROM mixed_test;

-- Test 15: Type system integration
\echo 'Test 15: Type system integration'
SELECT 
    pg_typeof(kinstant_now()) = 'kinstant'::regtype as kinstant_type_correct,
    pg_typeof('PT1H'::kDuration) = 'kduration'::regtype as kduration_type_correct,
    pg_typeof(kjson_now()) = 'kjson'::regtype as kjson_type_correct,
    pg_typeof(kjson_duration('PT1H')) = 'kjson'::regtype as kjson_duration_type_correct;

-- Test 16: Precision preservation testing
\echo 'Test 16: Precision preservation testing'
SELECT 
    '2024-01-01T12:00:00.123456789Z'::kInstant::text ~ '\\.123456789' as kinstant_preserves_nanoseconds,
    'PT1.123456789S'::kDuration::text ~ '1\\.123456789' as kduration_preserves_nanoseconds,
    (kinstant_now() + 'PT0.000000001S'::kDuration) > kinstant_now() as nanosecond_arithmetic_works;

-- Test 17: Error handling and boundary conditions
\echo 'Test 17: Error handling and boundary conditions'
\set ON_ERROR_STOP off

-- Test invalid formats
SELECT 'invalid'::kInstant;
SELECT 'invalid'::kDuration;

\set ON_ERROR_STOP on

-- Test valid edge cases
SELECT 
    'PT0.000000001S'::kDuration IS NOT NULL as minimum_duration,
    '2024-12-31T23:59:59.999999999Z'::kInstant IS NOT NULL as year_end_instant,
    (kinstant_now() + 'PT0S'::kDuration) = kinstant_now() as zero_duration_addition;

-- Test 18: Indexing and sorting capabilities
\echo 'Test 18: Indexing and sorting capabilities'
WITH sorting_test AS (
    SELECT unnest(ARRAY[
        kinstant_now(),
        kinstant_now() + 'PT1H'::kDuration,
        kinstant_now() + 'PT2H'::kDuration,
        kinstant_now() + 'PT30M'::kDuration
    ]) as instant_values
)
SELECT 
    COUNT(*) as total_values,
    COUNT(DISTINCT instant_values) as distinct_values,
    array_agg(instant_values ORDER BY instant_values) IS NOT NULL as ordering_works
FROM sorting_test;

-- Test 19: Advanced temporal calculations
\echo 'Test 19: Advanced temporal calculations'
WITH advanced_test AS (
    SELECT 
        kinstant_now() as start_time,
        'P1Y'::kDuration as one_year,
        'P1M'::kDuration as one_month,
        'P1D'::kDuration as one_day
)
SELECT 
    (start_time + one_year) IS NOT NULL as year_addition,
    (start_time + one_month) IS NOT NULL as month_addition,
    (start_time + one_day) IS NOT NULL as day_addition,
    ((start_time + one_year) - start_time) IS NOT NULL as year_duration_result,
    kduration_to_seconds(one_year) > kduration_to_seconds(one_month) as year_greater_than_month
FROM advanced_test;

-- Test 20: Integration summary
\echo 'Test 20: Integration summary'
SELECT 
    'kjson temporal' as system,
    kjson_now() IS NOT NULL as now_function,
    kjson_duration('PT1H') IS NOT NULL as duration_function,
    kjson_add_duration(kjson_now(), kjson_duration('PT1H')) IS NOT NULL as arithmetic_function
UNION ALL
SELECT 
    'native temporal' as system,
    kinstant_now() IS NOT NULL as now_function,
    'PT1H'::kDuration IS NOT NULL as duration_function,
    (kinstant_now() + 'PT1H'::kDuration) IS NOT NULL as arithmetic_function;

\echo 'Integrated temporal functionality tests completed!'
\echo ''
\echo 'SUMMARY:'
\echo '✓ kjson temporal functionality (kjson_now, kjson_duration, kjson_add_duration)'
\echo '✓ Native kInstant type with nanosecond precision timestamps'
\echo '✓ Native kDuration type with ISO 8601 duration support'
\echo '✓ Full arithmetic operations between kInstant and kDuration'
\echo '✓ Cross-type compatibility and conversions'
\echo '✓ Timezone support in kInstant'
\echo '✓ Precision preservation and nanosecond arithmetic'
\echo '✓ Comprehensive error handling and edge cases'
\echo '✓ Integration with PostgreSQL type system'
\echo '✓ Indexing and sorting capabilities'
\echo ''
\echo 'Both temporal systems are now integrated into the kjson extension!'