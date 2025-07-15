# kInstant and kDuration PostgreSQL Types

This document describes the native PostgreSQL types `kInstant` and `kDuration` which provide nanosecond-precision temporal functionality.

## Overview

These types offer significant advantages over storing temporal data as kjson:

- **Direct column storage**: Use directly in table columns without JSON overhead
- **Nanosecond precision**: Full nanosecond precision for high-frequency data
- **Native indexing**: Full B-tree index support for efficient queries
- **Type safety**: PostgreSQL type system ensures correctness
- **Performance**: Faster than parsing JSON for temporal operations

## Installation

1. Compile the extension:
```bash
make -f Makefile.kinstant_kduration
sudo make -f Makefile.kinstant_kduration install
```

2. Create the extension in your database:
```sql
CREATE EXTENSION kinstant_kduration;
```

## kInstant Type

The `kInstant` type represents a nanosecond-precision timestamp with timezone offset.

### Basic Usage

```sql
-- Create a table with kInstant column
CREATE TABLE events (
    id SERIAL PRIMARY KEY,
    name TEXT NOT NULL,
    timestamp kInstant NOT NULL,
    created_at kInstant DEFAULT kinstant_now()
);

-- Insert data with various timestamp formats
INSERT INTO events (name, timestamp) VALUES
    ('Event 1', '2023-12-25T10:30:45.123456789Z'),
    ('Event 2', '2023-12-25T10:30:45.123456789+05:30'),
    ('Event 3', '2023-12-25T10:30:45.123456789-08:00');

-- Query with timestamp comparisons
SELECT * FROM events 
WHERE timestamp > '2023-12-25T10:00:00Z'
ORDER BY timestamp;

-- Get current nanosecond-precision timestamp
SELECT kinstant_now();
```

### Conversion Functions

```sql
-- Convert between kInstant and PostgreSQL timestamp
SELECT kinstant_from_timestamp(NOW());
SELECT kinstant_to_timestamp('2023-12-25T10:30:45.123456789Z'::kInstant);

-- Convert to/from Unix epoch seconds
SELECT kinstant_from_epoch(1703502645.123456789);
SELECT kinstant_extract_epoch('2023-12-25T10:30:45.123456789Z'::kInstant);
```

### Indexing

```sql
-- Create indexes for efficient queries
CREATE INDEX idx_events_timestamp ON events USING btree(timestamp);
CREATE INDEX idx_events_created_at ON events USING btree(created_at);

-- Range queries will use the index
SELECT * FROM events 
WHERE timestamp BETWEEN '2023-12-01T00:00:00Z'::kInstant 
                   AND '2023-12-31T23:59:59.999999999Z'::kInstant;
```

## kDuration Type

The `kDuration` type represents a high-precision duration with ISO 8601 support.

### Basic Usage

```sql
-- Create a table with kDuration column
CREATE TABLE tasks (
    id SERIAL PRIMARY KEY,
    name TEXT NOT NULL,
    duration kDuration NOT NULL,
    max_duration kDuration DEFAULT 'PT1H'::kDuration
);

-- Insert data with various duration formats
INSERT INTO tasks (name, duration) VALUES
    ('Quick task', 'PT5M30.123456789S'),
    ('Daily task', 'P1DT2H30M'),
    ('Weekly task', 'P1W'),
    ('Monthly task', 'P1M'),
    ('Complex task', 'P1Y2M3DT4H5M6.789S');

-- Query with duration comparisons
SELECT * FROM tasks 
WHERE duration > 'PT1H'::kDuration
ORDER BY duration;
```

### Arithmetic Operations

```sql
-- Add durations
SELECT 'PT1H'::kDuration + 'PT30M'::kDuration; -- Returns PT1H30M

-- Subtract durations
SELECT 'PT2H'::kDuration - 'PT30M'::kDuration; -- Returns PT1H30M

-- Negate duration
SELECT -'PT1H'::kDuration; -- Returns -PT1H

-- Convert to/from seconds
SELECT kduration_to_seconds('PT1H30M'::kDuration); -- Returns 5400.0
SELECT kduration_from_seconds(5400.0); -- Returns PT1H30M
```

## Cross-Type Operations

Combine kInstant and kDuration for powerful temporal arithmetic:

```sql
-- Add duration to instant
SELECT kinstant_now() + 'PT1H'::kDuration;

-- Subtract duration from instant
SELECT kinstant_now() - 'PT30M'::kDuration;

-- Calculate duration between two instants
SELECT 
    '2023-12-25T12:00:00Z'::kInstant - 
    '2023-12-25T10:30:00Z'::kInstant AS time_diff;
```

## Advanced Examples

### High-Frequency Trading Data

```sql
-- Table for nanosecond-precision trading data
CREATE TABLE trades (
    id BIGSERIAL PRIMARY KEY,
    symbol TEXT NOT NULL,
    price NUMERIC(20,8) NOT NULL,
    volume NUMERIC(20,8) NOT NULL,
    timestamp kInstant NOT NULL,
    processing_time kDuration NOT NULL
);

-- Index for time-based queries
CREATE INDEX idx_trades_timestamp ON trades USING btree(timestamp);
CREATE INDEX idx_trades_symbol_timestamp ON trades USING btree(symbol, timestamp);

-- Insert high-frequency data
INSERT INTO trades (symbol, price, volume, timestamp, processing_time) VALUES
    ('BTCUSDT', 50000.12345678, 1.23456789, kinstant_now(), 'PT0.000001S'),
    ('ETHUSDT', 3000.87654321, 2.34567890, kinstant_now(), 'PT0.000002S');

-- Query trades within a microsecond window
SELECT * FROM trades 
WHERE timestamp BETWEEN 
    kinstant_now() - 'PT0.001S'::kDuration 
    AND kinstant_now();

-- Calculate average processing time
SELECT 
    symbol,
    AVG(kduration_to_seconds(processing_time)) * 1000000 AS avg_processing_us
FROM trades 
GROUP BY symbol;
```

### Event Scheduling

```sql
-- Table for scheduled events
CREATE TABLE scheduled_events (
    id SERIAL PRIMARY KEY,
    name TEXT NOT NULL,
    scheduled_at kInstant NOT NULL,
    estimated_duration kDuration NOT NULL,
    actual_duration kDuration,
    status TEXT DEFAULT 'pending'
);

-- Schedule events
INSERT INTO scheduled_events (name, scheduled_at, estimated_duration) VALUES
    ('Backup', kinstant_now() + 'PT1H'::kDuration, 'PT30M'),
    ('Maintenance', kinstant_now() + 'P1D'::kDuration, 'PT2H');

-- Find events due within next hour
SELECT * FROM scheduled_events 
WHERE scheduled_at <= kinstant_now() + 'PT1H'::kDuration
  AND status = 'pending';

-- Calculate estimated completion time
SELECT 
    name,
    scheduled_at,
    scheduled_at + estimated_duration AS estimated_completion
FROM scheduled_events
WHERE status = 'pending';
```

### Performance Monitoring

```sql
-- Table for performance metrics
CREATE TABLE performance_metrics (
    id SERIAL PRIMARY KEY,
    operation TEXT NOT NULL,
    timestamp kInstant NOT NULL,
    duration kDuration NOT NULL,
    success BOOLEAN NOT NULL
);

-- Record performance data
INSERT INTO performance_metrics (operation, timestamp, duration, success) VALUES
    ('database_query', kinstant_now(), 'PT0.045S', true),
    ('api_call', kinstant_now(), 'PT0.123S', true),
    ('file_processing', kinstant_now(), 'PT2.456S', false);

-- Calculate performance statistics
SELECT 
    operation,
    COUNT(*) as total_calls,
    AVG(kduration_to_seconds(duration)) as avg_duration_seconds,
    MAX(kduration_to_seconds(duration)) as max_duration_seconds,
    MIN(kduration_to_seconds(duration)) as min_duration_seconds,
    SUM(CASE WHEN success THEN 1 ELSE 0 END)::float / COUNT(*) as success_rate
FROM performance_metrics
WHERE timestamp >= kinstant_now() - 'PT24H'::kDuration
GROUP BY operation;
```

## Type Specifications

### kInstant

- **Size**: 16 bytes
- **Range**: 292 billion years before/after Unix epoch
- **Precision**: Nanoseconds
- **Timezone**: Offset in minutes from UTC
- **Format**: ISO 8601 with nanosecond precision

### kDuration

- **Size**: 32 bytes  
- **Components**: Years, months, days, hours, minutes, nanoseconds
- **Range**: ±2 billion years in each component
- **Precision**: Nanoseconds for sub-minute precision
- **Format**: ISO 8601 duration format

## Best Practices

1. **Use kInstant for absolute timestamps** requiring nanosecond precision
2. **Use kDuration for time intervals** and elapsed time measurements
3. **Create indexes** on kInstant columns for time-based queries
4. **Consider timezone handling** - kInstant stores offset but comparison is in UTC
5. **Use appropriate precision** - nanoseconds are suitable for high-frequency data
6. **Batch operations** when possible to minimize overhead

## Migration from kjson

To migrate from storing temporal data as kjson:

```sql
-- Old schema with kjson
CREATE TABLE old_events (
    id SERIAL PRIMARY KEY,
    data kjson NOT NULL
);

-- New schema with native types
CREATE TABLE new_events (
    id SERIAL PRIMARY KEY,
    name TEXT NOT NULL,
    timestamp kInstant NOT NULL,
    duration kDuration
);

-- Migration query
INSERT INTO new_events (id, name, timestamp, duration)
SELECT 
    id,
    data->>'name' as name,
    (data->>'timestamp')::kInstant as timestamp,
    (data->>'duration')::kDuration as duration
FROM old_events;
```

The native types provide significant performance and usability benefits over storing temporal data as kjson while maintaining full precision and functionality.