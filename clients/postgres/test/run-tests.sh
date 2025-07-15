#\!/bin/bash
# Test runner script executed inside the container

set -e

echo "=== kJSON PostgreSQL Extension Test Suite ==="
echo ""

# Wait for PostgreSQL to be ready
echo "Waiting for PostgreSQL to be ready..."
for i in {1..30}; do
    if pg_isready -h $PGHOST -p $PGPORT -U $PGUSER >/dev/null 2>&1; then
        echo "PostgreSQL is ready\!"
        break
    fi
    echo -n "."
    sleep 1
done
echo ""

# Create extension as superuser
echo "Creating kJSON extension..."
PGPASSWORD=postgres psql -h $PGHOST -p $PGPORT -U postgres -d $PGDATABASE -c "CREATE EXTENSION IF NOT EXISTS kjson"

# Run SQL tests
echo ""
echo "Running SQL tests..."
for test_file in /test/sql/*.sql; do
    if [ -f "$test_file" ]; then
        echo ""
        echo "Running $(basename $test_file)..."
        PGPASSWORD=$PGPASSWORD psql -h $PGHOST -p $PGPORT -U $PGUSER -d $PGDATABASE -f "$test_file" || exit 1
    fi
done

# Run Python tests
echo ""
echo "Running Python integration tests..."
cd /test
python3 -m pytest test_kjson.py -v || exit 1

echo ""
echo "=== All tests passed\! ==="
