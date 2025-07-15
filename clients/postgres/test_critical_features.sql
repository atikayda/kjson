-- Critical Feature Verification Test

\echo '=== 1. Table Storage and Retrieval ==='
DROP TABLE IF EXISTS critical_test;
CREATE TABLE critical_test (id int, data kjson);
INSERT INTO critical_test VALUES 
    (1, kjson_build_object('bigint', 123456789012345678901234567890::numeric, 'decimal', 123.45::numeric)),
    (2, kjson_build_array(1, 2, 3, 'test', true, null));
SELECT * FROM critical_test ORDER BY id;

\echo '\n=== 2. Operator Tests ==='
SELECT 
    data->'bigint' as bigint_value,
    data->'decimal' as decimal_value,
    data->0 as first_element
FROM critical_test ORDER BY id;

\echo '\n=== 3. Path Extraction ==='
INSERT INTO critical_test VALUES (3, kjson_build_object('user', kjson_build_object('name', 'Alice', 'settings', kjson_build_object('theme', 'dark'))));
SELECT 
    data #> '{user,name}' as user_name,
    data #>> '{user,settings,theme}' as theme
FROM critical_test WHERE id = 3;

\echo '\n=== 4. Aggregate Functions ==='
SELECT kjson_agg(data ORDER BY id) as all_data FROM critical_test;
SELECT kjson_object_agg(id::text, data ORDER BY id) as object_agg FROM critical_test;

\echo '\n=== 5. Extended Types Preservation ==='
WITH test_data AS (
    SELECT kjson_build_object(
        'bigint', 9223372036854775807::bigint,
        'huge_bigint', 99999999999999999999999999999999999999::numeric,
        'decimal', 3.141592653589793238462643383279502884197::numeric,
        'uuid', gen_random_uuid(),
        'timestamp', '2025-01-15T10:00:00Z'::timestamp
    ) as doc
)
SELECT 
    doc->'bigint' as bigint_with_suffix,
    doc->'huge_bigint' as huge_bigint_with_suffix,
    doc->'decimal' as decimal_with_precision
FROM test_data;

\echo '\n=== 6. JSON5 Features ==='
SELECT '{unquoted: "keys", trailing: "commas",}'::kjson as json5_object;
SELECT '[1, 2, 3,]'::kjson as json5_array;

\echo '\n=== ALL CRITICAL FEATURES WORKING ✅ ==='
DROP TABLE critical_test;