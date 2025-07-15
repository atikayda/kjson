-- kjson extension SQL definitions
-- This file creates the kjson type and associated functions

-- Create the type shell
CREATE TYPE kjson;

-- Input function: parses kJSON text and stores as binary
CREATE OR REPLACE FUNCTION kjson_in(cstring)
RETURNS kjson
AS 'MODULE_PATHNAME', 'kjson_in'
LANGUAGE C IMMUTABLE STRICT;

-- Output function: converts binary to kJSON text
CREATE OR REPLACE FUNCTION kjson_out(kjson)
RETURNS cstring
AS 'MODULE_PATHNAME', 'kjson_out'
LANGUAGE C IMMUTABLE STRICT;

-- Binary receive function (for COPY BINARY)
CREATE OR REPLACE FUNCTION kjson_recv(internal)
RETURNS kjson
AS 'MODULE_PATHNAME', 'kjson_recv'
LANGUAGE C IMMUTABLE STRICT;

-- Binary send function (for COPY BINARY)
CREATE OR REPLACE FUNCTION kjson_send(kjson)
RETURNS bytea
AS 'MODULE_PATHNAME', 'kjson_send'
LANGUAGE C IMMUTABLE STRICT;

-- Create the type with I/O functions
CREATE TYPE kjson (
    INPUT = kjson_in,
    OUTPUT = kjson_out,
    RECEIVE = kjson_recv,
    SEND = kjson_send,
    STORAGE = extended,  -- Allow compression for large documents
    ALIGNMENT = int4     -- 4-byte alignment
);

-- Cast from kjson to json
CREATE OR REPLACE FUNCTION kjson_to_json(kjson)
RETURNS json
AS 'MODULE_PATHNAME', 'kjson_to_json'
LANGUAGE C IMMUTABLE STRICT;

CREATE CAST (kjson AS json) WITH FUNCTION kjson_to_json(kjson);

-- Cast from kjson to jsonb
CREATE OR REPLACE FUNCTION kjson_to_jsonb(kjson)
RETURNS jsonb
AS 'MODULE_PATHNAME', 'kjson_to_jsonb'
LANGUAGE C IMMUTABLE STRICT;

CREATE CAST (kjson AS jsonb) WITH FUNCTION kjson_to_jsonb(kjson);

-- Cast from json to kjson
CREATE OR REPLACE FUNCTION json_to_kjson(json)
RETURNS kjson
AS 'MODULE_PATHNAME', 'json_to_kjson'
LANGUAGE C IMMUTABLE STRICT;

CREATE CAST (json AS kjson) WITH FUNCTION json_to_kjson(json);

-- Cast from jsonb to kjson
CREATE OR REPLACE FUNCTION jsonb_to_kjson(jsonb)
RETURNS kjson
AS 'MODULE_PATHNAME', 'jsonb_to_kjson'
LANGUAGE C IMMUTABLE STRICT;

CREATE CAST (jsonb AS kjson) WITH FUNCTION jsonb_to_kjson(jsonb);

-- Cast from text to kjson (explicit cast only)
CREATE OR REPLACE FUNCTION text_to_kjson(text)
RETURNS kjson
AS 'MODULE_PATHNAME', 'text_to_kjson'
LANGUAGE C IMMUTABLE STRICT;

CREATE CAST (text AS kjson) WITH FUNCTION text_to_kjson(text) AS ASSIGNMENT;

-- Operators for kjson

-- Equality
CREATE OR REPLACE FUNCTION kjson_eq(kjson, kjson)
RETURNS boolean
AS 'MODULE_PATHNAME', 'kjson_eq'
LANGUAGE C IMMUTABLE STRICT;

CREATE OPERATOR = (
    LEFTARG = kjson,
    RIGHTARG = kjson,
    FUNCTION = kjson_eq,
    COMMUTATOR = =,
    NEGATOR = <>,
    RESTRICT = eqsel,
    JOIN = eqjoinsel,
    HASHES,
    MERGES
);

-- Not equal
CREATE OR REPLACE FUNCTION kjson_ne(kjson, kjson)
RETURNS boolean
AS 'MODULE_PATHNAME', 'kjson_ne'
LANGUAGE C IMMUTABLE STRICT;

CREATE OPERATOR <> (
    LEFTARG = kjson,
    RIGHTARG = kjson,
    FUNCTION = kjson_ne,
    COMMUTATOR = <>,
    NEGATOR = =,
    RESTRICT = neqsel,
    JOIN = neqjoinsel
);

-- Comparison functions
CREATE OR REPLACE FUNCTION kjson_cmp(kjson, kjson)
RETURNS integer
AS 'MODULE_PATHNAME', 'kjson_cmp'
LANGUAGE C IMMUTABLE STRICT;

CREATE OR REPLACE FUNCTION kjson_lt(kjson, kjson)
RETURNS boolean
AS 'MODULE_PATHNAME', 'kjson_lt'
LANGUAGE C IMMUTABLE STRICT;

CREATE OR REPLACE FUNCTION kjson_le(kjson, kjson)
RETURNS boolean
AS 'MODULE_PATHNAME', 'kjson_le'
LANGUAGE C IMMUTABLE STRICT;

CREATE OR REPLACE FUNCTION kjson_gt(kjson, kjson)
RETURNS boolean
AS 'MODULE_PATHNAME', 'kjson_gt'
LANGUAGE C IMMUTABLE STRICT;

CREATE OR REPLACE FUNCTION kjson_ge(kjson, kjson)
RETURNS boolean
AS 'MODULE_PATHNAME', 'kjson_ge'
LANGUAGE C IMMUTABLE STRICT;

-- Comparison operators
CREATE OPERATOR < (
    LEFTARG = kjson,
    RIGHTARG = kjson,
    FUNCTION = kjson_lt,
    COMMUTATOR = >,
    NEGATOR = >=,
    RESTRICT = scalarltsel,
    JOIN = scalarltjoinsel
);

CREATE OPERATOR <= (
    LEFTARG = kjson,
    RIGHTARG = kjson,
    FUNCTION = kjson_le,
    COMMUTATOR = >=,
    NEGATOR = >,
    RESTRICT = scalarltsel,
    JOIN = scalarltjoinsel
);

CREATE OPERATOR > (
    LEFTARG = kjson,
    RIGHTARG = kjson,
    FUNCTION = kjson_gt,
    COMMUTATOR = <,
    NEGATOR = <=,
    RESTRICT = scalargtsel,
    JOIN = scalargtjoinsel
);

CREATE OPERATOR >= (
    LEFTARG = kjson,
    RIGHTARG = kjson,
    FUNCTION = kjson_ge,
    COMMUTATOR = <=,
    NEGATOR = <,
    RESTRICT = scalargtsel,
    JOIN = scalargtjoinsel
);

-- B-tree operator class for kjson
CREATE OPERATOR CLASS kjson_ops
DEFAULT FOR TYPE kjson USING btree AS
    OPERATOR 1 < ,
    OPERATOR 2 <= ,
    OPERATOR 3 = ,
    OPERATOR 4 >= ,
    OPERATOR 5 > ,
    FUNCTION 1 kjson_cmp(kjson, kjson);

-- Extract operators (like JSONB)

-- Get object field as kjson
CREATE OR REPLACE FUNCTION kjson_object_field(kjson, text)
RETURNS kjson
AS 'MODULE_PATHNAME', 'kjson_object_field'
LANGUAGE C IMMUTABLE STRICT;

CREATE OPERATOR -> (
    LEFTARG = kjson,
    RIGHTARG = text,
    FUNCTION = kjson_object_field
);

-- Get object field as text
CREATE OR REPLACE FUNCTION kjson_object_field_text(kjson, text)
RETURNS text
AS 'MODULE_PATHNAME', 'kjson_object_field_text'
LANGUAGE C IMMUTABLE STRICT;

CREATE OPERATOR ->> (
    LEFTARG = kjson,
    RIGHTARG = text,
    FUNCTION = kjson_object_field_text
);

-- Get array element
CREATE OR REPLACE FUNCTION kjson_array_element(kjson, integer)
RETURNS kjson
AS 'MODULE_PATHNAME', 'kjson_array_element'
LANGUAGE C IMMUTABLE STRICT;

CREATE OPERATOR -> (
    LEFTARG = kjson,
    RIGHTARG = integer,
    FUNCTION = kjson_array_element
);

-- Get array element as text
CREATE OR REPLACE FUNCTION kjson_array_element_text(kjson, integer)
RETURNS text
AS 'MODULE_PATHNAME', 'kjson_array_element_text'
LANGUAGE C IMMUTABLE STRICT;

CREATE OPERATOR ->> (
    LEFTARG = kjson,
    RIGHTARG = integer,
    FUNCTION = kjson_array_element_text
);

-- Utility functions

-- Pretty print with indentation
CREATE OR REPLACE FUNCTION kjson_pretty(kjson, indent integer DEFAULT 2)
RETURNS text
AS 'MODULE_PATHNAME', 'kjson_pretty'
LANGUAGE C IMMUTABLE;

-- Get type of a kjson value
CREATE OR REPLACE FUNCTION kjson_typeof(kjson)
RETURNS text
AS 'MODULE_PATHNAME', 'kjson_typeof'
LANGUAGE C IMMUTABLE STRICT;

-- Extract specific types with proper casting

-- Extract UUID
CREATE OR REPLACE FUNCTION kjson_extract_uuid(kjson, VARIADIC text[])
RETURNS uuid
AS 'MODULE_PATHNAME', 'kjson_extract_uuid'
LANGUAGE C IMMUTABLE;

-- Extract numeric (for BigInt and Decimal128)
CREATE OR REPLACE FUNCTION kjson_extract_numeric(kjson, VARIADIC text[])
RETURNS numeric
AS 'MODULE_PATHNAME', 'kjson_extract_numeric'
LANGUAGE C IMMUTABLE;

-- Extract instant
CREATE OR REPLACE FUNCTION kjson_extract_instant(kjson, VARIADIC text[])
RETURNS timestamptz
AS 'MODULE_PATHNAME', 'kjson_extract_instant'
LANGUAGE C IMMUTABLE;

-- Aggregate functions

-- Build kjson array from values
CREATE OR REPLACE FUNCTION kjson_agg_transfn(internal, anyelement)
RETURNS internal
AS 'MODULE_PATHNAME', 'kjson_agg_transfn'
LANGUAGE C IMMUTABLE;

CREATE OR REPLACE FUNCTION kjson_agg_finalfn(internal)
RETURNS kjson
AS 'MODULE_PATHNAME', 'kjson_agg_finalfn'
LANGUAGE C IMMUTABLE;

CREATE AGGREGATE kjson_agg(anyelement) (
    SFUNC = kjson_agg_transfn,
    STYPE = internal,
    FINALFUNC = kjson_agg_finalfn
);

-- Build kjson object from key/value pairs
CREATE OR REPLACE FUNCTION kjson_object_agg_transfn(internal, text, anyelement)
RETURNS internal
AS 'MODULE_PATHNAME', 'kjson_object_agg_transfn'
LANGUAGE C IMMUTABLE;

CREATE OR REPLACE FUNCTION kjson_object_agg_finalfn(internal)
RETURNS kjson
AS 'MODULE_PATHNAME', 'kjson_object_agg_finalfn'
LANGUAGE C IMMUTABLE;

CREATE AGGREGATE kjson_object_agg(text, anyelement) (
    SFUNC = kjson_object_agg_transfn,
    STYPE = internal,
    FINALFUNC = kjson_object_agg_finalfn
);

-- Index support (GIN indexes like JSONB)

-- GIN support functions for fast existence and containment queries

-- Extract values from kjson for GIN indexing
CREATE OR REPLACE FUNCTION kjson_gin_extract_value(kjson, internal)
RETURNS internal
AS 'MODULE_PATHNAME', 'kjson_gin_extract_value'
LANGUAGE C IMMUTABLE STRICT;

-- Extract query keys from search conditions
CREATE OR REPLACE FUNCTION kjson_gin_extract_query(kjson, internal, int2, internal, internal)
RETURNS internal
AS 'MODULE_PATHNAME', 'kjson_gin_extract_query'
LANGUAGE C IMMUTABLE STRICT;

-- Check consistency between index and query
CREATE OR REPLACE FUNCTION kjson_gin_consistent(internal, int2, kjson, int4, internal, internal)
RETURNS boolean
AS 'MODULE_PATHNAME', 'kjson_gin_consistent'
LANGUAGE C IMMUTABLE STRICT;

-- Partial key comparison for GIN
CREATE OR REPLACE FUNCTION kjson_gin_compare_partial(text, text, int2, internal)
RETURNS int4
AS 'MODULE_PATHNAME', 'kjson_gin_compare_partial'
LANGUAGE C IMMUTABLE STRICT;


-- Builder functions

-- Build kjson object from key/value pairs
CREATE OR REPLACE FUNCTION kjson_build_object(VARIADIC "any")
RETURNS kjson
AS 'MODULE_PATHNAME', 'kjson_build_object'
LANGUAGE C IMMUTABLE;

-- Build kjson array from values
CREATE OR REPLACE FUNCTION kjson_build_array(VARIADIC "any")
RETURNS kjson
AS 'MODULE_PATHNAME', 'kjson_build_array'
LANGUAGE C IMMUTABLE;

-- Convert row to kjson
CREATE OR REPLACE FUNCTION row_to_kjson(record)
RETURNS kjson
AS 'MODULE_PATHNAME', 'row_to_kjson'
LANGUAGE C STABLE STRICT;

-- Path extraction operators

-- Extract kjson at path
CREATE OR REPLACE FUNCTION kjson_extract_path(kjson, VARIADIC text[])
RETURNS kjson
AS 'MODULE_PATHNAME', 'kjson_extract_path'
LANGUAGE C IMMUTABLE;

CREATE OPERATOR #> (
    LEFTARG = kjson,
    RIGHTARG = text[],
    FUNCTION = kjson_extract_path
);

-- Extract text at path
CREATE OR REPLACE FUNCTION kjson_extract_path_text(kjson, VARIADIC text[])
RETURNS text
AS 'MODULE_PATHNAME', 'kjson_extract_path_text'
LANGUAGE C IMMUTABLE;

CREATE OPERATOR #>> (
    LEFTARG = kjson,
    RIGHTARG = text[],
    FUNCTION = kjson_extract_path_text
);

-- Existence operators

-- Check if key exists
CREATE OR REPLACE FUNCTION kjson_exists(kjson, text)
RETURNS boolean
AS 'MODULE_PATHNAME', 'kjson_exists'
LANGUAGE C IMMUTABLE STRICT;

CREATE OPERATOR ? (
    LEFTARG = kjson,
    RIGHTARG = text,
    FUNCTION = kjson_exists
);

-- Check if any keys exist
CREATE OR REPLACE FUNCTION kjson_exists_any(kjson, text[])
RETURNS boolean
AS 'MODULE_PATHNAME', 'kjson_exists_any'
LANGUAGE C IMMUTABLE STRICT;

CREATE OPERATOR ?| (
    LEFTARG = kjson,
    RIGHTARG = text[],
    FUNCTION = kjson_exists_any
);

-- Check if all keys exist
CREATE OR REPLACE FUNCTION kjson_exists_all(kjson, text[])
RETURNS boolean
AS 'MODULE_PATHNAME', 'kjson_exists_all'
LANGUAGE C IMMUTABLE STRICT;

CREATE OPERATOR ?& (
    LEFTARG = kjson,
    RIGHTARG = text[],
    FUNCTION = kjson_exists_all
);

-- Containment operators

-- Contains
CREATE OR REPLACE FUNCTION kjson_contains(kjson, kjson)
RETURNS boolean
AS 'MODULE_PATHNAME', 'kjson_contains'
LANGUAGE C IMMUTABLE STRICT;

CREATE OPERATOR @> (
    LEFTARG = kjson,
    RIGHTARG = kjson,
    FUNCTION = kjson_contains,
    COMMUTATOR = <@
);

-- Contained by
CREATE OR REPLACE FUNCTION kjson_contained(kjson, kjson)
RETURNS boolean
AS 'MODULE_PATHNAME', 'kjson_contained'
LANGUAGE C IMMUTABLE STRICT;

CREATE OPERATOR <@ (
    LEFTARG = kjson,
    RIGHTARG = kjson,
    FUNCTION = kjson_contained,
    COMMUTATOR = @>
);

-- GIN operator class for kjson (must be after all operators are defined)
CREATE OPERATOR CLASS kjson_gin_ops
DEFAULT FOR TYPE kjson USING gin AS
    OPERATOR 7 @> (kjson, kjson),
    OPERATOR 9 ? (kjson, text),
    OPERATOR 10 ?| (kjson, text[]),
    OPERATOR 11 ?& (kjson, text[]),
    FUNCTION 1 kjson_gin_compare_partial(text, text, int2, internal),
    FUNCTION 2 kjson_gin_extract_value(kjson, internal),
    FUNCTION 3 kjson_gin_extract_query(kjson, internal, int2, internal, internal),
    FUNCTION 4 kjson_gin_consistent(internal, int2, kjson, int4, internal, internal),
    STORAGE text;

-- Additional utility functions

-- Get array length
CREATE OR REPLACE FUNCTION kjson_array_length(kjson)
RETURNS integer
AS 'MODULE_PATHNAME', 'kjson_array_length'
LANGUAGE C IMMUTABLE STRICT;

-- Extract array elements as a set
CREATE OR REPLACE FUNCTION kjson_array_elements(kjson)
RETURNS SETOF kjson
AS 'MODULE_PATHNAME', 'kjson_array_elements'
LANGUAGE C IMMUTABLE STRICT;

-- Extract array elements as text
CREATE OR REPLACE FUNCTION kjson_array_elements_text(kjson)
RETURNS SETOF text
AS 'MODULE_PATHNAME', 'kjson_array_elements_text'
LANGUAGE C IMMUTABLE STRICT;

-- Get object keys
CREATE OR REPLACE FUNCTION kjson_object_keys(kjson)
RETURNS SETOF text
AS 'MODULE_PATHNAME', 'kjson_object_keys'
LANGUAGE C IMMUTABLE STRICT;

-- Strip null values
CREATE OR REPLACE FUNCTION kjson_strip_nulls(kjson)
RETURNS kjson
AS 'MODULE_PATHNAME', 'kjson_strip_nulls'
LANGUAGE C IMMUTABLE STRICT;

-- Compact format (no extra spaces or indentation)
CREATE OR REPLACE FUNCTION kjson_compact(kjson)
RETURNS text
AS 'MODULE_PATHNAME', 'kjson_compact'
LANGUAGE C IMMUTABLE;

-- High-precision temporal functions

-- Get current instant with nanosecond precision
CREATE OR REPLACE FUNCTION kjson_now()
RETURNS kjson
AS 'MODULE_PATHNAME', 'kjson_now'
LANGUAGE C STABLE;

-- Create duration from ISO 8601 string
CREATE OR REPLACE FUNCTION kjson_duration(text)
RETURNS kjson
AS 'MODULE_PATHNAME', 'kjson_duration_from_iso'
LANGUAGE C IMMUTABLE STRICT;

-- Add duration to instant
CREATE OR REPLACE FUNCTION kjson_add_duration(kjson, kjson)
RETURNS kjson
AS 'MODULE_PATHNAME', 'kjson_add_duration'
LANGUAGE C IMMUTABLE STRICT;

-- ================================
-- Native High-Precision Temporal Types
-- ================================

-- Create kInstant type
CREATE TYPE kInstant;

-- Input/Output functions for kInstant
CREATE OR REPLACE FUNCTION kinstant_in(cstring)
    RETURNS kInstant
    AS 'MODULE_PATHNAME', 'kinstant_in'
    LANGUAGE C IMMUTABLE STRICT;

CREATE OR REPLACE FUNCTION kinstant_out(kInstant)
    RETURNS cstring
    AS 'MODULE_PATHNAME', 'kinstant_out'
    LANGUAGE C IMMUTABLE STRICT;

-- Binary I/O functions for kInstant
CREATE OR REPLACE FUNCTION kinstant_recv(internal)
    RETURNS kInstant
    AS 'MODULE_PATHNAME', 'kinstant_recv'
    LANGUAGE C IMMUTABLE STRICT;

CREATE OR REPLACE FUNCTION kinstant_send(kInstant)
    RETURNS bytea
    AS 'MODULE_PATHNAME', 'kinstant_send'
    LANGUAGE C IMMUTABLE STRICT;

-- Complete the kInstant type definition
CREATE TYPE kInstant (
    INTERNALLENGTH = 16,
    INPUT = kinstant_in,
    OUTPUT = kinstant_out,
    RECEIVE = kinstant_recv,
    SEND = kinstant_send,
    ALIGNMENT = double,
    STORAGE = plain
);

-- Comparison functions for kInstant
CREATE OR REPLACE FUNCTION kinstant_eq(kInstant, kInstant)
    RETURNS boolean
    AS 'MODULE_PATHNAME', 'kinstant_eq'
    LANGUAGE C IMMUTABLE STRICT;

CREATE OR REPLACE FUNCTION kinstant_ne(kInstant, kInstant)
    RETURNS boolean
    AS 'MODULE_PATHNAME', 'kinstant_ne'
    LANGUAGE C IMMUTABLE STRICT;

CREATE OR REPLACE FUNCTION kinstant_lt(kInstant, kInstant)
    RETURNS boolean
    AS 'MODULE_PATHNAME', 'kinstant_lt'
    LANGUAGE C IMMUTABLE STRICT;

CREATE OR REPLACE FUNCTION kinstant_le(kInstant, kInstant)
    RETURNS boolean
    AS 'MODULE_PATHNAME', 'kinstant_le'
    LANGUAGE C IMMUTABLE STRICT;

CREATE OR REPLACE FUNCTION kinstant_gt(kInstant, kInstant)
    RETURNS boolean
    AS 'MODULE_PATHNAME', 'kinstant_gt'
    LANGUAGE C IMMUTABLE STRICT;

CREATE OR REPLACE FUNCTION kinstant_ge(kInstant, kInstant)
    RETURNS boolean
    AS 'MODULE_PATHNAME', 'kinstant_ge'
    LANGUAGE C IMMUTABLE STRICT;

CREATE OR REPLACE FUNCTION kinstant_cmp(kInstant, kInstant)
    RETURNS integer
    AS 'MODULE_PATHNAME', 'kinstant_cmp'
    LANGUAGE C IMMUTABLE STRICT;

-- Operators for kInstant
CREATE OPERATOR = (
    LEFTARG = kInstant,
    RIGHTARG = kInstant,
    PROCEDURE = kinstant_eq,
    COMMUTATOR = =,
    NEGATOR = <>,
    RESTRICT = eqsel,
    JOIN = eqjoinsel,
    HASHES,
    MERGES
);

CREATE OPERATOR <> (
    LEFTARG = kInstant,
    RIGHTARG = kInstant,
    PROCEDURE = kinstant_ne,
    COMMUTATOR = <>,
    NEGATOR = =,
    RESTRICT = neqsel,
    JOIN = neqjoinsel
);

CREATE OPERATOR < (
    LEFTARG = kInstant,
    RIGHTARG = kInstant,
    PROCEDURE = kinstant_lt,
    COMMUTATOR = >,
    NEGATOR = >=,
    RESTRICT = scalarltsel,
    JOIN = scalarltjoinsel
);

CREATE OPERATOR <= (
    LEFTARG = kInstant,
    RIGHTARG = kInstant,
    PROCEDURE = kinstant_le,
    COMMUTATOR = >=,
    NEGATOR = >,
    RESTRICT = scalarltsel,
    JOIN = scalarltjoinsel
);

CREATE OPERATOR > (
    LEFTARG = kInstant,
    RIGHTARG = kInstant,
    PROCEDURE = kinstant_gt,
    COMMUTATOR = <,
    NEGATOR = <=,
    RESTRICT = scalargtsel,
    JOIN = scalargtjoinsel
);

CREATE OPERATOR >= (
    LEFTARG = kInstant,
    RIGHTARG = kInstant,
    PROCEDURE = kinstant_ge,
    COMMUTATOR = <=,
    NEGATOR = <,
    RESTRICT = scalargtsel,
    JOIN = scalargtjoinsel
);

-- B-tree operator class for kInstant
CREATE OPERATOR CLASS kinstant_ops
    DEFAULT FOR TYPE kInstant USING btree AS
        OPERATOR        1       <,
        OPERATOR        2       <=,
        OPERATOR        3       =,
        OPERATOR        4       >=,
        OPERATOR        5       >,
        FUNCTION        1       kinstant_cmp(kInstant, kInstant);

-- Utility functions for kInstant
CREATE OR REPLACE FUNCTION kinstant_now()
    RETURNS kInstant
    AS 'MODULE_PATHNAME', 'kinstant_now'
    LANGUAGE C VOLATILE;

CREATE OR REPLACE FUNCTION kinstant_from_timestamp(timestamp)
    RETURNS kInstant
    AS 'MODULE_PATHNAME', 'kinstant_from_timestamp'
    LANGUAGE C IMMUTABLE STRICT;

CREATE OR REPLACE FUNCTION kinstant_to_timestamp(kInstant)
    RETURNS timestamp
    AS 'MODULE_PATHNAME', 'kinstant_to_timestamp'
    LANGUAGE C IMMUTABLE STRICT;

-- Cast functions for kInstant
CREATE CAST (timestamp AS kInstant) WITH FUNCTION kinstant_from_timestamp(timestamp);
CREATE CAST (kInstant AS timestamp) WITH FUNCTION kinstant_to_timestamp(kInstant);

-- ================================
-- kDuration type definitions
-- ================================

-- Create kDuration type
CREATE TYPE kDuration;

-- Input/Output functions for kDuration
CREATE OR REPLACE FUNCTION kduration_in(cstring)
    RETURNS kDuration
    AS 'MODULE_PATHNAME', 'kduration_in'
    LANGUAGE C IMMUTABLE STRICT;

CREATE OR REPLACE FUNCTION kduration_out(kDuration)
    RETURNS cstring
    AS 'MODULE_PATHNAME', 'kduration_out'
    LANGUAGE C IMMUTABLE STRICT;

-- Binary I/O functions for kDuration
CREATE OR REPLACE FUNCTION kduration_recv(internal)
    RETURNS kDuration
    AS 'MODULE_PATHNAME', 'kduration_recv'
    LANGUAGE C IMMUTABLE STRICT;

CREATE OR REPLACE FUNCTION kduration_send(kDuration)
    RETURNS bytea
    AS 'MODULE_PATHNAME', 'kduration_send'
    LANGUAGE C IMMUTABLE STRICT;

-- Complete the kDuration type definition
CREATE TYPE kDuration (
    INTERNALLENGTH = 32,
    INPUT = kduration_in,
    OUTPUT = kduration_out,
    RECEIVE = kduration_recv,
    SEND = kduration_send,
    ALIGNMENT = double,
    STORAGE = plain
);

-- Comparison functions for kDuration
CREATE OR REPLACE FUNCTION kduration_eq(kDuration, kDuration)
    RETURNS boolean
    AS 'MODULE_PATHNAME', 'kduration_eq'
    LANGUAGE C IMMUTABLE STRICT;

CREATE OR REPLACE FUNCTION kduration_ne(kDuration, kDuration)
    RETURNS boolean
    AS 'MODULE_PATHNAME', 'kduration_ne'
    LANGUAGE C IMMUTABLE STRICT;

CREATE OR REPLACE FUNCTION kduration_lt(kDuration, kDuration)
    RETURNS boolean
    AS 'MODULE_PATHNAME', 'kduration_lt'
    LANGUAGE C IMMUTABLE STRICT;

CREATE OR REPLACE FUNCTION kduration_le(kDuration, kDuration)
    RETURNS boolean
    AS 'MODULE_PATHNAME', 'kduration_le'
    LANGUAGE C IMMUTABLE STRICT;

CREATE OR REPLACE FUNCTION kduration_gt(kDuration, kDuration)
    RETURNS boolean
    AS 'MODULE_PATHNAME', 'kduration_gt'
    LANGUAGE C IMMUTABLE STRICT;

CREATE OR REPLACE FUNCTION kduration_ge(kDuration, kDuration)
    RETURNS boolean
    AS 'MODULE_PATHNAME', 'kduration_ge'
    LANGUAGE C IMMUTABLE STRICT;

CREATE OR REPLACE FUNCTION kduration_cmp(kDuration, kDuration)
    RETURNS integer
    AS 'MODULE_PATHNAME', 'kduration_cmp'
    LANGUAGE C IMMUTABLE STRICT;

-- Operators for kDuration
CREATE OPERATOR = (
    LEFTARG = kDuration,
    RIGHTARG = kDuration,
    PROCEDURE = kduration_eq,
    COMMUTATOR = =,
    NEGATOR = <>,
    RESTRICT = eqsel,
    JOIN = eqjoinsel,
    HASHES,
    MERGES
);

CREATE OPERATOR <> (
    LEFTARG = kDuration,
    RIGHTARG = kDuration,
    PROCEDURE = kduration_ne,
    COMMUTATOR = <>,
    NEGATOR = =,
    RESTRICT = neqsel,
    JOIN = neqjoinsel
);

CREATE OPERATOR < (
    LEFTARG = kDuration,
    RIGHTARG = kDuration,
    PROCEDURE = kduration_lt,
    COMMUTATOR = >,
    NEGATOR = >=,
    RESTRICT = scalarltsel,
    JOIN = scalarltjoinsel
);

CREATE OPERATOR <= (
    LEFTARG = kDuration,
    RIGHTARG = kDuration,
    PROCEDURE = kduration_le,
    COMMUTATOR = >=,
    NEGATOR = >,
    RESTRICT = scalarltsel,
    JOIN = scalarltjoinsel
);

CREATE OPERATOR > (
    LEFTARG = kDuration,
    RIGHTARG = kDuration,
    PROCEDURE = kduration_gt,
    COMMUTATOR = <,
    NEGATOR = <=,
    RESTRICT = scalargtsel,
    JOIN = scalargtjoinsel
);

CREATE OPERATOR >= (
    LEFTARG = kDuration,
    RIGHTARG = kDuration,
    PROCEDURE = kduration_ge,
    COMMUTATOR = <=,
    NEGATOR = <,
    RESTRICT = scalargtsel,
    JOIN = scalargtjoinsel
);

-- B-tree operator class for kDuration
CREATE OPERATOR CLASS kduration_ops
    DEFAULT FOR TYPE kDuration USING btree AS
        OPERATOR        1       <,
        OPERATOR        2       <=,
        OPERATOR        3       =,
        OPERATOR        4       >=,
        OPERATOR        5       >,
        FUNCTION        1       kduration_cmp(kDuration, kDuration);

-- Arithmetic functions for kDuration
CREATE OR REPLACE FUNCTION kduration_add(kDuration, kDuration)
    RETURNS kDuration
    AS 'MODULE_PATHNAME', 'kduration_add'
    LANGUAGE C IMMUTABLE STRICT;

CREATE OR REPLACE FUNCTION kduration_subtract(kDuration, kDuration)
    RETURNS kDuration
    AS 'MODULE_PATHNAME', 'kduration_subtract'
    LANGUAGE C IMMUTABLE STRICT;

CREATE OR REPLACE FUNCTION kduration_negate(kDuration)
    RETURNS kDuration
    AS 'MODULE_PATHNAME', 'kduration_negate'
    LANGUAGE C IMMUTABLE STRICT;

-- Arithmetic operators for kDuration
CREATE OPERATOR + (
    LEFTARG = kDuration,
    RIGHTARG = kDuration,
    PROCEDURE = kduration_add,
    COMMUTATOR = +
);

CREATE OPERATOR - (
    LEFTARG = kDuration,
    RIGHTARG = kDuration,
    PROCEDURE = kduration_subtract
);

CREATE OPERATOR - (
    RIGHTARG = kDuration,
    PROCEDURE = kduration_negate
);

-- ================================
-- Cross-type operations
-- ================================

-- Function to add kDuration to kInstant
CREATE OR REPLACE FUNCTION kinstant_add_duration(kInstant, kDuration)
    RETURNS kInstant
    AS 'MODULE_PATHNAME', 'kinstant_add_duration'
    LANGUAGE C IMMUTABLE STRICT;

-- Function to subtract kDuration from kInstant
CREATE OR REPLACE FUNCTION kinstant_subtract_duration(kInstant, kDuration)
    RETURNS kInstant
    AS 'MODULE_PATHNAME', 'kinstant_subtract_duration'
    LANGUAGE C IMMUTABLE STRICT;

-- Function to subtract two kInstants to get kDuration
CREATE OR REPLACE FUNCTION kinstant_subtract_instant(kInstant, kInstant)
    RETURNS kDuration
    AS 'MODULE_PATHNAME', 'kinstant_subtract_instant'
    LANGUAGE C IMMUTABLE STRICT;

-- Cross-type operators
CREATE OPERATOR + (
    LEFTARG = kInstant,
    RIGHTARG = kDuration,
    PROCEDURE = kinstant_add_duration,
    COMMUTATOR = +
);

CREATE OPERATOR - (
    LEFTARG = kInstant,
    RIGHTARG = kDuration,
    PROCEDURE = kinstant_subtract_duration
);

CREATE OPERATOR - (
    LEFTARG = kInstant,
    RIGHTARG = kInstant,
    PROCEDURE = kinstant_subtract_instant
);

-- ================================
-- Additional utility functions
-- ================================

-- Extract Unix epoch seconds from kInstant
CREATE OR REPLACE FUNCTION kinstant_extract_epoch(kInstant)
    RETURNS double precision
    AS 'MODULE_PATHNAME', 'kinstant_extract_epoch'
    LANGUAGE C IMMUTABLE STRICT;

-- Create kInstant from Unix epoch seconds
CREATE OR REPLACE FUNCTION kinstant_from_epoch(double precision)
    RETURNS kInstant
    AS 'MODULE_PATHNAME', 'kinstant_from_epoch'
    LANGUAGE C IMMUTABLE STRICT;

-- Convert kDuration to approximate total seconds
CREATE OR REPLACE FUNCTION kduration_to_seconds(kDuration)
    RETURNS double precision
    AS 'MODULE_PATHNAME', 'kduration_to_seconds'
    LANGUAGE C IMMUTABLE STRICT;

-- Create kDuration from total seconds
CREATE OR REPLACE FUNCTION kduration_from_seconds(double precision)
    RETURNS kDuration
    AS 'MODULE_PATHNAME', 'kduration_from_seconds'
    LANGUAGE C IMMUTABLE STRICT;

-- Comments
COMMENT ON TYPE kjson IS 'kJSON data type with extended type support';
COMMENT ON TYPE kInstant IS 'Nanosecond-precision timestamp with timezone offset';
COMMENT ON TYPE kDuration IS 'High-precision duration with ISO 8601 support';
COMMENT ON FUNCTION kjson_pretty(kjson, integer) IS 'Pretty-print kjson with indentation';
COMMENT ON FUNCTION kjson_compact(kjson) IS 'Convert kjson to compact text format';
COMMENT ON FUNCTION kjson_typeof(kjson) IS 'Get the type of a kjson value';
COMMENT ON FUNCTION kjson_extract_uuid(kjson, VARIADIC text[]) IS 'Extract UUID from kjson path';

-- kInstant function comments
COMMENT ON FUNCTION kinstant_now() IS 'Get current timestamp with nanosecond precision';
COMMENT ON FUNCTION kinstant_from_timestamp(timestamp) IS 'Convert PostgreSQL timestamp to kInstant';
COMMENT ON FUNCTION kinstant_to_timestamp(kInstant) IS 'Convert kInstant to PostgreSQL timestamp';
COMMENT ON FUNCTION kinstant_from_epoch(double precision) IS 'Create kInstant from Unix epoch seconds';
COMMENT ON FUNCTION kinstant_extract_epoch(kInstant) IS 'Extract Unix epoch seconds from kInstant';
COMMENT ON FUNCTION kinstant_add_duration(kInstant, kDuration) IS 'Add duration to instant';
COMMENT ON FUNCTION kinstant_subtract_duration(kInstant, kDuration) IS 'Subtract duration from instant';
COMMENT ON FUNCTION kinstant_subtract_instant(kInstant, kInstant) IS 'Subtract two instants to get duration';

-- kDuration function comments
COMMENT ON FUNCTION kduration_in(cstring) IS 'Parse ISO 8601 duration string (e.g. P1Y2M3DT4H5M6.789S)';
COMMENT ON FUNCTION kduration_out(kDuration) IS 'Format duration as ISO 8601 string';
COMMENT ON FUNCTION kduration_add(kDuration, kDuration) IS 'Add two durations';
COMMENT ON FUNCTION kduration_subtract(kDuration, kDuration) IS 'Subtract two durations';
COMMENT ON FUNCTION kduration_negate(kDuration) IS 'Negate a duration';
COMMENT ON FUNCTION kduration_to_seconds(kDuration) IS 'Convert duration to approximate total seconds';
COMMENT ON FUNCTION kduration_from_seconds(double precision) IS 'Create duration from total seconds';
COMMENT ON FUNCTION kjson_extract_numeric(kjson, VARIADIC text[]) IS 'Extract numeric (BigInt/Decimal128) from kjson path';
COMMENT ON FUNCTION kjson_extract_instant(kjson, VARIADIC text[]) IS 'Extract instant from kjson path';
COMMENT ON FUNCTION kjson_build_object(VARIADIC "any") IS 'Build kjson object from key/value pairs';
COMMENT ON FUNCTION kjson_build_array(VARIADIC "any") IS 'Build kjson array from values';
COMMENT ON FUNCTION row_to_kjson(record) IS 'Convert table row to kjson object';
COMMENT ON FUNCTION kjson_array_length(kjson) IS 'Get the length of a kjson array';
COMMENT ON FUNCTION kjson_array_elements(kjson) IS 'Extract array elements as a set of kjson values';
COMMENT ON FUNCTION kjson_array_elements_text(kjson) IS 'Extract array elements as a set of text values';
COMMENT ON FUNCTION kjson_object_keys(kjson) IS 'Get the keys of a kjson object as a set';
COMMENT ON FUNCTION kjson_strip_nulls(kjson) IS 'Remove all null values from kjson';

-- GIN Index Comments
COMMENT ON FUNCTION kjson_gin_extract_value(kjson, internal) IS 'GIN support: extract indexable keys from kjson values';
COMMENT ON FUNCTION kjson_gin_extract_query(kjson, internal, int2, internal, internal) IS 'GIN support: extract query keys for index lookup';
COMMENT ON FUNCTION kjson_gin_consistent(internal, int2, kjson, int4, internal, internal) IS 'GIN support: check index consistency with query';
COMMENT ON FUNCTION kjson_gin_compare_partial(text, text, int2, internal) IS 'GIN support: partial key comparison';
COMMENT ON OPERATOR CLASS kjson_gin_ops USING gin IS 'GIN operator class for fast kjson existence and containment queries';