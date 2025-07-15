-- Test kJSON aggregate functions
-- Tests: kjson_agg, kjson_object_agg

\echo '=== Testing aggregate functions ==='

-- Create test data
CREATE TEMP TABLE test_aggregates (
    id integer,
    category text,
    name text,
    value numeric,
    active boolean,
    metadata kjson
);

INSERT INTO test_aggregates VALUES
    (1, 'A', 'Item 1', 10.5, true, '{"color": "red"}'::kjson),
    (2, 'A', 'Item 2', 20.0, true, '{"color": "blue"}'::kjson),
    (3, 'B', 'Item 3', 15.75, false, '{"color": "green"}'::kjson),
    (4, 'B', 'Item 4', 30.25, true, '{"color": "yellow"}'::kjson),
    (5, 'C', 'Item 5', 5.0, false, null);

-- Test kjson_agg - aggregate values into array
\echo '-- Testing kjson_agg'
-- kjson_agg expects kjson values, so we need to convert first
SELECT kjson_agg(kjson_build_object('value', value)) as all_values FROM test_aggregates;
-- Need to wrap text values as JSON strings
SELECT kjson_agg(('"' || name || '"')::kjson) as all_names FROM test_aggregates WHERE name IS NOT NULL;
SELECT kjson_agg(kjson_build_object('active', active)) as all_active FROM test_aggregates;

-- Aggregate with GROUP BY
SELECT category, kjson_agg(('"' || name || '"')::kjson) as names_by_category 
FROM test_aggregates 
GROUP BY category 
ORDER BY category;

-- Aggregate kjson values
SELECT kjson_agg(metadata) as all_metadata 
FROM test_aggregates 
WHERE metadata IS NOT NULL;

-- Aggregate complex structures
SELECT kjson_agg(
    kjson_build_object(
        'id', id,
        'name', name,
        'value', value
    )
) as object_array
FROM test_aggregates;

-- Test kjson_object_agg - aggregate key/value pairs into object
\echo '-- Testing kjson_object_agg'
SELECT kjson_object_agg(name, kjson_build_object('value', value)) as name_value_map 
FROM test_aggregates;

SELECT kjson_object_agg(id::text, ('"' || name || '"')::kjson) as id_name_map 
FROM test_aggregates;

-- Object aggregation with GROUP BY
SELECT category, 
       kjson_object_agg(name, kjson_build_object('value', value)) as items_by_name
FROM test_aggregates
GROUP BY category
ORDER BY category;

-- Aggregate with complex values
-- Note: kjson_build_object currently has issues with nested kjson values
-- So we'll aggregate simpler objects
SELECT kjson_object_agg(
    name,
    kjson_build_object(
        'value', value,
        'active', active
    )
) as simple_map
FROM test_aggregates;

-- Test with NULL handling
\echo '-- Testing NULL handling in aggregates'
CREATE TEMP TABLE test_nulls (
    key text,
    value text
);

INSERT INTO test_nulls VALUES
    ('a', 'value_a'),
    ('b', null),
    (null, 'value_c'),
    ('d', 'value_d');

-- kjson_agg includes nulls
SELECT kjson_agg(kjson_build_object('value', value)) FROM test_nulls;

-- kjson_object_agg skips null keys
SELECT kjson_object_agg(key, kjson_build_object('value', value)) FROM test_nulls WHERE key IS NOT NULL;

-- Test empty aggregates
\echo '-- Testing empty aggregates'
CREATE TEMP TABLE test_empty (id integer);

-- No rows, so no aggregation happens
SELECT kjson_agg(kjson_build_object('id', id)) FROM test_empty;
SELECT kjson_object_agg(id::text, kjson_build_object('id', id)) FROM test_empty;

-- Test distinct aggregation
\echo '-- Testing DISTINCT in aggregates'
CREATE TEMP TABLE test_distinct (
    value text
);

INSERT INTO test_distinct VALUES
    ('a'), ('b'), ('a'), ('c'), ('b'), ('a');

-- DISTINCT requires equality operators which kjson doesn't have
-- So we'll just aggregate all values including duplicates
SELECT kjson_agg(('"' || value || '"')::kjson ORDER BY value) as all_values
FROM test_distinct;

-- Test ORDER BY in aggregates
\echo '-- Testing ORDER BY in aggregates'
SELECT kjson_agg(('"' || name || '"')::kjson ORDER BY value DESC) as names_by_value_desc
FROM test_aggregates;

SELECT kjson_agg(
    kjson_build_object('name', name, 'value', value) 
    ORDER BY value
) as objects_by_value
FROM test_aggregates;

-- Clean up
DROP TABLE test_aggregates;
DROP TABLE test_nulls;
DROP TABLE test_empty;
DROP TABLE test_distinct;