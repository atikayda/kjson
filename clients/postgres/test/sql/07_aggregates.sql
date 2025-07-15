-- Test aggregate functions
\echo '=== Testing Aggregate Functions ==='

-- Create test data
CREATE TEMP TABLE test_agg (
    category text,
    value kjson
);

INSERT INTO test_agg (category, value) VALUES
    ('numbers', '10'::kjson),
    ('numbers', '20'::kjson),
    ('numbers', '30'::kjson),
    ('strings', '"apple"'::kjson),
    ('strings', '"banana"'::kjson),
    ('strings', '"cherry"'::kjson),
    ('mixed', '{"type": "object", "value": 1}'::kjson),
    ('mixed', 'true'::kjson),
    ('mixed', 'null'::kjson);

-- Test kjson_agg
SELECT category, kjson_agg(value) AS aggregated
FROM test_agg
GROUP BY category
ORDER BY category;

-- Test kjson_agg with complex values
WITH complex_data AS (
    SELECT n AS id, 
           ('{
               "id": ' || n || 'n,
               "name": "Item ' || n || '",
               "price": ' || (n * 10.99) || 'm
            }')::kjson AS item
    FROM generate_series(1, 5) n
)
SELECT kjson_agg(item) AS items FROM complex_data;

-- Test kjson_object_agg
CREATE TEMP TABLE test_obj_agg (
    key text,
    value kjson
);

INSERT INTO test_obj_agg (key, value) VALUES
    ('name', '"Alice"'::kjson),
    ('age', '30'::kjson),
    ('city', '"NYC"'::kjson),
    ('active', 'true'::kjson);

SELECT kjson_object_agg(key, value) AS person FROM test_obj_agg;

-- Test kjson_object_agg with grouping
CREATE TEMP TABLE test_grouped (
    group_id int,
    key text,
    value kjson
);

INSERT INTO test_grouped (group_id, key, value) VALUES
    (1, 'a', '1'::kjson),
    (1, 'b', '2'::kjson),
    (2, 'x', '"hello"'::kjson),
    (2, 'y', '"world"'::kjson);

SELECT group_id, kjson_object_agg(key, value) AS obj
FROM test_grouped
GROUP BY group_id
ORDER BY group_id;

-- Test with NULL handling
INSERT INTO test_agg (category, value) VALUES
    ('with_nulls', '1'::kjson),
    ('with_nulls', NULL),
    ('with_nulls', '3'::kjson);

SELECT category, kjson_agg(value) AS aggregated
FROM test_agg
WHERE category = 'with_nulls'
GROUP BY category;

DROP TABLE test_agg;
DROP TABLE test_obj_agg;
DROP TABLE test_grouped;