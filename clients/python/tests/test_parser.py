"""Test kJSON parser."""

import pytest
import uuid
from kjson import loads, JSONDecodeError, BigInt, Decimal128, Date


class TestBasicTypes:
    """Test parsing basic JSON types."""
    
    def test_null(self):
        """Test parsing null."""
        assert loads("null") is None
    
    def test_boolean(self):
        """Test parsing boolean values."""
        assert loads("true") is True
        assert loads("false") is False
    
    def test_number(self):
        """Test parsing numbers."""
        assert loads("42") == 42
        assert loads("3.14") == 3.14
        assert loads("-123") == -123
        assert loads("1.23e-4") == 0.000123
        assert loads("1E+2") == 100
    
    def test_string(self):
        """Test parsing strings."""
        assert loads('"hello"') == "hello"
        assert loads('"hello world"') == "hello world"
        assert loads('""') == ""
        
        # Escape sequences
        assert loads(r'"\n\r\t"') == "\n\r\t"
        assert loads(r'"\""') == '"'
        assert loads(r'"\\"') == "\\"
        assert loads(r'"\/"') == "/"
        assert loads(r'"\b\f"') == "\b\f"
        
        # Unicode escapes
        assert loads(r'"\u0048\u0065\u006c\u006c\u006f"') == "Hello"
    
    def test_array(self):
        """Test parsing arrays."""
        assert loads("[]") == []
        assert loads("[1, 2, 3]") == [1, 2, 3]
        assert loads('["a", "b", "c"]') == ["a", "b", "c"]
        assert loads("[true, false, null]") == [True, False, None]
        
        # Nested arrays
        assert loads("[[1, 2], [3, 4]]") == [[1, 2], [3, 4]]
    
    def test_object(self):
        """Test parsing objects."""
        assert loads("{}") == {}
        assert loads('{"a": 1, "b": 2}') == {"a": 1, "b": 2}
        assert loads('{"name": "test", "value": 42}') == {"name": "test", "value": 42}
        
        # Nested objects
        result = loads('{"a": {"b": {"c": 123}}}')
        assert result == {"a": {"b": {"c": 123}}}


class TestExtendedTypes:
    """Test parsing extended kJSON types."""
    
    def test_bigint(self):
        """Test parsing BigInt values."""
        result = loads("123456789012345678901234567890n")
        assert isinstance(result, BigInt)
        assert str(result) == "123456789012345678901234567890"
        
        # Negative BigInt
        result = loads("-123456789012345678901234567890n")
        assert isinstance(result, BigInt)
        assert str(result) == "-123456789012345678901234567890"
    
    def test_decimal128(self):
        """Test parsing Decimal128 values."""
        result = loads("99.99m")
        assert isinstance(result, Decimal128)
        assert str(result) == "99.99"
        
        # Integer decimal
        result = loads("100m")
        assert isinstance(result, Decimal128)
        assert str(result) == "100"
        
        # Negative decimal
        result = loads("-99.99m")
        assert isinstance(result, Decimal128)
        assert str(result) == "-99.99"
    
    def test_uuid(self):
        """Test parsing UUID values."""
        uuid_str = "550e8400-e29b-41d4-a716-446655440000"
        result = loads(uuid_str)
        assert isinstance(result, uuid.UUID)
        assert str(result) == uuid_str
    
    def test_date(self):
        """Test parsing Date values."""
        # UTC date
        result = loads("2025-01-10T12:00:00Z")
        assert isinstance(result, Date)
        assert result.to_iso8601() == "2025-01-10T12:00:00Z"
        
        # Date with timezone
        result = loads("2025-01-10T12:00:00-08:00")
        assert isinstance(result, Date)
        assert "-08:00" in result.to_iso8601()


class TestJSON5Features:
    """Test JSON5 syntax features."""
    
    def test_unquoted_keys(self):
        """Test objects with unquoted keys."""
        result = loads("{name: 'test', value: 42}")
        assert result == {"name": "test", "value": 42}
        
        # Mixed quoted and unquoted
        result = loads('{name: "test", "value": 42}')
        assert result == {"name": "test", "value": 42}
    
    def test_trailing_commas(self):
        """Test trailing commas in arrays and objects."""
        assert loads("[1, 2, 3,]") == [1, 2, 3]
        assert loads("{a: 1,}") == {"a": 1}
        assert loads("{a: 1, b: 2,}") == {"a": 1, "b": 2}
    
    def test_single_quotes(self):
        """Test single-quoted strings."""
        assert loads("'hello'") == "hello"
        assert loads("{a: 'test'}") == {"a": "test"}
        assert loads("['a', 'b', 'c']") == ["a", "b", "c"]
    
    def test_comments(self):
        """Test single-line and block comments."""
        # Single-line comments
        result = loads("""
        {
            // This is a comment
            "name": "test",  // Another comment
            "value": 42
        }
        """)
        assert result == {"name": "test", "value": 42}
        
        # Block comments
        result = loads("""
        {
            /* This is a
               multi-line comment */
            "name": "test",
            /* Another comment */ "value": 42
        }
        """)
        assert result == {"name": "test", "value": 42}


class TestComplexStructures:
    """Test parsing complex nested structures."""
    
    def test_mixed_types(self):
        """Test object with mixed extended types."""
        kjson_str = """{
            id: 550e8400-e29b-41d4-a716-446655440000,
            bigNumber: 123456789012345678901234567890n,
            price: 99.99m,
            created: 2025-01-10T12:00:00Z,
            active: true,
            tags: ["new", "sale"],
            metadata: {
                version: 1,
            },
        }"""
        
        result = loads(kjson_str)
        
        assert isinstance(result["id"], uuid.UUID)
        assert isinstance(result["bigNumber"], BigInt)
        assert isinstance(result["price"], Decimal128)
        assert isinstance(result["created"], Date)
        assert result["active"] is True
        assert result["tags"] == ["new", "sale"]
        assert result["metadata"]["version"] == 1
    
    def test_deeply_nested(self):
        """Test deeply nested structures."""
        kjson_str = """
        {
            "level1": {
                "level2": {
                    "level3": {
                        "data": [1, 2, 3],
                        "bigint": 999999999999999999999n
                    }
                }
            }
        }
        """
        
        result = loads(kjson_str)
        assert result["level1"]["level2"]["level3"]["data"] == [1, 2, 3]
        assert isinstance(result["level1"]["level2"]["level3"]["bigint"], BigInt)


class TestErrorHandling:
    """Test parser error handling."""
    
    def test_invalid_json(self):
        """Test invalid JSON syntax."""
        with pytest.raises(JSONDecodeError):
            loads("{invalid json}")
        
        with pytest.raises(JSONDecodeError):
            loads("[1, 2,")  # Unterminated array
        
        with pytest.raises(JSONDecodeError):
            loads('{"key":')  # Unterminated object
    
    def test_invalid_escape(self):
        """Test invalid escape sequences."""
        with pytest.raises(JSONDecodeError) as exc_info:
            loads(r'"\x"')
        assert "Invalid escape sequence" in str(exc_info.value)
    
    def test_invalid_number(self):
        """Test invalid number formats."""
        with pytest.raises(JSONDecodeError):
            loads("123.456.789")  # Multiple decimal points
        
        with pytest.raises(JSONDecodeError):
            loads("123e")  # Incomplete exponent
    
    def test_invalid_bigint(self):
        """Test invalid BigInt values."""
        with pytest.raises(JSONDecodeError):
            loads("123.45n")  # BigInt with decimal
        
        with pytest.raises(JSONDecodeError):
            loads("123e5n")  # BigInt with exponent
    
    def test_unterminated_string(self):
        """Test unterminated string literal."""
        with pytest.raises(JSONDecodeError) as exc_info:
            loads('"unterminated')
        assert "Unterminated string" in str(exc_info.value)
    
    def test_extra_data(self):
        """Test extra data after valid JSON."""
        with pytest.raises(JSONDecodeError) as exc_info:
            loads('{"valid": true} extra')
        assert "Extra data" in str(exc_info.value)


class TestEdgeCases:
    """Test edge cases and special scenarios."""
    
    def test_empty_values(self):
        """Test empty arrays and objects."""
        assert loads("[]") == []
        assert loads("{}") == {}
        assert loads('""') == ""
    
    def test_whitespace_handling(self):
        """Test various whitespace scenarios."""
        assert loads("  \n\t42\n\t  ") == 42
        assert loads("[\n\t1,\n\t2\n]") == [1, 2]
    
    def test_unicode_in_strings(self):
        """Test Unicode characters in strings."""
        assert loads('"Hello 世界"') == "Hello 世界"
        assert loads('"😀"') == "😀"
    
    def test_object_hook(self):
        """Test object hook functionality."""
        def hook(obj):
            if "type" in obj and obj["type"] == "special":
                return "SPECIAL_OBJECT"
            return obj
        
        result = loads('{"type": "special", "data": 123}', object_hook=hook)
        assert result == "SPECIAL_OBJECT"
        
        # Nested objects
        result = loads('{"outer": {"type": "special"}}', object_hook=hook)
        assert result == {"outer": "SPECIAL_OBJECT"}


class TestBacktickStrings:
    """Test parsing backtick-quoted strings."""
    
    def test_simple_backtick_string(self):
        """Test basic backtick string parsing."""
        assert loads('`hello`') == "hello"
        assert loads('`template string`') == "template string"
    
    def test_backticks_with_quotes(self):
        """Test backtick strings containing single and double quotes."""
        assert loads('`He said "Hello" and \'Hi\'`') == 'He said "Hello" and \'Hi\''
        assert loads('`"quoted" text`') == '"quoted" text'
        assert loads('`\'single\' quotes`') == "'single' quotes"
    
    def test_multiline_backtick_strings(self):
        """Test multiline backtick strings."""
        assert loads('`Line 1\nLine 2\nLine 3`') == "Line 1\nLine 2\nLine 3"
    
    def test_escaped_backticks(self):
        """Test escaped backticks in backtick strings."""
        assert loads('`This has a \\` backtick`') == "This has a ` backtick"
        assert loads('`Multiple \\` backticks \\` here`') == "Multiple ` backticks ` here"
    
    def test_backtick_object_keys(self):
        """Test backtick-quoted object keys."""
        result = loads('{`key with spaces`: "value"}')
        assert result == {"key with spaces": "value"}
        
        result = loads('{`"quoted" key`: "value"}')
        assert result == {'"quoted" key': "value"}
    
    def test_mixed_quote_types_in_object(self):
        """Test object with different quote types."""
        kjson_str = """{
            single: 'value1',
            double: "value2", 
            backtick: `value3`,
            mixed: `He said "hello" and 'hi'`
        }"""
        
        result = loads(kjson_str)
        assert result == {
            "single": "value1",
            "double": "value2",
            "backtick": "value3",
            "mixed": 'He said "hello" and \'hi\''
        }