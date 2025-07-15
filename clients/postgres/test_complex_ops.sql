-- Test the complex operations that were failing

WITH complex_data AS (
    SELECT kjson_build_object(
        'users', kjson_build_array(
            kjson_build_object('id', 1, 'name', 'Alice', 'active', true),
            kjson_build_object('id', 2, 'name', 'Bob', 'active', false),
            kjson_build_object('id', 3, 'name', 'Charlie', 'active', true)
        ),
        'metadata', kjson_build_object(
            'total', 3,
            'last_updated', '2025-01-15T10:00:00Z'::timestamp
        )
    ) as doc
)
SELECT 
    doc->'users'->0->'name' as first_user,
    doc #> '{users,1,name}' as second_user,
    doc #>> '{metadata,total}' as total_count
FROM complex_data;