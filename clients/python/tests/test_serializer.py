"""Test kJSON serializer."""

import pytest
import uuid
from datetime import datetime, timezone
from kjson import dumps, loads, BigInt, Decimal128, Date, JSONEncoder


class TestBasicSerialization:
    """Test serialization of basic types."""
    
    def test_primitives(self):
        """Test serializing primitive types."""
        assert dumps(None) == "null"
        assert dumps(True) == "true"
        assert dumps(False) == "false"
        assert dumps(42) == "42"
        assert dumps(3.14) == "3.14"
        assert dumps("hello") == "'hello'"
    
    def test_arrays(self):
        """Test serializing arrays."""
        assert dumps([]) == "[]"
        assert dumps([1, 2, 3]) == "[1, 2, 3]"
        assert dumps(["a", "b", "c"]) == "['a', 'b', 'c']"
        assert dumps([True, False, None]) == "[true, false, null]"
    
    def test_objects(self):
        """Test serializing objects."""
        assert dumps({}) == "{}"
        assert dumps({"a": 1}) == "{a: 1}"
        assert dumps({"name": "test", "value": 42}) == "{name: 'test', value: 42}"
        
        # Keys that need quoting
        assert dumps({"with space": 1}) == "{'with space': 1}"
        assert dumps({"123": 1}) == "{'123': 1}"
        assert dumps({"$special": 1}) == "{'$special': 1}"
    
    def test_nested_structures(self):
        """Test serializing nested structures."""
        data = {
            "array": [1, 2, 3],
            "object": {"nested": True},
            "deep": {"level1": {"level2": [4, 5, 6]}}
        }
        result = dumps(data)
        # Verify it can be parsed back
        parsed = loads(result)
        assert parsed == data


class TestExtendedTypes:
    """Test serialization of extended types."""
    
    def test_bigint(self):
        """Test serializing BigInt."""
        bi = BigInt("123456789012345678901234567890")
        assert dumps(bi) == "123456789012345678901234567890n"
        
        # In object
        assert dumps({"value": bi}) == "{value: 123456789012345678901234567890n}"
    
    def test_decimal128(self):
        """Test serializing Decimal128."""
        d = Decimal128("99.99")
        assert dumps(d) == "99.99m"
        
        # In array
        assert dumps([d, Decimal128("100")]) == "[99.99m, 100m]"
    
    def test_uuid(self):
        """Test serializing UUID."""
        u = uuid.UUID("550e8400-e29b-41d4-a716-446655440000")
        assert dumps(u) == "550e8400-e29b-41d4-a716-446655440000"
        
        # In object
        assert dumps({"id": u}) == "{id: 550e8400-e29b-41d4-a716-446655440000}"
    
    def test_date(self):
        """Test serializing Date."""
        # UTC date
        d = Date(datetime(2025, 1, 10, 12, 0, 0, tzinfo=timezone.utc))
        assert dumps(d) == "2025-01-10T12:00:00Z"
        
        # In array
        assert dumps([d]) == "[2025-01-10T12:00:00Z]"


class TestFormatting:
    """Test output formatting options."""
    
    def test_pretty_print(self):
        """Test pretty printing with indentation."""
        data = {"name": "test", "items": [1, 2, 3]}
        
        # With indent=2
        result = dumps(data, indent=2)
        expected = """{
  name: 'test',
  items: [
    1,
    2,
    3
  ]
}"""
        assert result == expected
        
        # With indent=4
        result = dumps(data, indent=4)
        assert "    " in result  # 4 spaces
    
    def test_sort_keys(self):
        """Test key sorting."""
        data = {"z": 1, "a": 2, "m": 3}
        result = dumps(data, sort_keys=True)
        assert result == "{a: 2, m: 3, z: 1}"
        
        # With pretty print
        result = dumps(data, sort_keys=True, indent=2)
        lines = result.split('\n')
        assert "a: 2" in lines[1]
        assert "m: 3" in lines[2]
        assert "z: 1" in lines[3]
    
    def test_ensure_ascii(self):
        """Test ASCII escaping."""
        # Unicode characters
        assert dumps("Hello 世界") == "'Hello 世界'"
        assert dumps("Hello 世界", ensure_ascii=True) == r"'Hello \u4e16\u754c'"
        
        # In object
        data = {"message": "Hello 世界"}
        result = dumps(data, ensure_ascii=True)
        assert r"\u4e16\u754c" in result


class TestRoundTrip:
    """Test round-trip serialization and parsing."""
    
    def test_all_types(self):
        """Test round-trip with all supported types."""
        u = uuid.uuid4()
        data = {
            "null": None,
            "bool": True,
            "int": 42,
            "float": 3.14,
            "string": "hello",
            "array": [1, 2, 3],
            "object": {"nested": True},
            "bigint": BigInt("999999999999999999999999999999"),
            "decimal": Decimal128("99.99"),
            "uuid": u,
            "date": Date("2025-01-10T12:00:00Z")
        }
        
        # Serialize and parse back
        kjson_str = dumps(data)
        parsed = loads(kjson_str)
        
        # Verify all values
        assert parsed["null"] is None
        assert parsed["bool"] is True
        assert parsed["int"] == 42
        assert parsed["float"] == 3.14
        assert parsed["string"] == "hello"
        assert parsed["array"] == [1, 2, 3]
        assert parsed["object"] == {"nested": True}
        assert isinstance(parsed["bigint"], BigInt)
        assert str(parsed["bigint"]) == "999999999999999999999999999999"
        assert isinstance(parsed["decimal"], Decimal128)
        assert str(parsed["decimal"]) == "99.99"
        assert isinstance(parsed["uuid"], uuid.UUID)
        assert parsed["uuid"] == u
        assert isinstance(parsed["date"], Date)
        assert parsed["date"].to_iso8601() == "2025-01-10T12:00:00Z"
    
    def test_json5_features(self):
        """Test round-trip with JSON5 features."""
        data = {
            "unquoted": "key",
            "single": "quotes",
            "trailing": [1, 2, 3]
        }
        
        kjson_str = dumps(data)
        parsed = loads(kjson_str)
        assert parsed == data


class TestEncoderClass:
    """Test JSONEncoder class."""
    
    def test_encoder_basic(self):
        """Test basic encoder functionality."""
        encoder = JSONEncoder()
        assert encoder.encode({"a": 1}) == "{a: 1}"
        assert encoder.encode([1, 2, 3]) == "[1, 2, 3]"
    
    def test_encoder_options(self):
        """Test encoder with options."""
        encoder = JSONEncoder(indent=2, sort_keys=True)
        result = encoder.encode({"z": 1, "a": 2})
        assert "a: 2" in result
        assert "z: 1" in result
        assert "\n" in result
    
    def test_iterencode(self):
        """Test iterencode method."""
        encoder = JSONEncoder()
        result = list(encoder.iterencode({"a": 1}))
        assert len(result) == 1
        assert result[0] == "{a: 1}"


class TestSpecialCases:
    """Test special cases and edge conditions."""
    
    def test_special_float_values(self):
        """Test special float values."""
        # NaN and Infinity should be converted to null
        assert dumps(float('nan')) == "null"
        assert dumps(float('inf')) == "null"
        assert dumps(float('-inf')) == "null"
    
    def test_custom_objects(self):
        """Test serializing custom objects."""
        class CustomObject:
            def __init__(self):
                self.name = "custom"
                self.value = 42
        
        obj = CustomObject()
        result = dumps(obj)
        parsed = loads(result)
        assert parsed == {"name": "custom", "value": 42}
    
    def test_default_function(self):
        """Test custom default function."""
        class CustomType:
            def __init__(self, value):
                self.value = value
        
        def default(obj):
            if isinstance(obj, CustomType):
                return {"_type": "custom", "value": obj.value}
            raise TypeError(f"Type {type(obj)} not serializable")
        
        obj = CustomType(42)
        result = dumps({"custom": obj}, default=default)
        parsed = loads(result)
        assert parsed == {"custom": {"_type": "custom", "value": 42}}
    
    def test_tuple_serialization(self):
        """Test that tuples are serialized as arrays."""
        assert dumps((1, 2, 3)) == "[1, 2, 3]"
        assert dumps({"tuple": (4, 5, 6)}) == "{tuple: [4, 5, 6]}"


class TestSmartQuoteSelection:
    """Test smart quote selection during serialization."""
    
    def test_no_quotes_uses_single(self):
        """Test that strings with no quotes use single quotes."""
        assert dumps({"text": "hello"}) == "{text: 'hello'}"
        assert dumps({"text": "simple text"}) == "{text: 'simple text'}"
    
    def test_single_quotes_uses_double(self):
        """Test that strings with single quotes use double quotes."""
        assert dumps({"text": "it's nice"}) == '{text: "it\'s nice"}'
        assert dumps({"text": "don't do that"}) == '{text: "don\'t do that"}'
    
    def test_double_quotes_uses_single(self):
        """Test that strings with double quotes use single quotes."""
        assert dumps({"text": 'He said "hi"'}) == "{text: 'He said \"hi\"'}"
        assert dumps({"text": 'The "quoted" word'}) == "{text: 'The \"quoted\" word'}"
    
    def test_both_quotes_uses_backticks(self):
        """Test that strings with both quote types use backticks."""
        assert dumps({"text": 'He said "hello" and \'hi\''}) == "{text: `He said \"hello\" and 'hi'`}"
        assert dumps({"text": 'Mix "both" \'types\''}) == "{text: `Mix \"both\" 'types'`}"
    
    def test_backticks_in_string(self):
        """Test strings containing backticks."""
        assert dumps({"text": "template `string`"}) == "{text: 'template `string`'}"
        # With multiple backticks, single quotes still win due to tie-breaking
        assert dumps({"text": "`backtick` only"}) == "{text: '`backtick` only'}"
    
    def test_quote_selection_for_keys(self):
        """Test smart quote selection for object keys."""
        # Key with single quote should use double quotes
        assert dumps({"it's": "value"}) == '{\"it\'s\": \'value\'}'
        
        # Key with double quote should use single quotes
        assert dumps({'say "hi"': "value"}) == "{'say \"hi\"': 'value'}"
        
        # Key with both quotes should use backticks
        assert dumps({'mix "both" \'types\'': "value"}) == "{`mix \"both\" 'types'`: 'value'}"
    
    def test_round_trip_with_smart_quotes(self):
        """Test that smart quote selection preserves data through round trips."""
        test_data = {
            "simple": "hello",
            "single": "it's nice", 
            "double": 'He said "hi"',
            "mixed": 'He said "hello" and \'hi\'',
            "backticks": "template `string`"
        }
        
        json_str = dumps(test_data)
        parsed = loads(json_str)
        assert parsed == test_data