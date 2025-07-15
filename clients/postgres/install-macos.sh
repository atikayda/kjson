#!/bin/bash
# Quick installation script for kJSON PostgreSQL extension on macOS

set -e

echo "🚀 kJSON PostgreSQL Extension Installer for macOS"
echo "================================================"

# Detect PostgreSQL installation
if command -v /opt/homebrew/opt/postgresql@15/bin/pg_config &> /dev/null; then
    echo "✅ Found PostgreSQL 15 (Homebrew ARM64)"
    export PATH="/opt/homebrew/opt/postgresql@15/bin:$PATH"
elif command -v /opt/homebrew/opt/postgresql@14/bin/pg_config &> /dev/null; then
    echo "✅ Found PostgreSQL 14 (Homebrew ARM64)"
    export PATH="/opt/homebrew/opt/postgresql@14/bin:$PATH"
elif command -v /usr/local/opt/postgresql@15/bin/pg_config &> /dev/null; then
    echo "✅ Found PostgreSQL 15 (Homebrew Intel)"
    export PATH="/usr/local/opt/postgresql@15/bin:$PATH"
elif command -v /usr/local/opt/postgresql@14/bin/pg_config &> /dev/null; then
    echo "✅ Found PostgreSQL 14 (Homebrew Intel)"
    export PATH="/usr/local/opt/postgresql@14/bin:$PATH"
elif command -v pg_config &> /dev/null; then
    echo "✅ Found PostgreSQL in PATH"
else
    echo "❌ PostgreSQL not found. Please install PostgreSQL first:"
    echo "   brew install postgresql@15"
    exit 1
fi

# Show PostgreSQL version
echo "📌 PostgreSQL version: $(pg_config --version)"
echo "📌 Installation directory: $(pg_config --sharedir)"

# Build the C library first
echo ""
echo "🔨 Building kJSON C library..."
if ! (cd ../c && make clean && make); then
    echo "❌ Failed to build C library"
    exit 1
fi

# Build the extension
echo ""
echo "🔨 Building kJSON PostgreSQL extension..."
if ! make clean PG_CONFIG=$(which pg_config); then
    echo "❌ Failed to clean build directory"
    exit 1
fi

if ! make PG_CONFIG=$(which pg_config); then
    echo "❌ Failed to build PostgreSQL extension"
    exit 1
fi

# Install the extension
echo ""
echo "📦 Installing kJSON extension..."
echo "   (You may be prompted for your password)"
sudo make install PG_CONFIG=$(which pg_config)

echo ""
echo "✅ Installation complete!"
echo ""
echo "📝 Next steps:"
echo "1. Start PostgreSQL if not running:"
echo "   brew services start postgresql@15"
echo ""
echo "2. Create the extension in your database:"
echo "   psql -d your_database -c 'DROP EXTENSION IF EXISTS kjson CASCADE; CREATE EXTENSION kjson;'"
echo ""
echo "3. Test it works:"
echo "   psql -d your_database -c \"SELECT '{test: 123n}'::kjson;\""
echo ""
echo "4. Run comprehensive tests:"
echo "   psql -d your_database -f test_fixes.sql"
echo ""
echo "🎉 Enjoy using kJSON with PostgreSQL!"