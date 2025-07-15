-- Test kJSON existence operators
-- Tests: ?, ?|, ?&, kjson_exists, kjson_exists_any, kjson_exists_all

\echo '=== Testing existence operators ?, ?|, ?& ==='

-- Create test data
CREATE TEMP TABLE test_existence AS
SELECT 
    kjson_build_object('a', 1, 'b', 2, 'c', 3) as obj1,
    kjson_build_object('x', 10, 'y', 20, 'z', 30) as obj2,
    kjson_build_object(
        'name', 'Test',
        'tags', kjson_build_array('red', 'green', 'blue'),
        'active', true,
        'metadata', kjson_build_object('version', 1, 'type', 'standard')
    ) as complex_obj,
    kjson_build_array(1, 2, 3) as arr,
    '"just a string"'::kjson as str,
    '123'::kjson as num,
    'null'::kjson as null_val;

-- Test ? operator (single key exists)
\echo '-- Testing ? operator'
SELECT obj1 ? 'a' as has_a, obj1 ? 'b' as has_b, obj1 ? 'd' as has_d FROM test_existence;
SELECT obj2 ? 'x' as has_x, obj2 ? 'a' as has_a FROM test_existence;
SELECT complex_obj ? 'name' as has_name, 
       complex_obj ? 'tags' as has_tags,
       complex_obj ? 'missing' as has_missing 
FROM test_existence;

-- ? operator on non-objects
SELECT arr ? 'a' as arr_has_a FROM test_existence; -- Should be false
SELECT str ? 'a' as str_has_a FROM test_existence; -- Should be false
SELECT num ? 'a' as num_has_a FROM test_existence; -- Should be false
SELECT null_val ? 'a' as null_has_a FROM test_existence; -- Should be false

-- Test ?| operator (any keys exist)
\echo '-- Testing ?| operator'
SELECT obj1 ?| ARRAY['a', 'x'] as has_a_or_x,
       obj1 ?| ARRAY['d', 'e', 'f'] as has_d_e_or_f,
       obj1 ?| ARRAY['a', 'd'] as has_a_or_d
FROM test_existence;

SELECT complex_obj ?| ARRAY['name', 'missing'] as has_name_or_missing,
       complex_obj ?| ARRAY['missing1', 'missing2'] as has_neither
FROM test_existence;

-- ?| with empty array
SELECT obj1 ?| ARRAY[]::text[] as empty_array FROM test_existence;

-- ?| on non-objects
SELECT arr ?| ARRAY['a', 'b'] as arr_has_any FROM test_existence;
SELECT str ?| ARRAY['a', 'b'] as str_has_any FROM test_existence;

-- Test ?& operator (all keys exist)
\echo '-- Testing ?& operator'
SELECT obj1 ?& ARRAY['a', 'b'] as has_a_and_b,
       obj1 ?& ARRAY['a', 'b', 'c'] as has_all_abc,
       obj1 ?& ARRAY['a', 'b', 'd'] as has_a_b_and_d
FROM test_existence;

SELECT complex_obj ?& ARRAY['name', 'tags'] as has_name_and_tags,
       complex_obj ?& ARRAY['name', 'tags', 'active'] as has_all_three,
       complex_obj ?& ARRAY['name', 'missing'] as has_name_and_missing
FROM test_existence;

-- ?& with empty array (should be true - vacuous truth)
SELECT obj1 ?& ARRAY[]::text[] as empty_array FROM test_existence;

-- ?& with single element
SELECT obj1 ?& ARRAY['a'] as single_exists,
       obj1 ?& ARRAY['d'] as single_missing
FROM test_existence;

-- ?& on non-objects
SELECT arr ?& ARRAY['a', 'b'] as arr_has_all FROM test_existence;
SELECT str ?& ARRAY['a', 'b'] as str_has_all FROM test_existence;

-- Test with nested objects
\echo '-- Testing existence on nested objects'
CREATE TEMP TABLE test_nested AS
SELECT kjson_build_object(
    'user', kjson_build_object(
        'id', 1,
        'profile', kjson_build_object(
            'name', 'Alice',
            'email', 'alice@example.com',
            'settings', kjson_build_object(
                'theme', 'dark',
                'notifications', true
            )
        )
    ),
    'posts', kjson_build_array(
        kjson_build_object('id', 1, 'title', 'First'),
        kjson_build_object('id', 2, 'title', 'Second')
    )
) as data;

-- Check top-level keys
SELECT data ? 'user' as has_user,
       data ? 'posts' as has_posts,
       data ? 'comments' as has_comments
FROM test_nested;

-- Cannot check nested keys directly with ?
SELECT (data #> '{user}') ? 'id' as user_has_id,
       (data #> '{user}') ? 'profile' as user_has_profile,
       (data #> '{user, profile}') ? 'email' as profile_has_email
FROM test_nested;

-- Test with special key names
\echo '-- Testing special key names'
CREATE TEMP TABLE test_special_keys AS
SELECT kjson_build_object(
    '', 'empty key',
    ' ', 'space key',
    'with space', 'key with space',
    'with-dash', 'key with dash',
    'with.dot', 'key with dot',
    '123', 'numeric key',
    'true', 'boolean-like key',
    'null', 'null-like key',
    '🦄', 'unicode key'
) as data;

SELECT data ? '' as has_empty,
       data ? ' ' as has_space,
       data ? 'with space' as has_with_space,
       data ? 'with-dash' as has_dash,
       data ? 'with.dot' as has_dot,
       data ? '123' as has_numeric,
       data ? 'true' as has_true,
       data ? 'null' as has_null,
       data ? '🦄' as has_unicode
FROM test_special_keys;

-- Test case sensitivity
\echo '-- Testing case sensitivity'
CREATE TEMP TABLE test_case AS
SELECT kjson_build_object('Key', 1, 'key', 2, 'KEY', 3) as data;

SELECT data ? 'Key' as has_Key,
       data ? 'key' as has_key,
       data ? 'KEY' as has_KEY,
       data ? 'kEy' as has_kEy
FROM test_case;

-- Test with null values
\echo '-- Testing with null values'
CREATE TEMP TABLE test_nulls AS
SELECT kjson_build_object('a', null, 'b', 2) as data;

SELECT data ? 'a' as has_a_null,
       data ? 'b' as has_b,
       data ? 'c' as has_c
FROM test_nulls;

-- Clean up
DROP TABLE test_existence;
DROP TABLE test_nested;
DROP TABLE test_special_keys;
DROP TABLE test_case;
DROP TABLE test_nulls;