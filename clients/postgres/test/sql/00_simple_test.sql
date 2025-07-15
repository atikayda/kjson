-- Very simple test to isolate the issue
\echo '=== Simple Test ==='

-- Test extension is loaded
SELECT extname, extversion FROM pg_extension WHERE extname = 'kjson';

-- Try the simplest possible case
DO $$
BEGIN
    PERFORM 'null'::kjson;
    RAISE NOTICE 'Simple cast succeeded';
EXCEPTION WHEN OTHERS THEN
    RAISE NOTICE 'Simple cast failed: %', SQLERRM;
END $$;