-- Test aggregate functions specifically

\echo '=== Testing kjson_agg with simple values ==='
SELECT kjson_agg(val) FROM (VALUES 
    (kjson_build_object('a', 1)),
    (kjson_build_object('b', 2)),
    (kjson_build_object('c', 3))
) AS t(val);

\echo '=== Testing kjson_agg with table data ==='
-- Create a test table
DROP TABLE IF EXISTS test_agg;
CREATE TABLE test_agg (id int, data kjson);
INSERT INTO test_agg VALUES 
    (1, kjson_build_object('name', 'Alice', 'age', 30)),
    (2, kjson_build_object('name', 'Bob', 'age', 25)),
    (3, kjson_build_object('name', 'Charlie', 'age', 35));

-- Test aggregation
SELECT kjson_agg(data) FROM test_agg;

\echo '=== Testing kjson_object_agg ==='
SELECT kjson_object_agg(name::text, value) FROM (VALUES
    ('a', kjson_build_object('val', 1)),
    ('b', kjson_build_object('val', 2)),
    ('c', kjson_build_object('val', 3))
) AS t(name, value);

\echo '=== Testing the specific UNION ALL case ==='
WITH test_data AS (
    SELECT kjson_build_object(
        'name', 'Test',
        'active', true,
        'items', kjson_build_array(1, 2, 3),
        'nested', kjson_build_object('key', 'value')
    ) as doc
)
SELECT 
    'Default (compact)' as format_type,
    doc::text as output
FROM test_data
UNION ALL
SELECT 
    'Explicit compact' as format_type,
    kjson_compact(doc) as output
FROM test_data;

\echo '=== Testing empty builder functions ==='
SELECT kjson_build_object() as empty_object;
SELECT kjson_build_array() as empty_array;

-- Clean up
DROP TABLE test_agg;