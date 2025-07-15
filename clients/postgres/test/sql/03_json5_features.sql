-- Test JSON5 features
\echo '=== Testing JSON5 Features ==='

-- Test unquoted keys
SELECT '{name: "Alice", age: 30}'::kjson;
SELECT '{foo: 1, bar: 2}'::kjson;

-- Test trailing commas
SELECT '[1, 2, 3,]'::kjson;
SELECT '{a: 1, b: 2,}'::kjson;
SELECT '{
    name: "test",
    items: [1, 2, 3,],
}'::kjson;

-- Test single quotes
SELECT '{''name'': ''Alice''}'::kjson;
SELECT '["single", ''quotes'']'::kjson;

-- Test comments (single-line)
SELECT '{
    // This is a comment
    name: "Alice",
    age: 30
}'::kjson;

-- Test comments (multi-line)
SELECT '{
    /* This is a
       multi-line comment */
    name: "Bob",
    /* Another comment */ age: 25
}'::kjson;

-- Test reserved words as unquoted keys
SELECT '{true: 1, false: 2, null: 3}'::kjson;

-- Test mixed JSON5 features
SELECT '{
    // User data
    name: "Charlie",
    tags: [
        "admin",
        "developer",  // trailing comma
    ],
    /* Status flags */
    active: true,
    null: null,  // reserved word as key
}'::kjson;