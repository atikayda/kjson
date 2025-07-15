-- Initialize database for kJSON PostgreSQL extension testing
-- This script runs automatically when the container starts

-- Create test user and database if they don't exist
CREATE USER kjson_test WITH PASSWORD 'kjson_test' CREATEDB;
CREATE DATABASE kjson_test OWNER kjson_test;

-- Grant necessary permissions
GRANT ALL PRIVILEGES ON DATABASE kjson_test TO kjson_test;
