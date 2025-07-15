-- Test instant terminology and temporal functions
-- This tests the renamed temporal functions using "instant" instead of "timestamp"

-- Test kjson_now() function
SELECT 'kjson_now test'::text as test_name;
SELECT kjson_typeof(kjson_now()) = 'date' as is_date_type;

-- Test basic instant creation and extraction
SELECT 'instant extraction test'::text as test_name;
SELECT kjson_extract_instant('{"instant": "2024-01-15T10:30:45.123456789Z"}'::kjson, 'instant') IS NOT NULL as has_instant;

-- Test duration creation
SELECT 'duration creation test'::text as test_name;
SELECT kjson_typeof(kjson_duration('PT1H30M')) = 'duration' as is_duration_type;

-- Test duration addition
SELECT 'duration addition test'::text as test_name;
WITH test_data AS (
    SELECT 
        kjson_now() as base_instant,
        kjson_duration('PT1H') as one_hour
)
SELECT kjson_typeof(kjson_add_duration(base_instant, one_hour)) = 'date' as is_date_type
FROM test_data;

-- Test complex ISO duration
SELECT 'complex duration test'::text as test_name;
SELECT kjson_duration('P1Y2M3DT4H5M6.123456789S') IS NOT NULL as duration_created;

-- Test negative duration
SELECT 'negative duration test'::text as test_name;
SELECT kjson_duration('-PT30M') IS NOT NULL as negative_duration_created;

-- Test duration with instant arithmetic
SELECT 'instant arithmetic test'::text as test_name;
WITH test_data AS (
    SELECT 
        '{"timestamp": "2024-01-01T12:00:00.000000000Z"}'::kjson as test_instant,
        kjson_duration('PT6H') as six_hours
)
SELECT kjson_add_duration(test_instant->'timestamp', six_hours) IS NOT NULL as arithmetic_works
FROM test_data;

-- Test that old function name is renamed
SELECT 'function rename test'::text as test_name;
-- This should work (new name)
SELECT kjson_extract_instant('{"time": "2024-01-01T00:00:00Z"}'::kjson, 'time') IS NOT NULL as new_name_works;

-- Test nanosecond precision preservation
SELECT 'nanosecond precision test'::text as test_name;
WITH test_data AS (
    SELECT '{"precise": "2024-01-01T12:00:00.123456789Z"}'::kjson as test_json
)
SELECT test_json->>'precise' LIKE '%.123456789Z' as preserves_nanoseconds
FROM test_data;

-- Test pretty printing of instants and durations
SELECT 'pretty print test'::text as test_name;
SELECT kjson_pretty(kjson_now(), 2) LIKE '%-%-%T%:%:%.%Z%' as pretty_instant_format;

SELECT kjson_pretty(kjson_duration('P1DT2H3M4.567S'), 2) LIKE '%P1DT2H3M4.567%' as pretty_duration_format;