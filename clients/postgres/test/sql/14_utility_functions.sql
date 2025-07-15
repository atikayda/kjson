-- Test kJSON utility functions
-- Tests: kjson_strip_nulls, kjson_pretty, kjson_typeof, kjson_array_elements

\echo '=== Testing utility functions ==='

-- Test kjson_typeof
\echo '-- Testing kjson_typeof'
CREATE TEMP TABLE test_types AS
SELECT 
    kjson_build_object('key', 'value') as obj,
    kjson_build_array(1, 2, 3) as arr,
    '"hello"'::kjson as str,
    '123'::kjson as num,
    '123.45'::kjson as decimal,
    '123456789012345678901234567890n'::kjson as bigint_val,
    '99.99m'::kjson as decimal128_val,
    'true'::kjson as bool_true,
    'false'::kjson as bool_false,
    'null'::kjson as null_val,
    '"a0eebc99-9c0b-4ef8-bb6d-6bb9bd380a11"'::kjson as uuid_str,
    '"2025-01-15T10:00:00Z"'::kjson as date_str;

SELECT kjson_typeof(obj) as object_type FROM test_types;
SELECT kjson_typeof(arr) as array_type FROM test_types;
SELECT kjson_typeof(str) as string_type FROM test_types;
SELECT kjson_typeof(num) as number_type FROM test_types;
SELECT kjson_typeof(decimal) as decimal_type FROM test_types;
SELECT kjson_typeof(bigint_val) as bigint_type FROM test_types;
SELECT kjson_typeof(decimal128_val) as decimal128_type FROM test_types;
SELECT kjson_typeof(bool_true) as bool_true_type FROM test_types;
SELECT kjson_typeof(bool_false) as bool_false_type FROM test_types;
SELECT kjson_typeof(null_val) as null_type FROM test_types;
SELECT kjson_typeof(uuid_str) as uuid_type FROM test_types;
SELECT kjson_typeof(date_str) as date_type FROM test_types;

-- Test kjson_strip_nulls
\echo '-- Testing kjson_strip_nulls'
CREATE TEMP TABLE test_strip_nulls AS
SELECT kjson_build_object(
    'a', 1,
    'b', null,
    'c', kjson_build_object(
        'd', 2,
        'e', null,
        'f', kjson_build_array(3, null, 4)
    ),
    'g', kjson_build_array(null, 5, null),
    'h', null
) as data;

-- Strip nulls from object
SELECT kjson_strip_nulls(data) FROM test_strip_nulls;

-- Test with deeply nested nulls
SELECT kjson_strip_nulls(kjson_build_object(
    'level1', kjson_build_object(
        'level2', kjson_build_object(
            'level3', kjson_build_object(
                'value', null,
                'array', kjson_build_array(null, null, null)
            )
        ),
        'keep', 'this'
    )
));

-- Test with all nulls
SELECT kjson_strip_nulls(kjson_build_object('a', null, 'b', null));

-- Test with array containing nulls
SELECT kjson_strip_nulls(kjson_build_array(1, null, 2, null, 3));

-- Test kjson_pretty with different indentations
\echo '-- Testing kjson_pretty'
CREATE TEMP TABLE test_pretty AS
SELECT kjson_build_object(
    'name', 'Test',
    'values', kjson_build_array(1, 2, 3),
    'nested', kjson_build_object(
        'key', 'value',
        'array', kjson_build_array('a', 'b', 'c')
    ),
    'extended', kjson_build_object(
        'bignum', 123456789012345678901234567890::numeric,
        'uuid', 'a0eebc99-9c0b-4ef8-bb6d-6bb9bd380a11'::uuid
    )
) as data;

-- Default pretty (2 spaces)
SELECT kjson_pretty(data) FROM test_pretty;

-- Pretty with 4 spaces
SELECT kjson_pretty(data, 4) FROM test_pretty;

-- Pretty with tabs (using 8 as convention)
SELECT kjson_pretty(data, 8) FROM test_pretty;

-- Pretty with no indentation (compact)
SELECT kjson_pretty(data, 0) FROM test_pretty;

-- Test kjson_array_elements
\echo '-- Testing kjson_array_elements'
CREATE TEMP TABLE test_array_elements AS
SELECT 
    kjson_build_array(1, 2, 3, 4, 5) as numbers,
    kjson_build_array('a', 'b', 'c') as strings,
    kjson_build_array(
        kjson_build_object('id', 1, 'name', 'Alice'),
        kjson_build_object('id', 2, 'name', 'Bob'),
        kjson_build_object('id', 3, 'name', 'Charlie')
    ) as objects,
    kjson_build_array(
        kjson_build_array(1, 2),
        kjson_build_array(3, 4),
        kjson_build_array(5, 6)
    ) as nested_arrays,
    '[]'::kjson as empty_array;

-- Extract number elements
SELECT kjson_array_elements(numbers) FROM test_array_elements;

-- Extract string elements
SELECT kjson_array_elements(strings) FROM test_array_elements;

-- Extract object elements
SELECT kjson_array_elements(objects) FROM test_array_elements;

-- Extract nested array elements
SELECT kjson_array_elements(nested_arrays) FROM test_array_elements;

-- Empty array
SELECT kjson_array_elements(empty_array) FROM test_array_elements;

-- Test kjson_array_elements_text
\echo '-- Testing kjson_array_elements_text'
SELECT kjson_array_elements_text(numbers) FROM test_array_elements;
SELECT kjson_array_elements_text(strings) FROM test_array_elements;

-- Extract and process object elements
SELECT elem #>> '{name}' as name
FROM test_array_elements, kjson_array_elements(objects) elem;

-- Test error cases
\echo '-- Testing error cases'
-- kjson_typeof on non-kjson should error
-- SELECT kjson_typeof('not kjson'::text);

-- kjson_array_elements on non-array should error
-- SELECT kjson_array_elements('"not an array"'::kjson);

-- Clean up
DROP TABLE test_types;
DROP TABLE test_strip_nulls;
DROP TABLE test_pretty;
DROP TABLE test_array_elements;