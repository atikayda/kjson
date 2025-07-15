-- Test extended kJSON types
\echo '=== Testing Extended Types ==='

-- Test BigInt
SELECT '123456789012345678901234567890n'::kjson;
SELECT '-999999999999999999999999999999n'::kjson;
SELECT '0n'::kjson;

-- Test Decimal128
SELECT '99.99m'::kjson;
SELECT '-123.456m'::kjson;
SELECT '0.00000001m'::kjson;
SELECT '1.23e10m'::kjson;

-- Test UUID
SELECT '550e8400-e29b-41d4-a716-446655440000'::kjson;
SELECT '{id: 550e8400-e29b-41d4-a716-446655440000}'::kjson;

-- Test Date
SELECT '2025-01-10T12:00:00Z'::kjson;
SELECT '2025-01-10T12:00:00.123Z'::kjson;
SELECT '{created: 2025-01-10T12:00:00Z}'::kjson;

-- Test mixed types in object
SELECT '{
    "id": 123456789012345678901234567890n,
    "price": 99.99m,
    "uuid": 550e8400-e29b-41d4-a716-446655440000,
    "created": 2025-01-10T12:00:00Z,
    "active": true,
    "tags": ["new", "sale"]
}'::kjson;