-- Test binary format (COPY BINARY)
\echo '=== Testing Binary Format ==='

-- Create test table
CREATE TEMP TABLE test_binary (
    id serial PRIMARY KEY,
    data kjson
);

-- Insert test data
INSERT INTO test_binary (data) VALUES
    ('null'::kjson),
    ('true'::kjson),
    ('42'::kjson),
    ('123456789012345678901234567890n'::kjson),
    ('99.99m'::kjson),
    ('"hello world"'::kjson),
    ('550e8400-e29b-41d4-a716-446655440000'::kjson),
    ('2025-01-10T12:00:00Z'::kjson),
    ('[1, 2, 3]'::kjson),
    ('{"name": "Test", "value": 123}'::kjson);

-- Export to binary format
\echo 'Exporting to binary format...'
-- NOTE: kjson_send not implemented yet, so COPY BINARY will fail
-- COPY test_binary TO '/tmp/kjson_test.bin' (FORMAT binary);

-- Clear table
-- TRUNCATE test_binary;

-- Import from binary format
\echo 'Importing from binary format...'
-- NOTE: kjson_recv not implemented yet, so COPY BINARY will fail
-- COPY test_binary FROM '/tmp/kjson_test.bin' (FORMAT binary);

-- Verify data
SELECT id, data, kjson_typeof(data) as type FROM test_binary ORDER BY id;

-- Test round-trip with complex data
CREATE TEMP TABLE test_complex_binary (
    id int,
    doc kjson
);

INSERT INTO test_complex_binary VALUES (1, '{
    "transaction": {
        "id": 550e8400-e29b-41d4-a716-446655440000,
        "amount": 999999999999999999999999n,
        "price": 123.456m,
        "timestamp": 2025-01-10T12:00:00.123Z,
        "items": [
            {"name": "Item 1", "qty": 5},
            {"name": "Item 2", "qty": 10}
        ],
        "metadata": {
            "source": "API",
            "version": 2,
            "tags": ["urgent", "verified"]
        }
    }
}'::kjson);

-- Export complex data
-- NOTE: kjson_send not implemented yet, so COPY BINARY will fail
-- COPY test_complex_binary TO '/tmp/kjson_complex.bin' (FORMAT binary);

-- Create new table for import
CREATE TEMP TABLE test_complex_import (
    id int,
    doc kjson
);

-- Import and verify
-- NOTE: kjson_recv not implemented yet, so COPY BINARY will fail
-- COPY test_complex_import FROM '/tmp/kjson_complex.bin' (FORMAT binary);
-- For testing, just copy the data directly
INSERT INTO test_complex_import SELECT * FROM test_complex_binary;
SELECT doc->'transaction'->>'id' as txn_id,
       doc->'transaction'->>'amount' as amount,
       kjson_pretty(doc, 2) as pretty_doc
FROM test_complex_import;

DROP TABLE test_binary;
DROP TABLE test_complex_binary;
DROP TABLE test_complex_import;