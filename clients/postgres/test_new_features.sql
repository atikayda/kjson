-- Test new kJSON builder functions and operators

-- First, create the extension if not exists
CREATE EXTENSION IF NOT EXISTS kjson;

-- Test builder functions
SELECT kjson_build_object('name', 'Alice', 'age', 30, 'active', true);
SELECT kjson_build_array('hello', 123, true, null, 99.99);

-- Test with extended types
SELECT kjson_build_object(
    'id', '550e8400-e29b-41d4-a716-446655440000'::uuid,
    'balance', 123456789012345678901234567890::numeric,
    'rate', 3.14159265358979323846264338327950288419716939937510::numeric,
    'created', now()
);

-- Test row_to_kjson
CREATE TEMP TABLE test_users (
    id serial PRIMARY KEY,
    name text,
    email text,
    age integer,
    balance numeric,
    created_at timestamptz DEFAULT now()
);

INSERT INTO test_users (name, email, age, balance) VALUES 
    ('Alice', 'alice@example.com', 30, 1000.50),
    ('Bob', 'bob@example.com', 25, 2500.75);

SELECT row_to_kjson(test_users.*) FROM test_users;

-- Test path extraction operators
WITH data AS (
    SELECT kjson_build_object(
        'user', kjson_build_object(
            'name', 'Alice',
            'address', kjson_build_object(
                'city', 'New York',
                'zip', '10001'
            )
        ),
        'items', kjson_build_array(
            kjson_build_object('name', 'Item 1', 'price', 19.99),
            kjson_build_object('name', 'Item 2', 'price', 29.99)
        )
    ) as doc
)
SELECT 
    doc #> '{user, name}' as user_name,
    doc #> '{user, address, city}' as city,
    doc #> '{items, 0}' as first_item,
    doc #>> '{items, 1, price}' as second_item_price
FROM data;

-- Test existence operators
WITH data AS (
    SELECT kjson_build_object('a', 1, 'b', 2, 'c', 3) as doc
)
SELECT 
    doc ? 'a' as has_a,
    doc ? 'd' as has_d,
    doc ?| ARRAY['x', 'y', 'a'] as has_any,
    doc ?& ARRAY['a', 'b'] as has_all,
    doc ?& ARRAY['a', 'd'] as has_all_2
FROM data;

-- Test array functions
SELECT kjson_array_length(kjson_build_array(1, 2, 3, 4, 5));

-- Test object keys
SELECT kjson_object_keys(kjson_build_object('a', 1, 'b', 2, 'c', 3));

-- Test containment operators
SELECT 
    kjson_build_object('a', 1, 'b', 2) @> kjson_build_object('a', 1) as contains,
    kjson_build_object('a', 1) <@ kjson_build_object('a', 1, 'b', 2) as contained_by;

-- Test formatting functions
SELECT kjson_build_object(
    'name', 'Test',
    'values', kjson_build_array(1, 2, 3),
    'nested', kjson_build_object('key', 'value')
) as default_compact_format;

SELECT kjson_compact(kjson_build_object(
    'name', 'Test',
    'values', kjson_build_array(1, 2, 3),
    'nested', kjson_build_object('key', 'value')
)) as explicit_compact_format;

SELECT kjson_pretty(kjson_build_object(
    'name', 'Test',
    'values', kjson_build_array(1, 2, 3),
    'nested', kjson_build_object('key', 'value')
), 2) as pretty_format;

-- Show that casting preserves extended types
SELECT '{"bignum": 123456789012345678901234567890n, "decimal": 99.99m}'::kjson;

-- Clean up
DROP TABLE test_users;