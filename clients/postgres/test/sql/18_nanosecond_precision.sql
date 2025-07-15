-- === Testing nanosecond precision for timestamps ===

-- Test current timestamp with nanosecond precision
SELECT 'Testing nanosecond precision timestamps' as test_section;

-- Create table for timestamp testing
CREATE TABLE timestamp_precision_test (
    id SERIAL PRIMARY KEY,
    event_name TEXT,
    event_time kjson,
    created_at TIMESTAMPTZ DEFAULT NOW()
);

-- Insert test data with current timestamp
INSERT INTO timestamp_precision_test (event_name, event_time) VALUES
    ('test_event_1', kjson_build_object('timestamp', NOW())),
    ('test_event_2', kjson_build_object('timestamp', NOW() + INTERVAL '1 microsecond')),
    ('test_event_3', kjson_build_object('timestamp', NOW() + INTERVAL '2 microseconds'));

-- Test timestamp extraction with nanosecond precision
SELECT 
    event_name,
    event_time,
    kjson_extract_timestamp(event_time, 'timestamp') as extracted_timestamp,
    created_at
FROM timestamp_precision_test
ORDER BY id;

-- Test timestamp comparison
SELECT 'Testing timestamp comparison' as test_section;

-- Compare timestamps for ordering
SELECT 
    event_name,
    kjson_extract_timestamp(event_time, 'timestamp') as timestamp
FROM timestamp_precision_test
ORDER BY kjson_extract_timestamp(event_time, 'timestamp');

-- Test timestamp differences (should be very small)
WITH timestamp_data AS (
    SELECT 
        event_name,
        kjson_extract_timestamp(event_time, 'timestamp') as timestamp
    FROM timestamp_precision_test
    ORDER BY kjson_extract_timestamp(event_time, 'timestamp')
)
SELECT 
    a.event_name as event_a,
    b.event_name as event_b,
    EXTRACT(EPOCH FROM (b.timestamp - a.timestamp)) * 1000000 as microsecond_diff
FROM timestamp_data a
CROSS JOIN timestamp_data b
WHERE a.event_name < b.event_name
ORDER BY microsecond_diff;

-- Test timestamp serialization format
SELECT 'Testing timestamp serialization format' as test_section;

-- Check the string representation includes nanoseconds
SELECT 
    event_name,
    event_time::text as kjson_text,
    LENGTH(event_time::text) as text_length
FROM timestamp_precision_test;

-- Test with manually created high-precision timestamp
INSERT INTO timestamp_precision_test (event_name, event_time) VALUES
    ('high_precision', '{"timestamp": "2025-07-15T12:00:00.123456789Z"}'::kjson);

-- Verify high precision is preserved
SELECT 
    event_name,
    event_time::text as kjson_text
FROM timestamp_precision_test 
WHERE event_name = 'high_precision';

-- Test timestamp equality with nanosecond precision
SELECT 'Testing timestamp equality' as test_section;

-- Create identical timestamps
INSERT INTO timestamp_precision_test (event_name, event_time) VALUES
    ('identical_1', '{"timestamp": "2025-07-15T12:00:00.123456789Z"}'::kjson),
    ('identical_2', '{"timestamp": "2025-07-15T12:00:00.123456789Z"}'::kjson),
    ('different', '{"timestamp": "2025-07-15T12:00:00.123456788Z"}'::kjson);

-- Test equality comparisons
SELECT 
    a.event_name as event_a,
    b.event_name as event_b,
    (a.event_time = b.event_time) as timestamps_equal
FROM timestamp_precision_test a
CROSS JOIN timestamp_precision_test b
WHERE a.event_name IN ('identical_1', 'identical_2', 'different')
  AND b.event_name IN ('identical_1', 'identical_2', 'different')
  AND a.id <= b.id
ORDER BY a.event_name, b.event_name;

-- Test containment with timestamps
SELECT 'Testing timestamp containment' as test_section;

SELECT COUNT(*) as matching_timestamps
FROM timestamp_precision_test
WHERE event_time @> '{"timestamp": "2025-07-15T12:00:00.123456789Z"}'::kjson;

-- Test GIN index with timestamps (if GIN is working)
CREATE INDEX IF NOT EXISTS timestamp_gin_idx ON timestamp_precision_test USING gin (event_time kjson_gin_ops);

-- Test index usage with timestamp queries
EXPLAIN (COSTS OFF, BUFFERS OFF)
SELECT event_name FROM timestamp_precision_test 
WHERE event_time ? 'timestamp';

-- Cleanup
DROP TABLE timestamp_precision_test;