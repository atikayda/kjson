-- Test all the fixes we implemented

\echo '=== Testing Path Extraction Fix ==='
SELECT 
    kjson_build_object('user', kjson_build_object('name', 'Alice', 'age', 30)) as doc,
    kjson_build_object('user', kjson_build_object('name', 'Alice', 'age', 30)) #> '{user}' as extract_user,
    kjson_build_object('user', kjson_build_object('name', 'Alice', 'age', 30)) #> '{user,name}' as extract_name,
    kjson_build_object('user', kjson_build_object('name', 'Alice', 'age', 30)) #>> '{user,name}' as extract_name_text;

\echo '=== Testing Array Elements Functions ==='
SELECT kjson_array_elements(kjson_build_array(1, 2, 3, 4, 5));
SELECT kjson_array_elements_text(kjson_build_array('a', 'b', 'c'));

\echo '=== Testing kjson_typeof Fix ==='
SELECT 
    kjson_typeof(kjson_build_object('a', 1)) as object_type,
    kjson_typeof(kjson_build_array(1, 2, 3)) as array_type,
    kjson_typeof('"hello"'::kjson) as string_type,
    kjson_typeof('123'::kjson) as number_type,
    kjson_typeof('true'::kjson) as bool_type,
    kjson_typeof('null'::kjson) as null_type;

\echo '=== Testing kjson_strip_nulls Implementation ==='
SELECT kjson_strip_nulls(kjson_build_object(
    'a', 1,
    'b', null,
    'c', kjson_build_object(
        'd', 2,
        'e', null,
        'f', kjson_build_array(3, null, 4)
    ),
    'g', kjson_build_array(null, 5, null),
    'h', null
));

\echo '=== Testing Existence Operators ==='
WITH test_obj AS (
    SELECT kjson_build_object('a', 1, 'b', 2, 'c', 3) as obj
)
SELECT 
    obj ? 'a' as has_a,
    obj ? 'b' as has_b,
    obj ? 'd' as has_d,
    obj ?| ARRAY['a', 'x'] as has_a_or_x,
    obj ?& ARRAY['a', 'b'] as has_a_and_b,
    obj ?& ARRAY['a', 'd'] as has_a_and_d
FROM test_obj;

\echo '=== Testing Compact vs Pretty Formatting ==='
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
FROM test_data
UNION ALL
SELECT 
    'Pretty (2 spaces)' as format_type,
    kjson_pretty(doc, 2) as output
FROM test_data;

\echo '=== Testing Complex Nested Structures ==='
SELECT kjson_build_object(
    'user', kjson_build_object(
        'id', 123,
        'profile', kjson_build_object(
            'name', 'Alice',
            'tags', kjson_build_array('admin', 'user', 'vip')
        )
    ),
    'stats', kjson_build_object(
        'visits', 1000,
        'score', 99.5::numeric
    )
) as complex_doc;