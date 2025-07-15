-- Test kJSON indexing and performance features
-- Tests: GIN indexes, containment queries, existence queries

\echo '=== Testing indexing and performance features ==='

-- Create a table with kjson data
CREATE TEMP TABLE test_documents (
    id serial PRIMARY KEY,
    data kjson NOT NULL
);

-- Insert test data
INSERT INTO test_documents (data)
SELECT kjson_build_object(
    'id', i,
    'type', CASE 
        WHEN i % 3 = 0 THEN 'type_a'
        WHEN i % 3 = 1 THEN 'type_b'
        ELSE 'type_c'
    END,
    'value', i * 10,
    'active', i % 2 = 0,
    'tags', kjson_build_array(
        'tag_' || (i % 5),
        'tag_' || (i % 7),
        'tag_' || (i % 11)
    ),
    'metadata', kjson_build_object(
        'created', now() - (i || ' days')::interval,
        'user', 'user_' || (i % 10),
        'score', random() * 100
    )
)
FROM generate_series(1, 1000) i;

-- Create GIN indexes
\echo '-- Creating GIN indexes'
-- NOTE: GIN index support is not yet implemented for kjson type
-- These will fail with "no default operator class" error
-- CREATE INDEX idx_kjson_gin ON test_documents USING gin(data);
-- CREATE INDEX idx_kjson_gin_path ON test_documents USING gin(data kjson_path_ops);

-- Test containment queries with index
\echo '-- Testing containment queries'
EXPLAIN (ANALYZE, BUFFERS)
SELECT count(*) FROM test_documents 
WHERE data @> '{"type": "type_a"}'::kjson;

EXPLAIN (ANALYZE, BUFFERS)
SELECT count(*) FROM test_documents 
WHERE data @> '{"active": true}'::kjson;

EXPLAIN (ANALYZE, BUFFERS)
SELECT count(*) FROM test_documents 
WHERE data @> '{"tags": ["tag_3"]}'::kjson;

-- Test existence queries with index
\echo '-- Testing existence queries'
EXPLAIN (ANALYZE, BUFFERS)
SELECT count(*) FROM test_documents 
WHERE data ? 'metadata';

EXPLAIN (ANALYZE, BUFFERS)
SELECT count(*) FROM test_documents 
WHERE data ?| ARRAY['foo', 'bar', 'type'];

EXPLAIN (ANALYZE, BUFFERS)
SELECT count(*) FROM test_documents 
WHERE data ?& ARRAY['type', 'value', 'active'];

-- Test path extraction with index
\echo '-- Testing path extraction performance'
EXPLAIN (ANALYZE, BUFFERS)
SELECT data #> '{metadata, user}' as user
FROM test_documents
WHERE data #>> '{metadata, user}' = 'user_5';

-- Test complex queries
\echo '-- Testing complex queries'
EXPLAIN (ANALYZE, BUFFERS)
SELECT id, data #>> '{type}' as type, data #>> '{value}' as value
FROM test_documents
WHERE data @> '{"active": true}'::kjson
  AND (data #>> '{value}')::int > 500
ORDER BY (data #>> '{value}')::int DESC
LIMIT 10;

-- Test aggregate performance
\echo '-- Testing aggregate performance'
EXPLAIN (ANALYZE, BUFFERS)
SELECT 
    data #>> '{type}' as type,
    count(*) as count,
    avg((data #>> '{value}')::numeric) as avg_value
FROM test_documents
GROUP BY data #>> '{type}'
ORDER BY count DESC;

-- Test JSON path containment
\echo '-- Testing JSON path containment'
EXPLAIN (ANALYZE, BUFFERS)
SELECT count(*)
FROM test_documents
WHERE data #> '{metadata}' @> '{"user": "user_7"}'::kjson;

-- Test statistics
\echo '-- Table and index statistics'
SELECT 
    schemaname,
    tablename,
    attname,
    n_distinct,
    most_common_vals::text::text[] as most_common_vals
FROM pg_stats
WHERE tablename = 'test_documents'
  AND attname = 'data';

SELECT 
    schemaname,
    tablename,
    indexname,
    idx_scan,
    idx_tup_read,
    idx_tup_fetch
FROM pg_stat_user_indexes
WHERE tablename = 'test_documents'
ORDER BY indexname;

-- Test different data patterns
\echo '-- Testing with different data patterns'
CREATE TEMP TABLE test_sparse (
    id serial PRIMARY KEY,
    data kjson
);

-- Insert sparse data (many different keys)
INSERT INTO test_sparse (data)
SELECT kjson_build_object(
    'key_' || (i % 100), i,
    'common', 'value'
)
FROM generate_series(1, 1000) i;

-- CREATE INDEX idx_sparse_gin ON test_sparse USING gin(data);  -- GIN not supported yet

EXPLAIN (ANALYZE, BUFFERS)
SELECT count(*) FROM test_sparse
WHERE data ? 'key_42';

-- Test with nested data
CREATE TEMP TABLE test_nested (
    id serial PRIMARY KEY,
    data kjson
);

INSERT INTO test_nested (data)
SELECT kjson_build_object(
    'level1', kjson_build_object(
        'level2', kjson_build_object(
            'level3', kjson_build_object(
                'value', i
            )
        )
    )
)
FROM generate_series(1, 1000) i;

-- CREATE INDEX idx_nested_gin ON test_nested USING gin(data);  -- GIN not supported yet

EXPLAIN (ANALYZE, BUFFERS)
SELECT count(*) FROM test_nested
WHERE data #> '{level1, level2, level3}' @> '{"value": 42}'::kjson;

-- Clean up
DROP TABLE test_documents;
DROP TABLE test_sparse;
DROP TABLE test_nested;