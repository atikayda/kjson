-- Test utility functions
\echo '=== Testing Utility Functions ==='

-- Test kjson_pretty
SELECT kjson_pretty('{"name": "Alice", "age": 30}'::kjson, 2);
SELECT kjson_pretty('[1, 2, {"a": "b"}]'::kjson, 4);
SELECT kjson_pretty('{
    id: 123n,
    price: 9.99m,
    tags: ["new", "sale"]
}'::kjson, 2);

-- Test kjson_typeof
SELECT kjson_typeof('null'::kjson);
SELECT kjson_typeof('true'::kjson);
SELECT kjson_typeof('42'::kjson);
SELECT kjson_typeof('123n'::kjson);
SELECT kjson_typeof('9.99m'::kjson);
SELECT kjson_typeof('"text"'::kjson);
SELECT kjson_typeof('550e8400-e29b-41d4-a716-446655440000'::kjson);
SELECT kjson_typeof('2025-01-10T12:00:00Z'::kjson);
SELECT kjson_typeof('[]'::kjson);
SELECT kjson_typeof('{}'::kjson);

-- Test extracting specific field types
SELECT kjson_typeof('{"num": 42}'::kjson->'num');
SELECT kjson_typeof('{"big": 999n}'::kjson->'big');

-- Test extraction functions with complex data
CREATE TEMP TABLE test_extract (
    id serial PRIMARY KEY,
    data kjson
);

INSERT INTO test_extract (data) VALUES
('{
    "transaction_id": 550e8400-e29b-41d4-a716-446655440000,
    "amount": 123456789012345678901234567890n,
    "price": 99.99m,
    "timestamp": 2025-01-10T12:00:00Z,
    "metadata": {
        "user_id": 650e8400-e29b-41d4-a716-446655440001,
        "fee": 2.50m
    }
}'::kjson);

-- Test kjson_extract_uuid
SELECT kjson_extract_uuid(data, 'transaction_id') FROM test_extract;
SELECT kjson_extract_uuid(data, 'metadata', 'user_id') FROM test_extract;

-- Test kjson_extract_numeric
SELECT kjson_extract_numeric(data, 'amount') FROM test_extract;
SELECT kjson_extract_numeric(data, 'price') FROM test_extract;
SELECT kjson_extract_numeric(data, 'metadata', 'fee') FROM test_extract;

-- Test kjson_extract_timestamp
SELECT kjson_extract_timestamp(data, 'timestamp') FROM test_extract;

-- Test with NULL paths
SELECT kjson_extract_uuid(data, 'nonexistent') FROM test_extract;
SELECT kjson_extract_numeric(data, 'metadata', 'nonexistent') FROM test_extract;

DROP TABLE test_extract;