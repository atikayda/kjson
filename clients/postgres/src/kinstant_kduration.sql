-- kinstant_kduration.sql - SQL definitions for kInstant and kDuration types
--
-- This script creates the kInstant and kDuration types in PostgreSQL
-- with all necessary functions and operators for full functionality.

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

-- ================================
-- Comments and documentation
-- ================================

COMMENT ON TYPE kInstant IS 'Nanosecond-precision timestamp with timezone offset';
COMMENT ON TYPE kDuration IS 'High-precision duration with ISO 8601 support';

COMMENT ON FUNCTION kinstant_now() IS 'Get current timestamp with nanosecond precision';
COMMENT ON FUNCTION kinstant_from_timestamp(timestamp) IS 'Convert PostgreSQL timestamp to kInstant';
COMMENT ON FUNCTION kinstant_to_timestamp(kInstant) IS 'Convert kInstant to PostgreSQL timestamp';

-- Example usage comments
COMMENT ON FUNCTION kduration_in(cstring) IS 'Parse ISO 8601 duration string (e.g. P1Y2M3DT4H5M6.789S)';
COMMENT ON FUNCTION kduration_out(kDuration) IS 'Format duration as ISO 8601 string';