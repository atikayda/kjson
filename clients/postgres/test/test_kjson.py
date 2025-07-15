#!/usr/bin/env python3
"""
Integration tests for kJSON PostgreSQL extension
"""

import os
import json
import uuid
import decimal
import datetime
import psycopg2
import psycopg2.extras
import pytest
from psycopg2.extensions import register_adapter, AsIs


class TestKJsonExtension:
    """Test suite for kJSON PostgreSQL extension"""
    
    @pytest.fixture(scope="class")
    def db_connection(self):
        """Create database connection"""
        conn = psycopg2.connect(
            host=os.environ.get('PGHOST', 'localhost'),
            port=os.environ.get('PGPORT', '5432'),
            user=os.environ.get('PGUSER', 'kjson_test'),
            password=os.environ.get('PGPASSWORD', 'kjson_test'),
            database=os.environ.get('PGDATABASE', 'kjson_test')
        )
        conn.autocommit = True
        yield conn
        conn.close()
    
    @pytest.fixture
    def cursor(self, db_connection):
        """Create cursor for tests"""
        cur = db_connection.cursor()
        yield cur
        cur.close()
    
    def test_basic_types(self, cursor):
        """Test basic JSON types"""
        test_cases = [
            ('null', 'null'),
            ('true', 'true'),
            ('false', 'false'),
            ('42', '42'),
            ('-3.14', '-3.14'),
            ('"hello"', '"hello"'),
            ('[]', '[]'),
            ('{}', '{}'),
        ]
        
        for input_val, expected in test_cases:
            cursor.execute(f"SELECT '{input_val}'::kjson")
            result = cursor.fetchone()[0]
            assert result == expected, f"Expected {expected}, got {result}"
    
    def test_extended_types(self, cursor):
        """Test extended kJSON types"""
        # BigInt
        cursor.execute("SELECT '123456789012345678901234567890n'::kjson")
        result = cursor.fetchone()[0]
        assert 'n' in result
        
        # Decimal128
        cursor.execute("SELECT '99.99m'::kjson")
        result = cursor.fetchone()[0]
        assert 'm' in result
        
        # UUID
        cursor.execute("SELECT '550e8400-e29b-41d4-a716-446655440000'::kjson")
        result = cursor.fetchone()[0]
        assert '550e8400-e29b-41d4-a716-446655440000' in result
        
        # Date
        cursor.execute("SELECT '2025-01-10T12:00:00Z'::kjson")
        result = cursor.fetchone()[0]
        assert '2025-01-10T12:00:00Z' in result
    
    def test_json5_features(self, cursor):
        """Test JSON5 syntax support"""
        # Unquoted keys
        cursor.execute("SELECT '{name: \"Alice\", age: 30}'::kjson")
        result = cursor.fetchone()[0]
        assert 'name' in result and 'Alice' in result
        
        # Trailing commas
        cursor.execute("SELECT '[1, 2, 3,]'::kjson")
        result = cursor.fetchone()[0]
        # Should parse successfully
        
        # Comments
        cursor.execute("""
            SELECT '{
                // Comment
                key: "value"
            }'::kjson
        """)
        result = cursor.fetchone()[0]
        assert 'key' in result
    
    def test_operators(self, cursor):
        """Test kjson operators"""
        # Create test data
        cursor.execute("""
            CREATE TEMP TABLE test_ops (
                id serial PRIMARY KEY,
                data kjson
            )
        """)
        
        cursor.execute("""
            INSERT INTO test_ops (data) VALUES
            ('{"name": "Alice", "age": 30}'::kjson),
            ('{"scores": [85, 92, 78]}'::kjson)
        """)
        
        # Test -> operator
        cursor.execute("SELECT data->'name' FROM test_ops WHERE data->'name' IS NOT NULL")
        result = cursor.fetchone()[0]
        assert result == '"Alice"'
        
        # Test ->> operator
        cursor.execute("SELECT data->>'name' FROM test_ops WHERE data->>'name' IS NOT NULL")
        result = cursor.fetchone()[0]
        assert result == 'Alice'
        
        # Test array access
        cursor.execute("SELECT data->'scores'->1 FROM test_ops WHERE data->'scores' IS NOT NULL")
        result = cursor.fetchone()[0]
        assert result == '92'
        
        cursor.execute("DROP TABLE test_ops")
    
    def test_type_casting(self, cursor):
        """Test casting between types"""
        # kjson to json
        cursor.execute("SELECT '{\
"name\": \"Test\"}'::kjson::json")
        result = cursor.fetchone()[0]
        # Should be valid JSON
        json.loads(result)
        
        # json to kjson
        cursor.execute("SELECT '{\"a\": 1}'::json::kjson")
        result = cursor.fetchone()[0]
        assert result == '{"a": 1}'
        
        # Extended types lose precision in JSON
        cursor.execute("SELECT '{\"id\": 123n}'::kjson::json")
        result = cursor.fetchone()[0]
        parsed = json.loads(result)
        # BigInt becomes string in standard JSON
        assert isinstance(parsed['id'], str)
    
    def test_utility_functions(self, cursor):
        """Test utility functions"""
        # kjson_pretty
        cursor.execute("SELECT kjson_pretty('{\"a\": 1, \"b\": 2}'::kjson, 2)")
        result = cursor.fetchone()[0]
        assert '\n' in result  # Should have newlines
        
        # kjson_typeof
        cursor.execute("SELECT kjson_typeof('123n'::kjson)")
        result = cursor.fetchone()[0]
        assert result == 'bigint'
        
        cursor.execute("SELECT kjson_typeof('[]'::kjson)")
        result = cursor.fetchone()[0]
        assert result == 'array'
    
    def test_extraction_functions(self, cursor):
        """Test extraction functions"""
        test_data = """{
            "id": 550e8400-e29b-41d4-a716-446655440000,
            "amount": 999999999999999999n,
            "price": 99.99m,
            "created": 2025-01-10T12:00:00Z
        }"""
        
        # Extract UUID
        cursor.execute(f"SELECT kjson_extract_uuid('{test_data}'::kjson, 'id')")
        result = cursor.fetchone()[0]
        assert isinstance(result, uuid.UUID)
        assert str(result) == '550e8400-e29b-41d4-a716-446655440000'
        
        # Extract numeric
        cursor.execute(f"SELECT kjson_extract_numeric('{test_data}'::kjson, 'amount')")
        result = cursor.fetchone()[0]
        assert isinstance(result, decimal.Decimal)
        
        cursor.execute(f"SELECT kjson_extract_numeric('{test_data}'::kjson, 'price')")
        result = cursor.fetchone()[0]
        assert result == decimal.Decimal('99.99')
        
        # Extract timestamp
        cursor.execute(f"SELECT kjson_extract_timestamp('{test_data}'::kjson, 'created')")
        result = cursor.fetchone()[0]
        assert isinstance(result, datetime.datetime)
    
    def test_aggregate_functions(self, cursor):
        """Test aggregate functions"""
        # Test kjson_agg
        cursor.execute("""
            SELECT kjson_agg(x::text::kjson)
            FROM generate_series(1, 3) x
        """)
        result = cursor.fetchone()[0]
        assert result == '[1, 2, 3]'
        
        # Test kjson_object_agg
        cursor.execute("""
            WITH data (k, v) AS (
                VALUES ('a', '1'::kjson), ('b', '2'::kjson)
            )
            SELECT kjson_object_agg(k, v) FROM data
        """)
        result = cursor.fetchone()[0]
        assert 'a' in result and 'b' in result
    
    def test_performance(self, cursor):
        """Test performance with larger datasets"""
        # Create table with many rows
        cursor.execute("""
            CREATE TEMP TABLE perf_test (
                id serial PRIMARY KEY,
                data kjson
            )
        """)
        
        # Insert 1000 rows
        cursor.execute("""
            INSERT INTO perf_test (data)
            SELECT ('{
                "id": ' || n || 'n,
                "name": "Item ' || n || '",
                "price": ' || (n * 0.99) || 'm,
                "created": 2025-01-10T12:00:00Z,
                "tags": ["tag1", "tag2", "tag3"]
            }')::kjson
            FROM generate_series(1, 1000) n
        """)
        
        # Test query performance
        cursor.execute("""
            SELECT COUNT(*), 
                   kjson_agg(data->'id') as ids
            FROM perf_test
            WHERE (data->>'price')::numeric > 50
        """)
        
        count, ids = cursor.fetchone()
        assert count > 0
        assert ids is not None
        
        cursor.execute("DROP TABLE perf_test")
    
    def test_error_handling(self, cursor):
        """Test error handling"""
        # Invalid JSON
        with pytest.raises(psycopg2.Error):
            cursor.execute("SELECT '{invalid json}'::kjson")
        
        # Invalid UUID
        with pytest.raises(psycopg2.Error):
            cursor.execute("SELECT 'not-a-uuid'::kjson")
        
        # Invalid date
        with pytest.raises(psycopg2.Error):
            cursor.execute("SELECT '2025-13-45T25:99:99Z'::kjson")


if __name__ == '__main__':
    pytest.main([__file__, '-v'])