# Installing kJSON PostgreSQL Extension on macOS

## Prerequisites Check

You have PostgreSQL installed via Homebrew. Let's set up the kJSON extension.

## Step 1: Set Up PostgreSQL Development Tools

First, ensure you have the PostgreSQL development tools in your PATH. Choose your PostgreSQL version:

```bash
# For PostgreSQL 15 (recommended)
export PATH="/opt/homebrew/opt/postgresql@15/bin:$PATH"

# OR for PostgreSQL 14
export PATH="/opt/homebrew/opt/postgresql@14/bin:$PATH"

# Verify pg_config is available
pg_config --version
```

## Step 2: Build the kJSON Extension

```bash
# Navigate to the postgres client directory
cd /Users/kaity/src/trade/mt/kjson/clients/postgres

# Clean any previous builds
make clean

# Build the extension
make

# Install the extension (may require sudo)
make install
```

## Step 3: Create the Extension in PostgreSQL

First, ensure PostgreSQL is running:

```bash
# Start PostgreSQL if not running
brew services start postgresql@15

# Connect to PostgreSQL
psql -U $USER -d postgres
```

In the PostgreSQL prompt:

```sql
-- Create a test database
CREATE DATABASE kjson_test;

-- Connect to the test database
\c kjson_test

-- Create the extension
CREATE EXTENSION kjson;

-- Verify it's installed
\dx kjson
```

## Step 4: Test the Extension

Run some basic tests to verify everything works:

```sql
-- Test basic kJSON parsing
SELECT '{name: "test", value: 42}'::kjson;

-- Test extended types
SELECT '{
    id: 123456789012345678901234567890n,
    price: 99.99m,
    uuid: 550e8400-e29b-41d4-a716-446655440000,
    created: 2025-01-15T10:00:00Z
}'::kjson;

-- Test JSON5 features
SELECT '{
    // This is a comment
    unquoted: "keys",
    singleQuotes: \'allowed\',
    trailingComma: true,
}'::kjson;

-- Create a table with kjson column
CREATE TABLE test_data (
    id serial PRIMARY KEY,
    data kjson
);

-- Insert some data
INSERT INTO test_data (data) VALUES 
    ('{name: "Alice", score: 95.5m}'),
    ('{name: "Bob", score: 87.3m}'),
    ('{name: "Charlie", score: 92.1m}');

-- Query the data
SELECT data->'name' as name, data->'score' as score FROM test_data;

-- Test operators
SELECT data->>'name' as name_text FROM test_data WHERE data->'score' > '90m'::kjson;
```

## Step 5: Run Full Test Suite (Optional)

To run the complete test suite:

```bash
# In the postgres directory
cd /Users/kaity/src/trade/mt/kjson/clients/postgres

# Run the test suite
make installcheck
```

## Troubleshooting

### Error: "pg_config not found"

Make sure PostgreSQL bin directory is in your PATH:
```bash
# For Apple Silicon Macs
export PATH="/opt/homebrew/opt/postgresql@15/bin:$PATH"

# For Intel Macs
export PATH="/usr/local/opt/postgresql@15/bin:$PATH"
```

### Error: "Permission denied" during make install

Use sudo for the install step:
```bash
sudo make install
```

### Error: "could not open extension control file"

This means the extension files weren't installed in the right location. Check the installation path:
```bash
pg_config --sharedir
# Files should be installed in $(pg_config --sharedir)/extension/
```

### Error: "library not loaded" or compilation errors

Make sure you have Xcode command line tools:
```bash
xcode-select --install
```

### UUID Parsing Issue

**Note**: The current C implementation has a known issue with UUIDs starting with digits (like `550e8400-e29b-41d4-a716-446655440000`). These may fail to parse with "Number overflow" errors. UUIDs starting with letters work correctly. This is being addressed to match the JavaScript implementation.

## Alternative: Using Docker

If you prefer not to install directly on your system, you can use the Docker setup:

```bash
# In the postgres directory
cd /Users/kaity/src/trade/mt/kjson/clients/postgres

# Build and run with Docker
docker-compose up -d

# Connect to the containerized PostgreSQL
docker-compose exec postgres psql -U postgres

# The extension is pre-installed in the Docker image
```

## Next Steps

Once installed, you can:

1. Use kJSON for storing complex data with extended types
2. Take advantage of BigInt and Decimal128 for financial data
3. Store UUIDs and timestamps without conversion
4. Use JSON5 syntax for more readable data

## Example Use Case: Financial Data

```sql
CREATE TABLE transactions (
    id serial PRIMARY KEY,
    details kjson
);

INSERT INTO transactions (details) VALUES ('{
    transactionId: f47ac10b-58cc-4372-a567-0e02b2c3d479,
    amount: 12345.67m,
    accountNumber: 9876543210987654321n,
    timestamp: 2025-01-15T14:30:00Z,
    metadata: {
        source: "wire",
        currency: "USD",
        // Pending approval
        status: "pending"
    }
}');

-- Extract specific typed values
SELECT 
    kjson_extract_uuid(details, 'transactionId') as txn_id,
    kjson_extract_numeric(details, 'amount') as amount,
    kjson_extract_timestamp(details, 'timestamp') as when
FROM transactions;
```