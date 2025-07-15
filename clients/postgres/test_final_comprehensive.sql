-- Comprehensive Final Test Suite for kJSON PostgreSQL Extension

\echo '========================================='
\echo '1. TABLE STORAGE TEST (Main Fix)'
\echo '========================================='
DROP TABLE IF EXISTS kjson_test CASCADE;
CREATE TABLE kjson_test (
    id SERIAL PRIMARY KEY,
    data kjson,
    metadata kjson
);

-- Insert various kjson types
INSERT INTO kjson_test (data, metadata) VALUES
    (kjson_build_object('type', 'user', 'name', 'Alice', 'age', 30), 
     kjson_build_object('created', current_timestamp, 'version', 1)),
    (kjson_build_array(1, 2, 3, 'test', true, null),
     kjson_build_object('tags', kjson_build_array('important', 'archived'))),
    (kjson_build_object('bigint', 123456789012345678901234567890::numeric, 
                        'decimal', 123.45::numeric,
                        'uuid', gen_random_uuid(),
                        'date', current_timestamp),
     kjson_build_object('precision', 'high'));

-- Test retrieval and operators on stored data
\echo '\n-- Basic retrieval:'
SELECT id, data FROM kjson_test;

\echo '\n-- Arrow operators on stored data:'
SELECT id, data->'name', data->'type', metadata->'version' FROM kjson_test;

\echo '\n-- Path extraction on stored data:'
SELECT id, data #>> '{name}', metadata #>> '{tags,0}' FROM kjson_test;

\echo '\n========================================='
\echo '2. BIGINT AND DECIMAL PRECISION TEST'
\echo '========================================='
SELECT kjson_build_object(
    'small_int', 42,
    'big_int', 9223372036854775807::bigint,
    'huge_bigint', 123456789012345678901234567890::numeric,
    'decimal', 123.45::numeric,
    'sci_notation', 1.23e-10::numeric,
    'precise', 3.141592653589793238462643383279502884197::numeric
) as number_test;

\echo '\n========================================='
\echo '3. AGGREGATE FUNCTIONS TEST'
\echo '========================================='
-- Test kjson_agg
\echo '\n-- kjson_agg on stored data:'
SELECT kjson_agg(data) FROM kjson_test;

\echo '\n-- kjson_agg with GROUP BY:'
WITH grouped_data AS (
    SELECT 
        CASE WHEN id <= 2 THEN 'group1' ELSE 'group2' END as grp,
        data
    FROM kjson_test
)
SELECT grp, kjson_agg(data) FROM grouped_data GROUP BY grp;

\echo '\n-- kjson_object_agg:'
SELECT kjson_object_agg(id::text, data) FROM kjson_test;

\echo '\n========================================='
\echo '4. ARRAY FUNCTIONS TEST'
\echo '========================================='
WITH array_data AS (
    SELECT kjson_build_array('a', 'b', 'c', 'd', 'e') as arr
)
SELECT elem FROM array_data, kjson_array_elements(arr) elem;

WITH array_data AS (
    SELECT kjson_build_array(1, 2.5, true, 'text', null) as arr
)
SELECT elem FROM array_data, kjson_array_elements_text(arr) elem;

\echo '\n========================================='
\echo '5. COMPLEX OPERATIONS TEST'
\echo '========================================='
-- Nested operations
WITH complex_data AS (
    SELECT kjson_build_object(
        'users', kjson_build_array(
            kjson_build_object('id', 1, 'name', 'Alice', 'active', true),
            kjson_build_object('id', 2, 'name', 'Bob', 'active', false),
            kjson_build_object('id', 3, 'name', 'Charlie', 'active', true)
        ),
        'metadata', kjson_build_object(
            'total', 3,
            'last_updated', '2025-01-15T10:00:00Z'::timestamp
        )
    ) as doc
)
SELECT 
    doc->'users'->0->'name' as first_user,
    doc #> '{users,1,name}' as second_user,
    doc #>> '{metadata,total}' as total_count
FROM complex_data;

\echo '\n========================================='
\echo '6. UNION ALL TEST (Was Failing)'
\echo '========================================='
WITH test_data AS (
    SELECT kjson_build_object('id', 1, 'type', 'test') as doc
)
SELECT 'cast_to_text' as operation, doc::text as result FROM test_data
UNION ALL
SELECT 'kjson_compact' as operation, kjson_compact(doc) as result FROM test_data
UNION ALL
SELECT 'kjson_pretty' as operation, kjson_pretty(doc, 2) as result FROM test_data;

\echo '\n========================================='
\echo '7. EXISTENCE AND CONTAINMENT TEST'
\echo '========================================='
WITH test_doc AS (
    SELECT kjson_build_object('a', 1, 'b', 2, 'c', 3, 'd', kjson_build_array(4, 5, 6)) as doc
)
SELECT 
    doc ? 'a' as has_a,
    doc ? 'z' as has_z,
    doc ?| ARRAY['x', 'y', 'z'] as has_any_xyz,
    doc ?| ARRAY['a', 'x'] as has_a_or_x,
    doc ?& ARRAY['a', 'b', 'c'] as has_all_abc,
    doc @> kjson_build_object('a', 1) as contains_a1,
    kjson_build_object('a', 1, 'b', 2, 'c', 3, 'd', kjson_build_array(4, 5, 6), 'e', 7) @> doc as is_contained
FROM test_doc;

\echo '\n========================================='
\echo '8. OUTPUT FORMAT TEST'
\echo '========================================='
-- Test default compact output
SELECT kjson_build_object('compact', 'default', 'spaces', 'none', 'array', kjson_build_array(1,2,3));

-- Test kjson_strip_nulls
SELECT kjson_strip_nulls(
    kjson_build_object(
        'keep1', 'value1',
        'remove1', null,
        'nested', kjson_build_object(
            'keep2', 'value2',
            'remove2', null,
            'array', kjson_build_array(1, null, 2, null, 3)
        ),
        'remove3', null
    )
);

\echo '\n========================================='
\echo '9. TYPE FUNCTIONS TEST'
\echo '========================================='
SELECT 
    kjson_typeof(kjson_build_object('a', 1)) as obj,
    kjson_typeof(kjson_build_array(1, 2, 3)) as arr,
    kjson_typeof('123'::kjson) as num,
    kjson_typeof('"text"'::kjson) as str,
    kjson_typeof('true'::kjson) as bool,
    kjson_typeof('null'::kjson) as null_type;

\echo '\n========================================='
\echo '10. EDGE CASES TEST'
\echo '========================================='
-- Empty structures (via parsing since builder functions need args)
SELECT '{}'::kjson as empty_obj, '[]'::kjson as empty_arr;

-- Very nested structure
SELECT kjson_build_object(
    'level1', kjson_build_object(
        'level2', kjson_build_object(
            'level3', kjson_build_object(
                'level4', kjson_build_object(
                    'level5', 'deep value'
                )
            )
        )
    )
) #>> '{level1,level2,level3,level4,level5}' as deep_extraction;

-- Clean up
DROP TABLE kjson_test;

\echo '\n========================================='
\echo 'ALL TESTS COMPLETED SUCCESSFULLY!'
\echo '========================================='