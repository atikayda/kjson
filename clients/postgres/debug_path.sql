-- Debug path extraction issue
\echo '=== Testing path extraction step by step ==='

-- First, verify the object is built correctly
SELECT kjson_build_object('user', kjson_build_object('name', 'Alice', 'age', 30))::text as full_object;

-- Test single level extraction
SELECT kjson_build_object('user', kjson_build_object('name', 'Alice', 'age', 30)) #> ARRAY['user']::text[] as extract_user;

-- Test two level extraction with explicit text array
SELECT kjson_build_object('user', kjson_build_object('name', 'Alice', 'age', 30)) #> ARRAY['user', 'name']::text[] as extract_name;

-- Test with the simpler path notation  
SELECT kjson_build_object('user', kjson_build_object('name', 'Alice', 'age', 30)) #> '{user,name}' as extract_name_simple;

-- Test extracting age
SELECT kjson_build_object('user', kjson_build_object('name', 'Alice', 'age', 30)) #> '{user,age}' as extract_age;

-- Test with array path extraction
SELECT kjson_build_array(1, 2, 3) #> '{0}' as first_element;
SELECT kjson_build_array(1, 2, 3) #> '{1}' as second_element;

-- Test nested array/object
SELECT kjson_build_object('items', kjson_build_array('a', 'b', 'c')) #> '{items,0}' as first_item;