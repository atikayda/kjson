# kJSON PostgreSQL Extension Test Suite

Comprehensive test suite for the kJSON PostgreSQL extension, using Docker for isolation and reproducibility.

## Quick Start

The easiest way to run all tests:

```bash
./run-tests.sh
```

This script will:
1. Check Docker is running
2. Clean up any existing containers
3. Build the test containers
4. Run all SQL and Python tests
5. Report results

## Manual Test Commands

For more control, use the Makefile:

```bash
# Run all tests (build + run)
make -f Makefile.test test

# Build containers without running tests
make -f Makefile.test test-build

# Run tests without rebuilding
make -f Makefile.test test-run

# Clean up all test containers and volumes
make -f Makefile.test test-clean
```

## Interactive Testing

### PostgreSQL Shell Access

Get a PostgreSQL shell for manual testing:

```bash
make -f Makefile.test test-shell
```

This starts PostgreSQL and gives you a `psql` prompt where you can:
- Test kJSON functions interactively
- Debug specific issues
- Explore the extension capabilities

### View Test Logs

Monitor test execution in real-time:

```bash
make -f Makefile.test test-logs
```

## Running Specific Tests

### SQL Tests

Run a specific SQL test file:

```bash
make -f Makefile.test test-sql FILE=01_basic_types.sql
```

Available SQL test files:
- `01_basic_types.sql` - Basic JSON types (null, boolean, number, string, array, object)
- `02_extended_types.sql` - kJSON extended types (BigInt, Decimal128, UUID, Date)
- `03_json5_features.sql` - JSON5 syntax (unquoted keys, trailing commas, comments)
- `04_operators.sql` - kJSON operators (→, →>, =, <>)
- `05_casts.sql` - Type casting (kjson ↔ json/jsonb)
- `06_utility_functions.sql` - Utility functions (pretty print, typeof, extract)
- `07_aggregates.sql` - Aggregate functions (kjson_agg, kjson_object_agg)
- `08_binary_format.sql` - Binary format COPY support

### Python Tests

Run only the Python integration tests:

```bash
make -f Makefile.test test-python
```

## Test Architecture

### Docker Setup

The test suite uses Docker Compose with:
- **postgres-kjson**: PostgreSQL 15 with kJSON extension
  - Built from source in the container
  - Runs on port 5433 (to avoid conflicts)
  - Includes C library and PostgreSQL extension
- **test-runner**: Python environment for running tests
  - Includes psycopg2 and pytest
  - Runs SQL scripts and Python tests

### Test Structure

```
test/
├── sql/                    # SQL test scripts
│   ├── 01_basic_types.sql
│   ├── 02_extended_types.sql
│   ├── 03_json5_features.sql
│   ├── 04_operators.sql
│   ├── 05_casts.sql
│   ├── 06_utility_functions.sql
│   ├── 07_aggregates.sql
│   └── 08_binary_format.sql
├── test_kjson.py          # Python integration tests
└── README.md              # This file
```

### Test Coverage

The test suite covers:
- **Parser**: All kJSON syntax including JSON5 features
- **Types**: BigInt (n suffix), Decimal128 (m suffix), UUID, Date
- **Operators**: →, →>, =, <> with nested access
- **Functions**: pretty print, typeof, extract functions
- **Aggregates**: kjson_agg, kjson_object_agg
- **Casts**: Bidirectional casting with json/jsonb
- **Binary**: PostgreSQL binary format support
- **Performance**: Large dataset handling
- **Error Handling**: Invalid input detection

## Debugging Failed Tests

If tests fail:

1. **Check logs**: `make -f Makefile.test test-logs`
2. **Get shell access**: `make -f Makefile.test test-shell`
3. **Run specific test**: `make -f Makefile.test test-sql FILE=failing_test.sql`
4. **Check extension**: `make -f Makefile.test test-check`

Common issues:
- **Build failures**: Check C compiler errors in logs
- **Extension not found**: Verify installation with `\dx kjson`
- **Function missing**: List functions with `\df kjson_*`
- **Type errors**: Check with `SELECT kjson_typeof(...)`

## Development Workflow

When developing new features:

1. Add SQL test cases to appropriate test file
2. Add Python test methods to `test_kjson.py`
3. Run tests: `./run-tests.sh`
4. Debug failures with shell access
5. Iterate until all tests pass

## Environment Variables

The test suite uses these environment variables (set automatically by Docker):
- `PGHOST`: postgres-kjson
- `PGPORT`: 5432 (internal), 5433 (external)
- `PGUSER`: kjson_test
- `PGPASSWORD`: kjson_test
- `PGDATABASE`: kjson_test

## Continuous Integration

The test suite is designed to work in CI/CD pipelines:

```bash
# CI-friendly command (exits with proper code)
./run-tests.sh
```

Exit codes:
- 0: All tests passed
- 1: Test failures or setup issues

## Requirements

- Docker Desktop or Docker Engine
- Docker Compose (usually included)
- 2GB+ free disk space for images
- Port 5433 available (or modify docker-compose.yml)

## Troubleshooting

### Docker not running
```
❌ Docker is not running. Please start Docker first.
```
→ Start Docker Desktop or Docker daemon

### Port conflict
```
Error: bind: address already in use
```
→ Change port in docker-compose.yml or stop conflicting service

### Build failures
```
❌ Failed to build test containers
```
→ Check Docker logs, ensure internet connection for package downloads

### Test failures
```
❌ Tests failed
```
→ Follow debugging steps above to identify specific failure