-- Comprehensive tests for kInstant and kDuration cross-type operations
-- Tests arithmetic operations between kInstant and kDuration types

\echo 'Testing kInstant and kDuration cross-type operations...'

-- Test 1: kInstant + kDuration arithmetic
\echo 'Test 1: kInstant + kDuration arithmetic'
SELECT 
    ('2024-01-01T12:00:00.000000000Z'::kInstant + 'PT1H'::kDuration) IS NOT NULL as instant_plus_duration,
    ('2024-01-01T12:00:00.000000000Z'::kInstant + 'PT1H'::kDuration)::text ~ '2024-01-01T13:00:00' as hour_addition_result,
    ('2024-01-01T12:00:00.000000000Z'::kInstant + 'PT30M'::kDuration)::text ~ '2024-01-01T12:30:00' as minute_addition_result,
    ('2024-01-01T12:00:00.000000000Z'::kInstant + 'PT45S'::kDuration)::text ~ '2024-01-01T12:00:45' as second_addition_result;

-- Test 2: kInstant - kDuration arithmetic
\echo 'Test 2: kInstant - kDuration arithmetic'
SELECT 
    ('2024-01-01T12:00:00.000000000Z'::kInstant - 'PT1H'::kDuration) IS NOT NULL as instant_minus_duration,
    ('2024-01-01T12:00:00.000000000Z'::kInstant - 'PT1H'::kDuration)::text ~ '2024-01-01T11:00:00' as hour_subtraction_result,
    ('2024-01-01T12:00:00.000000000Z'::kInstant - 'PT30M'::kDuration)::text ~ '2024-01-01T11:30:00' as minute_subtraction_result,
    ('2024-01-01T12:00:00.000000000Z'::kInstant - 'PT45S'::kDuration)::text ~ '2024-01-01T11:59:15' as second_subtraction_result;

-- Test 3: kInstant - kInstant to get kDuration
\echo 'Test 3: kInstant - kInstant to get kDuration'
SELECT 
    ('2024-01-01T13:00:00.000000000Z'::kInstant - '2024-01-01T12:00:00.000000000Z'::kInstant) IS NOT NULL as instant_minus_instant,
    ('2024-01-01T13:00:00.000000000Z'::kInstant - '2024-01-01T12:00:00.000000000Z'::kInstant)::text ~ 'PT' as duration_result_format,
    ('2024-01-01T12:00:00.000000000Z'::kInstant - '2024-01-01T13:00:00.000000000Z'::kInstant)::text ~ '-PT' as negative_duration_result;

-- Test 4: Complex duration addition
\echo 'Test 4: Complex duration addition'
SELECT 
    ('2024-01-01T12:00:00.000000000Z'::kInstant + 'P1Y2M3DT4H5M6S'::kDuration) IS NOT NULL as complex_duration_addition,
    ('2024-01-01T12:00:00.000000000Z'::kInstant + 'P1DT2H30M'::kDuration) IS NOT NULL as day_hour_minute_addition,
    ('2024-01-01T12:00:00.000000000Z'::kInstant + 'P1Y6M'::kDuration) IS NOT NULL as year_month_addition;

-- Test 5: Negative duration arithmetic
\echo 'Test 5: Negative duration arithmetic'
SELECT 
    ('2024-01-01T12:00:00.000000000Z'::kInstant + '-PT1H'::kDuration) IS NOT NULL as negative_duration_addition,
    ('2024-01-01T12:00:00.000000000Z'::kInstant + '-PT1H'::kDuration)::text ~ '2024-01-01T11:00:00' as negative_hour_result,
    ('2024-01-01T12:00:00.000000000Z'::kInstant - '-PT1H'::kDuration)::text ~ '2024-01-01T13:00:00' as minus_negative_hour;

-- Test 6: Nanosecond precision arithmetic
\echo 'Test 6: Nanosecond precision arithmetic'
SELECT 
    ('2024-01-01T12:00:00.000000000Z'::kInstant + 'PT0.000000001S'::kDuration) IS NOT NULL as nanosecond_addition,
    ('2024-01-01T12:00:00.000000000Z'::kInstant + 'PT0.000000001S'::kDuration) > '2024-01-01T12:00:00.000000000Z'::kInstant as nanosecond_greater,
    ('2024-01-01T12:00:00.500000000Z'::kInstant + 'PT0.500000000S'::kDuration)::text ~ '2024-01-01T12:00:01' as half_second_addition;

-- Test 7: Cross-type function calls
\echo 'Test 7: Cross-type function calls'
SELECT 
    kinstant_add_duration('2024-01-01T12:00:00.000000000Z'::kInstant, 'PT1H'::kDuration) IS NOT NULL as add_function,
    kinstant_subtract_duration('2024-01-01T12:00:00.000000000Z'::kInstant, 'PT1H'::kDuration) IS NOT NULL as subtract_function,
    kinstant_subtract_instant('2024-01-01T13:00:00.000000000Z'::kInstant, '2024-01-01T12:00:00.000000000Z'::kInstant) IS NOT NULL as subtract_instant_function;

-- Test 8: Chained operations
\echo 'Test 8: Chained operations'
SELECT 
    (('2024-01-01T12:00:00.000000000Z'::kInstant + 'PT1H'::kDuration) + 'PT30M'::kDuration) IS NOT NULL as chained_additions,
    (('2024-01-01T12:00:00.000000000Z'::kInstant + 'PT2H'::kDuration) - 'PT1H'::kDuration) IS NOT NULL as addition_then_subtraction,
    (('2024-01-01T12:00:00.000000000Z'::kInstant + 'PT1H'::kDuration) - '2024-01-01T12:00:00.000000000Z'::kInstant) IS NOT NULL as add_then_subtract_instant;

-- Test 9: Timezone preservation in arithmetic
\echo 'Test 9: Timezone preservation in arithmetic'
SELECT 
    ('2024-01-01T12:00:00.000000000+05:30'::kInstant + 'PT1H'::kDuration) IS NOT NULL as timezone_preserved_addition,
    ('2024-01-01T12:00:00.000000000+05:30'::kInstant + 'PT1H'::kDuration)::text ~ '\\+05:30' as timezone_in_result,
    ('2024-01-01T12:00:00.000000000-08:00'::kInstant + 'PT1H'::kDuration)::text ~ '-08:00' as negative_timezone_preserved;

-- Test 10: Edge cases with large durations
\echo 'Test 10: Edge cases with large durations'
SELECT 
    ('2024-01-01T12:00:00.000000000Z'::kInstant + 'P1Y'::kDuration) IS NOT NULL as add_one_year,
    ('2024-01-01T12:00:00.000000000Z'::kInstant + 'P100Y'::kDuration) IS NOT NULL as add_hundred_years,
    ('2024-01-01T12:00:00.000000000Z'::kInstant + 'P1000D'::kDuration) IS NOT NULL as add_thousand_days;

-- Test 11: Duration conversion in context of instant arithmetic
\echo 'Test 11: Duration conversion in context of instant arithmetic'
WITH duration_test AS (
    SELECT 
        '2024-01-01T12:00:00.000000000Z'::kInstant as base_instant,
        'PT3600S'::kDuration as hour_in_seconds,
        'PT1H'::kDuration as hour_in_hours
)
SELECT 
    (base_instant + hour_in_seconds) IS NOT NULL as seconds_duration_works,
    (base_instant + hour_in_hours) IS NOT NULL as hours_duration_works,
    (base_instant + hour_in_seconds) = (base_instant + hour_in_hours) as equivalent_durations
FROM duration_test;

-- Test 12: Arithmetic with zero duration
\echo 'Test 12: Arithmetic with zero duration'
SELECT 
    ('2024-01-01T12:00:00.000000000Z'::kInstant + 'PT0S'::kDuration) = '2024-01-01T12:00:00.000000000Z'::kInstant as zero_duration_addition,
    ('2024-01-01T12:00:00.000000000Z'::kInstant - 'PT0S'::kDuration) = '2024-01-01T12:00:00.000000000Z'::kInstant as zero_duration_subtraction;

-- Test 13: Arithmetic ordering preservation
\echo 'Test 13: Arithmetic ordering preservation'
WITH time_test AS (
    SELECT 
        '2024-01-01T12:00:00.000000000Z'::kInstant as time1,
        '2024-01-01T13:00:00.000000000Z'::kInstant as time2,
        'PT30M'::kDuration as thirty_minutes
)
SELECT 
    (time1 + thirty_minutes) < (time2 + thirty_minutes) as ordering_preserved_addition,
    (time1 - thirty_minutes) < (time2 - thirty_minutes) as ordering_preserved_subtraction
FROM time_test;

-- Test 14: Leap year and month arithmetic
\echo 'Test 14: Leap year and month arithmetic'
SELECT 
    ('2024-01-01T12:00:00.000000000Z'::kInstant + 'P1Y'::kDuration) IS NOT NULL as leap_year_addition,
    ('2024-01-31T12:00:00.000000000Z'::kInstant + 'P1M'::kDuration) IS NOT NULL as month_end_addition,
    ('2024-02-29T12:00:00.000000000Z'::kInstant + 'P1D'::kDuration) IS NOT NULL as leap_day_addition;

-- Test 15: Performance with multiple operations
\echo 'Test 15: Performance with multiple operations'
WITH performance_test AS (
    SELECT 
        generate_series(1, 10) as i,
        kinstant_now() as base_time,
        ('PT' || generate_series(1, 10) || 'M')::kDuration as var_duration
)
SELECT 
    COUNT(*) = 10 as generated_ten,
    COUNT(base_time + var_duration) = 10 as all_additions_work,
    MIN(base_time + var_duration) IS NOT NULL as min_result_not_null,
    MAX(base_time + var_duration) IS NOT NULL as max_result_not_null
FROM performance_test;

-- Test 16: Boundary conditions
\echo 'Test 16: Boundary conditions'
SELECT 
    ('1970-01-01T00:00:00.000000000Z'::kInstant + 'PT1S'::kDuration) > '1970-01-01T00:00:00.000000000Z'::kInstant as epoch_plus_second,
    ('1970-01-01T00:00:01.000000000Z'::kInstant - 'PT1S'::kDuration) = '1970-01-01T00:00:00.000000000Z'::kInstant as back_to_epoch,
    ('2038-01-19T03:14:07.000000000Z'::kInstant + 'PT1S'::kDuration) IS NOT NULL as y2038_boundary;

-- Test 17: Fractional seconds in duration arithmetic
\echo 'Test 17: Fractional seconds in duration arithmetic'
SELECT 
    ('2024-01-01T12:00:00.000000000Z'::kInstant + 'PT1.5S'::kDuration) IS NOT NULL as fractional_second_addition,
    ('2024-01-01T12:00:00.000000000Z'::kInstant + 'PT0.123456789S'::kDuration) IS NOT NULL as nanosecond_fractional_addition,
    ('2024-01-01T12:00:00.000000000Z'::kInstant + 'PT1.5S'::kDuration)::text ~ '2024-01-01T12:00:01' as fractional_result_check;

-- Test 18: Duration aggregation with instant arithmetic
\echo 'Test 18: Duration aggregation with instant arithmetic'
WITH duration_aggregate AS (
    SELECT 
        '2024-01-01T12:00:00.000000000Z'::kInstant as base_instant,
        unnest(ARRAY['PT1H'::kDuration, 'PT30M'::kDuration, 'PT15M'::kDuration]) as dur
)
SELECT 
    COUNT(*) = 3 as three_durations,
    COUNT(base_instant + dur) = 3 as all_additions_successful,
    MIN(base_instant + dur) IS NOT NULL as min_addition_result,
    MAX(base_instant + dur) IS NOT NULL as max_addition_result
FROM duration_aggregate;

-- Test 19: Cross-type comparison after arithmetic
\echo 'Test 19: Cross-type comparison after arithmetic'
SELECT 
    ('2024-01-01T12:00:00.000000000Z'::kInstant + 'PT1H'::kDuration) > '2024-01-01T12:00:00.000000000Z'::kInstant as addition_greater,
    ('2024-01-01T12:00:00.000000000Z'::kInstant - 'PT1H'::kDuration) < '2024-01-01T12:00:00.000000000Z'::kInstant as subtraction_less,
    ('2024-01-01T13:00:00.000000000Z'::kInstant - '2024-01-01T12:00:00.000000000Z'::kInstant) > 'PT0S'::kDuration as positive_duration_result;

-- Test 20: Mixed precision arithmetic
\echo 'Test 20: Mixed precision arithmetic'
SELECT 
    ('2024-01-01T12:00:00.123456789Z'::kInstant + 'PT0.876543211S'::kDuration) IS NOT NULL as mixed_precision_addition,
    ('2024-01-01T12:00:00.999999999Z'::kInstant + 'PT0.000000001S'::kDuration) IS NOT NULL as precision_boundary_addition,
    ('2024-01-01T12:00:00.999999999Z'::kInstant + 'PT0.000000001S'::kDuration)::text ~ '2024-01-01T12:00:01' as precision_rollover;

\echo 'kInstant and kDuration cross-type operations tests completed!'