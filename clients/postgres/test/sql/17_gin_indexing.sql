-- === Testing GIN indexing for kjson ===

-- Create test table for GIN indexing
CREATE TABLE gin_test (
    id SERIAL PRIMARY KEY,
    data kjson,
    description TEXT
);

-- Insert comprehensive test data
INSERT INTO gin_test (data, description) VALUES
    (kjson_build_object('name', 'Alice', 'age', 30, 'active', true, 'tags', kjson_build_array('user', 'admin')), 'User Alice'),
    (kjson_build_object('name', 'Bob', 'age', 25, 'active', false, 'tags', kjson_build_array('user', 'guest')), 'User Bob'),
    (kjson_build_object('name', 'Charlie', 'age', 35, 'active', true, 'department', 'engineering', 'tags', kjson_build_array('employee', 'senior')), 'User Charlie'),
    (kjson_build_object('product', 'Widget', 'price', 99.99, 'in_stock', true, 'categories', kjson_build_array('electronics', 'gadgets')), 'Product Widget'),
    (kjson_build_object('product', 'Gadget', 'price', 149.99, 'in_stock', false, 'categories', kjson_build_array('electronics', 'premium')), 'Product Gadget'),
    (kjson_build_object('event', 'Conference', 'date', '2025-03-15T10:00:00.000Z'::kjson, 'attendees', 150, 'tags', kjson_build_array('business', 'networking')), 'Event Conference'),
    (kjson_build_object('nested', kjson_build_object('level1', kjson_build_object('level2', 'deep_value', 'other', 42)), 'top_level', 'surface'), 'Nested object'),
    ('{"empty_object": {}, "null_value": null, "mixed": [1, "text", true]}'::kjson, 'Mixed types'),
    (kjson_build_array(1, 2, 3, 4, 5), 'Simple array'),
    (kjson_build_array(kjson_build_object('id', 1), kjson_build_object('id', 2)), 'Array of objects');

-- Create GIN index on the kjson column
CREATE INDEX gin_test_data_idx ON gin_test USING gin (data kjson_gin_ops);

-- Test existence operator ?
SELECT 'Testing ? operator (key existence)' as test_section;

SELECT id, description 
FROM gin_test 
WHERE data ? 'name'
ORDER BY id;

SELECT id, description 
FROM gin_test 
WHERE data ? 'tags'
ORDER BY id;

SELECT id, description 
FROM gin_test 
WHERE data ? 'nonexistent_key'
ORDER BY id;

-- Test existence any operator ?|
SELECT 'Testing ?| operator (any key exists)' as test_section;

SELECT id, description 
FROM gin_test 
WHERE data ?| ARRAY['name', 'product']
ORDER BY id;

SELECT id, description 
FROM gin_test 
WHERE data ?| ARRAY['age', 'price']
ORDER BY id;

SELECT id, description 
FROM gin_test 
WHERE data ?| ARRAY['missing1', 'missing2']
ORDER BY id;

-- Test existence all operator ?&
SELECT 'Testing ?& operator (all keys exist)' as test_section;

SELECT id, description 
FROM gin_test 
WHERE data ?& ARRAY['name', 'age']
ORDER BY id;

SELECT id, description 
FROM gin_test 
WHERE data ?& ARRAY['name', 'tags']
ORDER BY id;

SELECT id, description 
FROM gin_test 
WHERE data ?& ARRAY['name', 'nonexistent']
ORDER BY id;

-- Test containment operator @>
SELECT 'Testing @> operator (contains)' as test_section;

SELECT id, description 
FROM gin_test 
WHERE data @> '{"active": true}'::kjson
ORDER BY id;

SELECT id, description 
FROM gin_test 
WHERE data @> '{"name": "Alice"}'::kjson
ORDER BY id;

SELECT id, description 
FROM gin_test 
WHERE data @> '{"age": 30, "active": true}'::kjson
ORDER BY id;

-- Test complex containment queries
SELECT 'Testing complex @> queries' as test_section;

-- Objects with specific nested structure
SELECT id, description 
FROM gin_test 
WHERE data @> '{"tags": ["user"]}'::kjson
ORDER BY id;

-- Test performance with EXPLAIN (without actual output)
SELECT 'Testing query plans (index usage)' as test_section;

-- Simple existence query
EXPLAIN (COSTS OFF, BUFFERS OFF) 
SELECT id FROM gin_test WHERE data ? 'name';

-- Containment query  
EXPLAIN (COSTS OFF, BUFFERS OFF)
SELECT id FROM gin_test WHERE data @> '{"active": true}'::kjson;

-- Multiple key existence
EXPLAIN (COSTS OFF, BUFFERS OFF)
SELECT id FROM gin_test WHERE data ?& ARRAY['name', 'age'];

-- Test index effectiveness with larger dataset
INSERT INTO gin_test (data, description)
SELECT 
    kjson_build_object(
        'user_id', n,
        'score', (n * 17) % 100,
        'active', (n % 3 = 0),
        'category', CASE (n % 4) 
            WHEN 0 THEN 'premium'
            WHEN 1 THEN 'standard' 
            WHEN 2 THEN 'basic'
            ELSE 'trial'
        END,
        'metadata', kjson_build_object('created', '2025-01-01T00:00:00.000Z'::kjson, 'version', (n % 5) + 1)
    ),
    'Generated user ' || n
FROM generate_series(1001, 2000) n;

-- Test queries on larger dataset
SELECT 'Testing on larger dataset (1000 additional records)' as test_section;

-- Count active users
SELECT COUNT(*) as active_users
FROM gin_test 
WHERE data @> '{"active": true}'::kjson;

-- Find users by category
SELECT COUNT(*) as premium_users
FROM gin_test 
WHERE data @> '{"category": "premium"}'::kjson;

-- Complex query with multiple conditions
SELECT COUNT(*) as complex_query_result
FROM gin_test 
WHERE data ? 'user_id' 
  AND data @> '{"active": true}'::kjson
  AND data ?& ARRAY['score', 'category'];

-- Test edge cases
SELECT 'Testing edge cases' as test_section;

-- Empty object containment
SELECT COUNT(*) as empty_object_contains
FROM gin_test 
WHERE data @> '{}'::kjson;

-- Null value queries
SELECT COUNT(*) as has_null_value
FROM gin_test 
WHERE data ? 'null_value';

-- Array containment
SELECT COUNT(*) as array_contains_electronics
FROM gin_test 
WHERE data @> '{"categories": ["electronics"]}'::kjson;

-- Cleanup
DROP INDEX gin_test_data_idx;
DROP TABLE gin_test;