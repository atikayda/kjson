#!/bin/bash
# Script to run kJSON PostgreSQL extension tests

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo "🧪 kJSON PostgreSQL Extension Test Suite"
echo "========================================"
echo ""

# Check if Docker is running
if ! docker info > /dev/null 2>&1; then
    echo -e "${RED}❌ Docker is not running. Please start Docker first.${NC}"
    exit 1
fi

# Clean up any existing containers
echo "🧹 Cleaning up existing containers..."
make -f Makefile.test test-clean > /dev/null 2>&1 || true

# Build the test containers
echo "🔨 Building test containers..."
if ! make -f Makefile.test test-build; then
    echo -e "${RED}❌ Failed to build test containers${NC}"
    exit 1
fi

echo ""
echo "🚀 Running tests..."
echo ""

# Run the tests
if make -f Makefile.test test-run; then
    echo ""
    echo -e "${GREEN}✅ All tests passed!${NC}"
    exit 0
else
    echo ""
    echo -e "${RED}❌ Tests failed${NC}"
    echo ""
    echo "To debug, you can:"
    echo "  1. View logs: make -f Makefile.test test-logs"
    echo "  2. Get a PostgreSQL shell: make -f Makefile.test test-shell"
    echo "  3. Run specific SQL test: make -f Makefile.test test-sql FILE=01_basic_types.sql"
    echo "  4. Run Python tests only: make -f Makefile.test test-python"
    exit 1
fi