-- Debug temporal functionality step by step
\echo 'Debug temporal functionality...'

-- Test 1: Basic instant parsing
\echo 'Test 1: Basic instant parsing'
SELECT 
    '"2024-01-15T10:30:45.123456789Z"'::kjson as parsed_instant,
    kjson_typeof('"2024-01-15T10:30:45.123456789Z"'::kjson) as parsed_type;

-- Test 2: Object with instant
\echo 'Test 2: Object with instant'
SELECT 
    '{"time": "2024-01-15T10:30:45.123456789Z"}'::kjson as object_with_instant,
    kjson_typeof('{"time": "2024-01-15T10:30:45.123456789Z"}'::kjson->'time') as instant_type;

-- Test 3: Manual instant extraction
\echo 'Test 3: Manual instant extraction'
SELECT 
    '{"time": "2024-01-15T10:30:45.123456789Z"}'::kjson->'time' as extracted_instant;

-- Test 4: Compare with kjson_now
\echo 'Test 4: Compare with kjson_now'
SELECT 
    kjson_now() as current_time,
    '{"time": "2024-01-15T10:30:45.123456789Z"}'::kjson->'time' as parsed_time,
    kjson_now() > '{"time": "2024-01-15T10:30:45.123456789Z"}'::kjson->'time' as comparison;

-- Test 5: Simple duration parsing
\echo 'Test 5: Simple duration parsing'
SELECT 
    kjson_duration('PT1H') as one_hour,
    kjson_duration('PT30M') as thirty_minutes,
    kjson_duration('PT1S') as one_second;

-- Test 6: Duration with fractional seconds
\echo 'Test 6: Duration with fractional seconds'
SELECT 
    kjson_duration('PT1.5S') as fractional_seconds;

-- Test 7: Test temporal arithmetic with simple values
\echo 'Test 7: Simple temporal arithmetic'
SELECT 
    kjson_add_duration(kjson_now(), kjson_duration('PT1H')) as add_hour,
    kjson_add_duration(kjson_now(), kjson_duration('PT1H')) > kjson_now() as future_time;

-- Test 8: Test instant extraction function directly
\echo 'Test 8: Direct instant extraction'
SELECT 
    kjson_extract_instant('{"test": "2024-01-01T12:00:00.000000000Z"}'::kjson, 'test') as extracted;

-- Test 9: Test with millisecond precision
\echo 'Test 9: Test with millisecond precision'
SELECT 
    '{"time": "2024-01-15T10:30:45.123Z"}'::kjson->'time' as millisecond_precision,
    kjson_typeof('{"time": "2024-01-15T10:30:45.123Z"}'::kjson->'time') as type;

-- Test 10: Test string vs instant parsing
\echo 'Test 10: String vs instant parsing'
SELECT 
    kjson_typeof('"2024-01-15T10:30:45.123456789Z"'::kjson) as quoted_type,
    kjson_typeof('2024-01-15T10:30:45.123456789Z'::kjson) as unquoted_type;

\echo 'Debug tests completed!'