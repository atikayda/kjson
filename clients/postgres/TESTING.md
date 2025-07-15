# Testing the kJSON PostgreSQL Extension

This document provides instructions for testing the kJSON PostgreSQL extension.

## Quick Start

The simplest way to run all tests:

```bash
./run-tests.sh
```

This will:
1. Build the Docker containers
2. Compile the C library and PostgreSQL extension
3. Start PostgreSQL on port 5433
4. Run all SQL and Python tests
5. Report results

## Test Structure

The test suite consists of:

### SQL Tests (`test/sql/`)
1. **01_basic_types.sql** - Basic JSON types (null, bool, number, string, array, object)
2. **02_extended_types.sql** - kJSON extended types (BigInt, Decimal128, UUID, Date)
3. **03_json5_features.sql** - JSON5 syntax (unquoted keys, comments, trailing commas)
4. **04_operators.sql** - Operators (→, →>, =, <>)
5. **05_casts.sql** - Type casting between kjson and json/jsonb
6. **06_utility_functions.sql** - Utility functions (pretty, typeof, extract)
7. **07_aggregates.sql** - Aggregate functions (kjson_agg, kjson_object_agg)
8. **08_binary_format.sql** - Binary COPY format support

### Python Tests (`test/test_kjson.py`)
- Comprehensive integration tests using pytest
- Tests all functionality programmatically
- Includes performance and error handling tests

## Manual Testing

### Get a PostgreSQL Shell

For interactive testing:

```bash
make -f Makefile.test test-shell
```

This gives you a `psql` prompt where you can:

```sql
-- Check extension
\dx kjson

-- List functions
\df kjson_*

-- Try queries
SELECT '{"amount": 123n}'::kjson;
SELECT kjson_pretty('{"a": 1, "b": 2}'::kjson, 2);
```

### Run Specific Tests

Run a single SQL test file:

```bash
make -f Makefile.test test-sql FILE=02_extended_types.sql
```

Run only Python tests:

```bash
make -f Makefile.test test-python
```

### View Logs

Monitor test execution:

```bash
make -f Makefile.test test-logs
```

## Test Environment

- **PostgreSQL**: Version 15 running on port 5433
- **Database**: kjson_test
- **User**: kjson_test / kjson_test
- **Extension**: Automatically created in test database

## Troubleshooting

### Build Failures

If the build fails, check:
1. Docker is running
2. You have internet connection (for package downloads)
3. No syntax errors in C code

### Test Failures

For failing tests:
1. Check which test failed in the output
2. Get a shell: `make -f Makefile.test test-shell`
3. Run the failing query manually
4. Check logs: `make -f Makefile.test test-logs`

### Common Issues

**Extension not found**:
```sql
CREATE EXTENSION kjson;
-- ERROR: could not open extension control file
```
→ Build may have failed, check logs

**Function not found**:
```sql
SELECT kjson_pretty(...);
-- ERROR: function kjson_pretty does not exist
```
→ Extension not installed, run `CREATE EXTENSION kjson;`

**Type errors**:
```sql
SELECT '{"bad": json}'::kjson;
-- ERROR: invalid kJSON value
```
→ Check syntax, use `kjson_typeof()` to debug

## Clean Up

Remove all test containers and data:

```bash
make -f Makefile.test test-clean
```

## Continuous Integration

The test script returns proper exit codes:
- 0 = All tests passed
- 1 = Test failures or setup issues

Perfect for CI/CD pipelines:

```yaml
# Example GitHub Actions
- name: Test kJSON PostgreSQL Extension
  run: ./run-tests.sh
```