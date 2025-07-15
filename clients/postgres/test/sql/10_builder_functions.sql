-- Test kJSON builder functions
-- Tests: kjson_build_object, kjson_build_array, row_to_kjson

\echo '=== Testing kjson_build_object ==='

-- Basic object building
SELECT kjson_build_object('name', 'Alice', 'age', 30);
SELECT kjson_build_object('a', 1, 'b', 2, 'c', 3);

-- Empty object
-- NOTE: VARIADIC functions require at least one argument in PostgreSQL
-- Use casting for empty objects instead
SELECT '{}'::kjson as empty_object;

-- Object with null values
SELECT kjson_build_object('a', null, 'b', 2);

-- Object with mixed types
SELECT kjson_build_object(
    'string', 'hello',
    'number', 42,
    'float', 3.14,
    'bool_true', true,
    'bool_false', false,
    'null_val', null
);

-- Object with PostgreSQL types
SELECT kjson_build_object(
    'smallint', 32767::smallint,
    'integer', 2147483647::integer,
    'bigint', 9223372036854775807::bigint,
    'real', 3.14::real,
    'double', 3.14159265358979::double precision,
    'numeric', 999999999999999999999999999999.999999999999::numeric
);

-- Object with extended types
SELECT kjson_build_object(
    'uuid', 'a0eebc99-9c0b-4ef8-bb6d-6bb9bd380a11'::uuid,
    'timestamp', '2025-01-15 10:30:00'::timestamp,
    'timestamptz', '2025-01-15 10:30:00-05'::timestamptz
);

-- Nested objects
SELECT kjson_build_object(
    'user', kjson_build_object('name', 'Alice', 'age', 30),
    'location', kjson_build_object('city', 'NYC', 'zip', '10001')
);

-- Object with array value
SELECT kjson_build_object(
    'items', kjson_build_array(1, 2, 3),
    'tags', kjson_build_array('a', 'b', 'c')
);

-- Error cases
-- Odd number of arguments
SELECT kjson_build_object('key_without_value');

-- Null key (should error)
SELECT kjson_build_object(null, 'value');

\echo '=== Testing kjson_build_array ==='

-- Basic array building
SELECT kjson_build_array(1, 2, 3, 4, 5);
SELECT kjson_build_array('a', 'b', 'c');

-- Empty array
-- NOTE: VARIADIC functions require at least one argument in PostgreSQL
-- Use casting for empty arrays instead
SELECT '[]'::kjson as empty_array;

-- Array with null values
SELECT kjson_build_array(1, null, 3);

-- Array with mixed types
SELECT kjson_build_array('string', 42, 3.14, true, false, null);

-- Array with PostgreSQL types
SELECT kjson_build_array(
    32767::smallint,
    2147483647::integer,
    9223372036854775807::bigint,
    3.14::real,
    3.14159265358979::double precision,
    999999999999999999999999999999.999999999999::numeric
);

-- Array with extended types
SELECT kjson_build_array(
    'a0eebc99-9c0b-4ef8-bb6d-6bb9bd380a11'::uuid,
    '2025-01-15 10:30:00'::timestamp,
    '2025-01-15 10:30:00-05'::timestamptz
);

-- Nested arrays
SELECT kjson_build_array(
    kjson_build_array(1, 2, 3),
    kjson_build_array(4, 5, 6),
    kjson_build_array(7, 8, 9)
);

-- Array with objects
SELECT kjson_build_array(
    kjson_build_object('x', 1, 'y', 2),
    kjson_build_object('x', 3, 'y', 4)
);

\echo '=== Testing row_to_kjson ==='

-- Create test table
CREATE TEMP TABLE test_row_conversion (
    id integer,
    name text,
    active boolean,
    score numeric(10,2),
    tags text[],
    created_at timestamp,
    data jsonb
);

-- Insert test data
INSERT INTO test_row_conversion VALUES 
    (1, 'Alice', true, 95.5, ARRAY['student', 'honors'], '2025-01-15 10:00:00', '{"extra": "data"}'),
    (2, 'Bob', false, 87.3, ARRAY['student'], '2025-01-15 11:00:00', null),
    (3, null, null, null, null, null, null);

-- Convert rows to kjson
SELECT row_to_kjson(test_row_conversion.*) FROM test_row_conversion;

-- Convert with calculated columns
SELECT row_to_kjson(t.*) 
FROM (
    SELECT id, name, score * 2 as double_score
    FROM test_row_conversion
) t;

-- Test with composite type
CREATE TYPE test_address AS (
    street text,
    city text,
    zip text
);

CREATE TEMP TABLE test_composite (
    id integer,
    name text,
    address test_address
);

INSERT INTO test_composite VALUES 
    (1, 'Alice', ROW('123 Main St', 'NYC', '10001')::test_address);

SELECT row_to_kjson(test_composite.*) FROM test_composite;

-- Clean up
DROP TABLE test_row_conversion;
DROP TABLE test_composite;
DROP TYPE test_address;