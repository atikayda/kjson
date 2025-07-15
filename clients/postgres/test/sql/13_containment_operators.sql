-- Test kJSON containment operators
-- Tests: @>, <@, kjson_contains, kjson_contained_by

\echo '=== Testing containment operators @> and <@ ==='

-- Create test data
CREATE TEMP TABLE test_containment AS
SELECT 
    kjson_build_object('a', 1, 'b', 2, 'c', 3) as obj1,
    kjson_build_object('a', 1, 'b', 2) as obj2,
    kjson_build_object('a', 1) as obj3,
    kjson_build_object('d', 4) as obj4,
    kjson_build_array(1, 2, 3) as arr1,
    kjson_build_array(1, 2) as arr2,
    kjson_build_array(2, 3) as arr3,
    kjson_build_array(1, 2, 3, 4) as arr4;

-- Test object containment with @>
\echo '-- Testing object containment @>'
SELECT obj1 @> obj2 as contains_subset FROM test_containment; -- true
SELECT obj1 @> obj3 as contains_single FROM test_containment; -- true
SELECT obj1 @> obj4 as contains_different FROM test_containment; -- false
SELECT obj2 @> obj1 as subset_contains_superset FROM test_containment; -- false
SELECT obj1 @> obj1 as self_contains FROM test_containment; -- true

-- Test object containment with <@
\echo '-- Testing object contained by <@'
SELECT obj2 <@ obj1 as subset_contained FROM test_containment; -- true
SELECT obj3 <@ obj1 as single_contained FROM test_containment; -- true
SELECT obj4 <@ obj1 as different_contained FROM test_containment; -- false
SELECT obj1 <@ obj2 as superset_contained FROM test_containment; -- false
SELECT obj1 <@ obj1 as self_contained FROM test_containment; -- true

-- Test array containment with @>
\echo '-- Testing array containment @>'
SELECT arr1 @> arr2 as array_contains_subset FROM test_containment; -- true
SELECT arr1 @> arr3 as array_contains_partial FROM test_containment; -- true
SELECT arr4 @> arr1 as larger_contains_smaller FROM test_containment; -- true
SELECT arr2 @> arr1 as smaller_contains_larger FROM test_containment; -- false

-- Test array containment with <@
\echo '-- Testing array contained by <@'
SELECT arr2 <@ arr1 as array_subset_contained FROM test_containment; -- true
SELECT arr3 <@ arr1 as array_partial_contained FROM test_containment; -- true
SELECT arr1 <@ arr4 as smaller_contained_in_larger FROM test_containment; -- true
SELECT arr1 <@ arr2 as larger_contained_in_smaller FROM test_containment; -- false

-- Test mixed type containment
\echo '-- Testing mixed type containment'
SELECT obj1 @> arr1 as object_contains_array FROM test_containment; -- false
SELECT arr1 @> obj1 as array_contains_object FROM test_containment; -- false
SELECT obj1 <@ arr1 as object_contained_in_array FROM test_containment; -- false
SELECT arr1 <@ obj1 as array_contained_in_object FROM test_containment; -- false

-- Test with nested structures
CREATE TEMP TABLE test_nested_containment AS
SELECT 
    kjson_build_object(
        'user', kjson_build_object('id', 1, 'name', 'Alice'),
        'tags', kjson_build_array('admin', 'user')
    ) as full_obj,
    kjson_build_object(
        'user', kjson_build_object('id', 1)
    ) as partial_obj,
    kjson_build_object(
        'user', kjson_build_object('id', 1, 'name', 'Alice')
    ) as user_only;

\echo '-- Testing nested containment'
SELECT full_obj @> partial_obj as contains_partial FROM test_nested_containment; -- true
SELECT full_obj @> user_only as contains_user_only FROM test_nested_containment; -- true
SELECT partial_obj @> full_obj as partial_contains_full FROM test_nested_containment; -- false
SELECT user_only <@ full_obj as user_contained FROM test_nested_containment; -- true

-- Test with null values
\echo '-- Testing containment with nulls'
CREATE TEMP TABLE test_null_containment AS
SELECT 
    kjson_build_object('a', 1, 'b', null, 'c', 3) as obj_with_null,
    kjson_build_object('a', 1, 'b', null) as partial_with_null,
    kjson_build_object('a', 1) as partial_without_null;

SELECT obj_with_null @> partial_with_null as contains_with_null FROM test_null_containment; -- true
SELECT obj_with_null @> partial_without_null as contains_without_null FROM test_null_containment; -- true
SELECT partial_with_null @> partial_without_null as null_contains_without FROM test_null_containment; -- false

-- Test with scalar values
\echo '-- Testing scalar containment'
CREATE TEMP TABLE test_scalars AS
SELECT 
    '123'::kjson as num1,
    '123'::kjson as num2,
    '456'::kjson as num3,
    '"hello"'::kjson as str1,
    '"hello"'::kjson as str2,
    '"world"'::kjson as str3,
    'true'::kjson as bool1,
    'false'::kjson as bool2;

SELECT num1 @> num2 as same_number FROM test_scalars; -- true
SELECT num1 @> num3 as different_number FROM test_scalars; -- false
SELECT str1 @> str2 as same_string FROM test_scalars; -- true
SELECT str1 @> str3 as different_string FROM test_scalars; -- false
SELECT bool1 @> bool1 as same_bool FROM test_scalars; -- true
SELECT bool1 @> bool2 as different_bool FROM test_scalars; -- false

-- Test with extended types
\echo '-- Testing extended type containment'
CREATE TEMP TABLE test_extended_containment AS
SELECT 
    kjson_build_object(
        'bignum', kjson_build_object('value', 123456789012345678901234567890::numeric),
        'uuid', 'a0eebc99-9c0b-4ef8-bb6d-6bb9bd380a11'::uuid,
        'date', '2025-01-15T10:00:00Z'::timestamp
    ) as full_extended,
    kjson_build_object(
        'bignum', kjson_build_object('value', 123456789012345678901234567890::numeric)
    ) as partial_extended;

SELECT full_extended @> partial_extended as contains_bignum FROM test_extended_containment; -- true
SELECT partial_extended <@ full_extended as bignum_contained FROM test_extended_containment; -- true

-- Clean up
DROP TABLE test_containment;
DROP TABLE test_nested_containment;
DROP TABLE test_null_containment;
DROP TABLE test_scalars;
DROP TABLE test_extended_containment;