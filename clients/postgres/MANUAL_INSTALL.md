# Manual Installation Steps for kJSON PostgreSQL Extension

Since the installation requires sudo permissions, here are the manual steps:

## Files that need to be installed:

1. **Shared library**: `kjson.so`
   - Source: `./kjson.so`
   - Destination: `/opt/homebrew/opt/postgresql@15/lib/postgresql/kjson.so`

2. **Control file**: `kjson.control`
   - Source: `./kjson.control`
   - Destination: `/opt/homebrew/opt/postgresql@15/share/postgresql@15/extension/kjson.control`

3. **SQL script**: `kjson--1.0.0.sql`
   - Source: `./sql/kjson--1.0.0.sql`
   - Destination: `/opt/homebrew/opt/postgresql@15/share/postgresql@15/extension/kjson--1.0.0.sql`

## Installation Commands:

```bash
# Copy the shared library
sudo cp kjson.so /opt/homebrew/opt/postgresql@15/lib/postgresql/

# Copy the control file
sudo cp kjson.control /opt/homebrew/opt/postgresql@15/share/postgresql@15/extension/

# Copy the SQL script
sudo cp sql/kjson--1.0.0.sql /opt/homebrew/opt/postgresql@15/share/postgresql@15/extension/

# Set proper permissions
sudo chmod 755 /opt/homebrew/opt/postgresql@15/lib/postgresql/kjson.so
sudo chmod 644 /opt/homebrew/opt/postgresql@15/share/postgresql@15/extension/kjson.control
sudo chmod 644 /opt/homebrew/opt/postgresql@15/share/postgresql@15/extension/kjson--1.0.0.sql
```

## Alternative: Use make install with sudo

```bash
# Set PATH and run make install
export PATH="/opt/homebrew/opt/postgresql@15/bin:$PATH"
sudo make install PG_CONFIG=/opt/homebrew/opt/postgresql@15/bin/pg_config
```

## Verify Installation:

After installation, connect to PostgreSQL and run:

```sql
-- List available extensions
SELECT * FROM pg_available_extensions WHERE name = 'kjson';

-- Create the extension
CREATE EXTENSION kjson;

-- Test it works
SELECT '{test: 123n}'::kjson;
```