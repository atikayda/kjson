-- Test kjson operators
\echo '=== Testing Operators ==='

-- Create test data
CREATE TEMP TABLE test_kjson (
    id serial PRIMARY KEY,
    data kjson
);

INSERT INTO test_kjson (data) VALUES
    ('{"name": "Alice", "age": 30, "city": "NYC"}'::kjson),
    ('{"name": "Bob", "scores": [85, 92, 78]}'::kjson),
    ('{
        "product": "Widget",
        "price": 19.99m,
        "id": 123456789012345678901234567890n,
        "uuid": 550e8400-e29b-41d4-a716-446655440000
    }'::kjson);

-- Test -> operator (get field as kjson)
SELECT data->'name' FROM test_kjson;
SELECT data->'age' FROM test_kjson WHERE data->'age' IS NOT NULL;
SELECT data->'scores' FROM test_kjson WHERE data->'scores' IS NOT NULL;

-- Test ->> operator (get field as text)
SELECT data->>'name' AS name FROM test_kjson;
SELECT data->>'price' AS price FROM test_kjson WHERE data->>'price' IS NOT NULL;

-- Test array access
SELECT data->'scores'->0 FROM test_kjson WHERE data->'scores' IS NOT NULL;
SELECT data->'scores'->>1 FROM test_kjson WHERE data->'scores' IS NOT NULL;

-- Test negative array indices
SELECT data->'scores'->-1 FROM test_kjson WHERE data->'scores' IS NOT NULL;

-- Test equality operators
SELECT COUNT(*) FROM test_kjson WHERE data = '{"name": "Alice", "age": 30, "city": "NYC"}'::kjson;
SELECT COUNT(*) FROM test_kjson WHERE data <> '{}'::kjson;

-- Test chained access
INSERT INTO test_kjson (data) VALUES
    ('{"user": {"profile": {"name": "David", "settings": {"theme": "dark"}}}}'::kjson);

SELECT data->'user'->'profile'->>'name' AS nested_name 
FROM test_kjson 
WHERE data->'user' IS NOT NULL;

SELECT data->'user'->'profile'->'settings'->>'theme' AS theme
FROM test_kjson 
WHERE data->'user' IS NOT NULL;

DROP TABLE test_kjson;