-- Test basic kJSON types
\echo '=== Testing Basic Types ==='

-- Test null
SELECT 'null'::kjson;
SELECT 'null'::kjson = 'null'::kjson AS null_equals;

-- Test boolean
SELECT 'true'::kjson;
SELECT 'false'::kjson;
SELECT 'true'::kjson = 'true'::kjson AS bool_equals;
SELECT 'true'::kjson <> 'false'::kjson AS bool_not_equals;

-- Test numbers
SELECT '42'::kjson;
SELECT '-3.14'::kjson;
SELECT '1.23e10'::kjson;
SELECT '42'::kjson = '42'::kjson AS number_equals;

-- Test strings
SELECT '"hello world"'::kjson;
SELECT '"escaped \"quotes\""'::kjson;
SELECT '"\n\t\r"'::kjson;
SELECT '"unicode: \u263A"'::kjson;

-- Test arrays
SELECT '[1, 2, 3]'::kjson;
SELECT '["a", "b", "c"]'::kjson;
SELECT '[true, false, null]'::kjson;
SELECT '[]'::kjson;

-- Test objects
SELECT '{}'::kjson;
SELECT '{"name": "Alice", "age": 30}'::kjson;
SELECT '{"nested": {"key": "value"}}'::kjson;