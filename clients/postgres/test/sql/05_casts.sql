-- Test casting between types
\echo '=== Testing Type Casting ==='

-- Test kjson to json cast
SELECT '{"name": "Alice", "age": 30}'::kjson::json;
SELECT '[1, 2, 3]'::kjson::json;

-- Test extended types to json (should lose type info)
SELECT '{
    "id": 123456789012345678901234567890n,
    "price": 99.99m,
    "uuid": 550e8400-e29b-41d4-a716-446655440000,
    "created": 2025-01-10T12:00:00Z
}'::kjson::json;

-- Test kjson to jsonb cast
SELECT '{"b": 2, "a": 1}'::kjson::jsonb;
SELECT '[true, null, 42]'::kjson::jsonb;

-- Test json to kjson cast
SELECT '{"name": "Bob"}'::json::kjson;
SELECT '[1, 2, 3]'::json::kjson;

-- Test jsonb to kjson cast  
SELECT '{"z": 26, "a": 1}'::jsonb::kjson;
SELECT '[]'::jsonb::kjson;

-- Test text to kjson cast
SELECT 'null'::text::kjson;
SELECT '{"unquoted": "keys", work: true}'::text::kjson;

-- Test round-trip conversions
WITH test_data AS (
    SELECT '{
        name: "Test",
        count: 999999999999999999n,
        price: 19.99m
    }'::kjson AS original
)
SELECT 
    original,
    original::json::kjson AS via_json,
    original::jsonb::kjson AS via_jsonb,
    original = original::json::kjson AS json_preserves,
    original = original::jsonb::kjson AS jsonb_preserves
FROM test_data;