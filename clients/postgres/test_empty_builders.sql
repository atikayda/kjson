-- Test if empty builders are actually needed

\echo '=== Testing with NULL arguments ==='
SELECT kjson_build_object(NULL) as object_with_null;
SELECT kjson_build_array(NULL) as array_with_null;

\echo '=== Testing practical use cases ==='
-- Empty objects and arrays are more commonly created via parsing
SELECT '{}'::kjson as empty_object_parse;
SELECT '[]'::kjson as empty_array_parse;

\echo '=== Testing builder functions with valid arguments ==='
SELECT kjson_build_object('key', NULL) as object_with_null_value;
SELECT kjson_build_array(NULL, NULL) as array_with_null_values;