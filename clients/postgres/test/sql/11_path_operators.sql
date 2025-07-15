-- Test kJSON path extraction operators
-- Tests: #>, #>>, kjson_extract_path, kjson_extract_path_text

\echo '=== Testing path extraction operators #> and #>> ==='

-- Create test data
CREATE TEMP TABLE test_paths AS
SELECT kjson_build_object(
    'name', 'Alice',
    'age', 30,
    'address', kjson_build_object(
        'street', '123 Main St',
        'city', 'New York',
        'zip', '10001',
        'coords', kjson_build_object('lat', 40.7128, 'lon', -74.0060)
    ),
    'phones', kjson_build_array('555-1234', '555-5678'),
    'orders', kjson_build_array(
        kjson_build_object('id', 1, 'total', 99.99, 'status', 'shipped'),
        kjson_build_object('id', 2, 'total', 149.99, 'status', 'pending'),
        kjson_build_object('id', 3, 'total', 79.99, 'status', 'delivered')
    ),
    'preferences', kjson_build_object(
        'notifications', kjson_build_object(
            'email', true,
            'sms', false,
            'push', true
        ),
        'theme', 'dark'
    ),
    'metadata', kjson_build_object(
        'bignum', kjson_build_object('value', 123456789012345678901234567890::numeric),
        'created', '2025-01-15T10:00:00Z'::timestamp
    )
) as data;

-- Test #> operator (returns kjson)
\echo '-- Object field access with #>'
SELECT data #> '{name}' FROM test_paths;
SELECT data #> '{age}' FROM test_paths;
SELECT data #> '{address}' FROM test_paths;
SELECT data #> '{address, city}' FROM test_paths;
SELECT data #> '{address, coords}' FROM test_paths;
SELECT data #> '{address, coords, lat}' FROM test_paths;

\echo '-- Array element access with #>'
SELECT data #> '{phones}' FROM test_paths;
SELECT data #> '{phones, 0}' FROM test_paths;
SELECT data #> '{phones, 1}' FROM test_paths;
SELECT data #> '{phones, 2}' FROM test_paths; -- Should return NULL

\echo '-- Nested array/object access with #>'
SELECT data #> '{orders}' FROM test_paths;
SELECT data #> '{orders, 0}' FROM test_paths;
SELECT data #> '{orders, 0, total}' FROM test_paths;
SELECT data #> '{orders, 1, status}' FROM test_paths;

\echo '-- Deep nesting with #>'
SELECT data #> '{preferences}' FROM test_paths;
SELECT data #> '{preferences, notifications}' FROM test_paths;
SELECT data #> '{preferences, notifications, email}' FROM test_paths;
SELECT data #> '{preferences, theme}' FROM test_paths;

\echo '-- Extended types with #>'
SELECT data #> '{metadata}' FROM test_paths;
SELECT data #> '{metadata, bignum}' FROM test_paths;
SELECT data #> '{metadata, bignum, value}' FROM test_paths;
SELECT data #> '{metadata, created}' FROM test_paths;

\echo '-- Non-existent paths with #>'
SELECT data #> '{nonexistent}' FROM test_paths;
SELECT data #> '{address, nonexistent}' FROM test_paths;
SELECT data #> '{orders, 99}' FROM test_paths;
SELECT data #> '{orders, 0, nonexistent}' FROM test_paths;

-- Test #>> operator (returns text)
\echo '-- Object field access with #>>'
SELECT data #>> '{name}' FROM test_paths;
SELECT data #>> '{age}' FROM test_paths;
SELECT data #>> '{address, city}' FROM test_paths;
SELECT data #>> '{address, coords, lat}' FROM test_paths;

\echo '-- Array element access with #>>'
SELECT data #>> '{phones, 0}' FROM test_paths;
SELECT data #>> '{phones, 1}' FROM test_paths;

\echo '-- Nested access with #>>'
SELECT data #>> '{orders, 0, total}' FROM test_paths;
SELECT data #>> '{orders, 1, status}' FROM test_paths;
SELECT data #>> '{preferences, notifications, email}' FROM test_paths;
SELECT data #>> '{preferences, theme}' FROM test_paths;

\echo '-- Complex types as text with #>>'
SELECT data #>> '{address}' FROM test_paths; -- Should stringify the object
SELECT data #>> '{phones}' FROM test_paths; -- Should stringify the array
SELECT data #>> '{metadata, bignum, value}' FROM test_paths; -- BigInt as text
SELECT data #>> '{metadata, created}' FROM test_paths; -- Timestamp as text

\echo '-- NULL handling'
INSERT INTO test_paths 
SELECT kjson_build_object(
    'nulls', kjson_build_object(
        'explicit', null,
        'array', kjson_build_array(1, null, 3)
    )
);

SELECT data #> '{nulls, explicit}' FROM test_paths WHERE data ? 'nulls';
SELECT data #>> '{nulls, explicit}' FROM test_paths WHERE data ? 'nulls';
SELECT data #> '{nulls, array, 1}' FROM test_paths WHERE data ? 'nulls';
SELECT data #>> '{nulls, array, 1}' FROM test_paths WHERE data ? 'nulls';

-- Test with empty path
SELECT data #> '{}' FROM test_paths LIMIT 1; -- Should return whole document
SELECT data #>> '{}' FROM test_paths LIMIT 1; -- Should return stringified document

-- Test with variadic function forms
SELECT kjson_extract_path(data, 'address', 'city') FROM test_paths LIMIT 1;
SELECT kjson_extract_path_text(data, 'orders', '0', 'total') FROM test_paths LIMIT 1;

-- Error cases
-- Invalid array index
SELECT data #> '{phones, abc}' FROM test_paths LIMIT 1;
SELECT data #> '{phones, -1}' FROM test_paths LIMIT 1;

-- Type mismatch (treating object as array)
SELECT data #> '{address, 0}' FROM test_paths LIMIT 1;

-- Treating scalar as container
SELECT data #> '{name, 0}' FROM test_paths LIMIT 1;

-- Clean up
DROP TABLE test_paths;