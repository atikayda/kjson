"""Test kJSON extended types."""

import pytest
from datetime import datetime, timezone, timedelta
from kjson import BigInt, Decimal128, Date, uuid_v4, uuid_v7


class TestBigInt:
    """Test BigInt type."""
    
    def test_from_int(self):
        """Test creating BigInt from int."""
        bi = BigInt(123456789012345678901234567890)
        assert str(bi) == "123456789012345678901234567890"
        assert bi.to_kjson_string() == "123456789012345678901234567890n"
    
    def test_from_string(self):
        """Test creating BigInt from string."""
        bi = BigInt("123456789012345678901234567890")
        assert str(bi) == "123456789012345678901234567890"
        
        # With 'n' suffix
        bi2 = BigInt("123456789012345678901234567890n")
        assert str(bi2) == "123456789012345678901234567890"
    
    def test_negative(self):
        """Test negative BigInt."""
        bi = BigInt("-123456789012345678901234567890")
        assert str(bi) == "-123456789012345678901234567890"
        assert bi.to_kjson_string() == "-123456789012345678901234567890n"
    
    def test_equality(self):
        """Test BigInt equality."""
        bi1 = BigInt(123)
        bi2 = BigInt("123")
        bi3 = BigInt(456)
        
        assert bi1 == bi2
        assert bi1 != bi3
        assert bi1 != 123  # Not equal to regular int
    
    def test_hash(self):
        """Test BigInt hashing."""
        bi1 = BigInt(123)
        bi2 = BigInt(123)
        assert hash(bi1) == hash(bi2)


class TestDecimal128:
    """Test Decimal128 type."""
    
    def test_from_float(self):
        """Test creating Decimal128 from float."""
        d = Decimal128(99.99)
        assert str(d) == "99.99"
        assert d.to_kjson_string() == "99.99m"
    
    def test_from_string(self):
        """Test creating Decimal128 from string."""
        d = Decimal128("99.99")
        assert str(d) == "99.99"
        
        # With 'm' suffix
        d2 = Decimal128("99.99m")
        assert str(d2) == "99.99"
    
    def test_integer_value(self):
        """Test Decimal128 with integer value."""
        d = Decimal128("100")
        assert str(d) == "100"
        assert d.to_kjson_string() == "100m"
    
    def test_negative(self):
        """Test negative Decimal128."""
        d = Decimal128("-99.99")
        assert str(d) == "-99.99"
        assert d.to_kjson_string() == "-99.99m"
    
    def test_leading_zeros(self):
        """Test Decimal128 with leading zeros after decimal."""
        d = Decimal128("0.0099")
        assert str(d) == "0.0099"
    
    def test_equality(self):
        """Test Decimal128 equality."""
        d1 = Decimal128("99.99")
        d2 = Decimal128(99.99)
        d3 = Decimal128("100.00")
        
        assert d1 == d2
        assert d1 != d3
        assert d1 != 99.99  # Not equal to regular float


class TestDate:
    """Test Date type."""
    
    def test_from_datetime_utc(self):
        """Test creating Date from UTC datetime."""
        dt = datetime(2025, 1, 10, 12, 0, 0, tzinfo=timezone.utc)
        date = Date(dt)
        
        assert date.to_iso8601() == "2025-01-10T12:00:00Z"
        assert date.tz_offset is None
    
    def test_from_datetime_with_offset(self):
        """Test creating Date from datetime with timezone."""
        tz = timezone(timedelta(hours=-8))  # PST
        dt = datetime(2025, 1, 10, 12, 0, 0, tzinfo=tz)
        date = Date(dt)
        
        assert date.tz_offset == -480  # -8 hours in minutes
        assert "2025-01-10T12:00:00-08:00" in date.to_iso8601()
    
    def test_from_string_utc(self):
        """Test creating Date from UTC ISO string."""
        date = Date("2025-01-10T12:00:00Z")
        assert date.to_iso8601() == "2025-01-10T12:00:00Z"
        assert date.tz_offset is None
    
    def test_from_string_with_offset(self):
        """Test creating Date from ISO string with offset."""
        date = Date("2025-01-10T12:00:00-08:00")
        assert date.tz_offset == -480
        assert "-08:00" in date.to_iso8601()
    
    def test_naive_datetime(self):
        """Test creating Date from naive datetime."""
        dt = datetime(2025, 1, 10, 12, 0, 0)
        date = Date(dt)
        
        assert date.to_iso8601() == "2025-01-10T12:00:00Z"
        assert date.tz_offset is None
    
    def test_equality(self):
        """Test Date equality."""
        date1 = Date("2025-01-10T12:00:00Z")
        date2 = Date(datetime(2025, 1, 10, 12, 0, 0, tzinfo=timezone.utc))
        date3 = Date("2025-01-10T13:00:00Z")
        
        assert date1 == date2
        assert date1 != date3


class TestUUIDGeneration:
    """Test UUID generation functions."""
    
    def test_uuid_v4(self):
        """Test UUID v4 generation."""
        u1 = uuid_v4()
        u2 = uuid_v4()
        
        assert u1.version == 4
        assert u1 != u2
        assert len(str(u1)) == 36
    
    def test_uuid_v7(self):
        """Test UUID v7 generation."""
        u1 = uuid_v7()
        u2 = uuid_v7()
        
        assert u1.version == 7
        assert u1 != u2
        assert len(str(u1)) == 36
    
    def test_uuid_v7_ordering(self):
        """Test that UUID v7 values increase over time."""
        import time
        
        u1 = uuid_v7()
        time.sleep(0.001)  # Sleep 1ms
        u2 = uuid_v7()
        
        # Extract timestamp from first 6 bytes
        ts1 = int.from_bytes(u1.bytes[:6], 'big')
        ts2 = int.from_bytes(u2.bytes[:6], 'big')
        
        assert ts2 >= ts1  # Later UUID should have same or higher timestamp