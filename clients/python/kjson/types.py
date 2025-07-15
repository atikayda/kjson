"""Extended types for kJSON."""

import uuid
import time
import struct
import secrets
from datetime import datetime, timezone, timedelta
from typing import Optional, Union


class BigInt:
    """Arbitrary precision integer type."""
    
    def __init__(self, value: Union[int, str]):
        """Initialize BigInt from int or string."""
        if isinstance(value, str):
            # Remove 'n' suffix if present
            value = value.rstrip('n')
        self.value = int(value)
    
    def __str__(self) -> str:
        """Return string representation without suffix."""
        return str(self.value)
    
    def __repr__(self) -> str:
        """Return Python representation."""
        return f"BigInt({self.value})"
    
    def __eq__(self, other) -> bool:
        """Compare equality."""
        if isinstance(other, BigInt):
            return self.value == other.value
        return False
    
    def __hash__(self) -> int:
        """Return hash value."""
        return hash(self.value)
    
    def to_kjson_string(self) -> str:
        """Return kJSON string representation with 'n' suffix."""
        return f"{self.value}n"


class Decimal128:
    """High-precision decimal type."""
    
    def __init__(self, value: Union[float, str]):
        """Initialize Decimal128 from float or string."""
        if isinstance(value, str):
            # Remove 'm' suffix if present
            value = value.rstrip('m')
            self.negative = value.startswith('-')
            value = value.lstrip('-')
            
            if '.' in value:
                integer_part, decimal_part = value.split('.')
                self.digits = integer_part + decimal_part
                self.exponent = -len(decimal_part)
            else:
                self.digits = value
                self.exponent = 0
        else:
            # Convert from float
            str_value = str(value)
            self.negative = value < 0
            str_value = str_value.lstrip('-')
            
            if '.' in str_value:
                integer_part, decimal_part = str_value.split('.')
                self.digits = integer_part + decimal_part
                self.exponent = -len(decimal_part)
            else:
                self.digits = str_value
                self.exponent = 0
    
    def __str__(self) -> str:
        """Return string representation without suffix."""
        if self.exponent == 0:
            result = self.digits
        elif self.exponent < 0:
            exp = -self.exponent
            if exp >= len(self.digits):
                # Need leading zeros
                zeros = '0' * (exp - len(self.digits) + 1)
                result = f"0.{zeros}{self.digits}"
            else:
                # Split at decimal point
                split_pos = len(self.digits) - exp
                result = f"{self.digits[:split_pos]}.{self.digits[split_pos:]}"
        else:
            # Positive exponent
            result = self.digits + '0' * self.exponent
        
        return f"-{result}" if self.negative else result
    
    def __repr__(self) -> str:
        """Return Python representation."""
        return f"Decimal128('{self}')"
    
    def __eq__(self, other) -> bool:
        """Compare equality."""
        if isinstance(other, Decimal128):
            return (self.negative == other.negative and 
                    self.digits == other.digits and 
                    self.exponent == other.exponent)
        return False
    
    def __hash__(self) -> int:
        """Return hash value."""
        return hash((self.negative, self.digits, self.exponent))
    
    def to_kjson_string(self) -> str:
        """Return kJSON string representation with 'm' suffix."""
        return f"{self}m"


class Instant:
    """Instant type representing a nanosecond-precision timestamp in Zulu time (UTC)."""
    
    def __init__(self, nanoseconds: int):
        """Initialize Instant from nanoseconds since epoch.
        
        Args:
            nanoseconds: nanoseconds since Unix epoch (1970-01-01T00:00:00Z)
        """
        self.nanoseconds = nanoseconds
    
    @classmethod
    def from_epoch_nanos(cls, nanos: int) -> 'Instant':
        """Create Instant from nanoseconds since epoch."""
        return cls(nanos)
    
    @classmethod
    def from_epoch_millis(cls, millis: int) -> 'Instant':
        """Create Instant from milliseconds since epoch."""
        return cls(millis * 1_000_000)
    
    @classmethod
    def from_epoch_seconds(cls, seconds: int) -> 'Instant':
        """Create Instant from seconds since epoch."""
        return cls(seconds * 1_000_000_000)
    
    @classmethod
    def now(cls) -> 'Instant':
        """Get current instant."""
        millis = int(time.time() * 1000)
        return cls.from_epoch_millis(millis)
    
    @classmethod
    def from_iso8601(cls, iso_string: str) -> 'Instant':
        """Parse ISO 8601 string to Instant.
        
        Supports formats like:
        - 2025-01-01T00:00:00Z
        - 2025-01-01T00:00:00.123Z
        - 2025-01-01T00:00:00.123456789Z
        - 2025-01-01T00:00:00+00:00 (converted to Zulu)
        - 2025-01-01T00:00:00-05:00 (converted to Zulu)
        """
        # Convert to Zulu time if it has a timezone
        if '+' in iso_string or (iso_string.count('-') > 2):
            # Has timezone offset, convert to Zulu
            dt = datetime.fromisoformat(iso_string)
            if dt.tzinfo is None:
                dt = dt.replace(tzinfo=timezone.utc)
            else:
                dt = dt.astimezone(timezone.utc)
            zulu_string = dt.isoformat().replace('+00:00', 'Z')
        elif not iso_string.endswith('Z'):
            # No timezone specified, assume Zulu
            zulu_string = iso_string + 'Z'
        else:
            zulu_string = iso_string
        
        # Parse the Zulu string manually to preserve nanosecond precision
        import re
        match = re.match(r'^(\d{4})-(\d{2})-(\d{2})T(\d{2}):(\d{2}):(\d{2})(?:\.(\d+))?Z$', zulu_string)
        if not match:
            raise ValueError(f"Invalid ISO date string format: {iso_string}")
        
        year, month, day, hour, minute, second, fraction_str = match.groups()
        
        # Create datetime object for the main parts
        dt = datetime(
            int(year), int(month), int(day),
            int(hour), int(minute), int(second),
            tzinfo=timezone.utc
        )
        
        nanos = int(dt.timestamp() * 1_000_000_000)
        
        # Handle fractional seconds
        if fraction_str:
            # Pad or truncate to 9 digits (nanoseconds)
            padded_fraction = fraction_str.ljust(9, '0')[:9]
            fraction_nanos = int(padded_fraction)
            nanos += fraction_nanos
        
        return cls(nanos)
    
    def to_iso8601(self) -> str:
        """Convert to ISO 8601 string with nanosecond precision."""
        # Convert to seconds and nanosecond remainder
        seconds = self.nanoseconds // 1_000_000_000
        nanos_remainder = self.nanoseconds % 1_000_000_000
        
        # Create datetime from seconds
        dt = datetime.fromtimestamp(seconds, tz=timezone.utc)
        
        # Format base ISO string
        base_iso = dt.strftime("%Y-%m-%dT%H:%M:%S")
        
        if nanos_remainder == 0:
            return base_iso + 'Z'
        
        # Format nanoseconds (remove trailing zeros)
        fractional_str = str(nanos_remainder).zfill(9).rstrip('0')
        return base_iso + '.' + fractional_str + 'Z'
    
    def to_datetime(self) -> datetime:
        """Convert to datetime object (loses nanosecond precision)."""
        seconds = self.nanoseconds / 1_000_000_000
        return datetime.fromtimestamp(seconds, tz=timezone.utc)
    
    def epoch_nanos(self) -> int:
        """Get nanoseconds since epoch."""
        return self.nanoseconds
    
    def epoch_millis(self) -> int:
        """Get milliseconds since epoch."""
        return self.nanoseconds // 1_000_000
    
    def epoch_seconds(self) -> int:
        """Get seconds since epoch."""
        return self.nanoseconds // 1_000_000_000
    
    def __str__(self) -> str:
        """Return ISO 8601 string representation."""
        return self.to_iso8601()
    
    def __repr__(self) -> str:
        """Return Python representation."""
        return f"Instant({self.nanoseconds})"
    
    def __eq__(self, other) -> bool:
        """Compare equality."""
        if isinstance(other, Instant):
            return self.nanoseconds == other.nanoseconds
        return False
    
    def __hash__(self) -> int:
        """Return hash value."""
        return hash(self.nanoseconds)
    
    def __lt__(self, other) -> bool:
        """Compare less than."""
        if isinstance(other, Instant):
            return self.nanoseconds < other.nanoseconds
        return NotImplemented
    
    def __le__(self, other) -> bool:
        """Compare less than or equal."""
        if isinstance(other, Instant):
            return self.nanoseconds <= other.nanoseconds
        return NotImplemented
    
    def __gt__(self, other) -> bool:
        """Compare greater than."""
        if isinstance(other, Instant):
            return self.nanoseconds > other.nanoseconds
        return NotImplemented
    
    def __ge__(self, other) -> bool:
        """Compare greater than or equal."""
        if isinstance(other, Instant):
            return self.nanoseconds >= other.nanoseconds
        return NotImplemented


class Duration:
    """Duration type representing a time span with nanosecond precision."""
    
    def __init__(self, nanoseconds: int):
        """Initialize Duration from nanoseconds.
        
        Args:
            nanoseconds: duration in nanoseconds
        """
        self.nanoseconds = nanoseconds
    
    @classmethod
    def from_nanos(cls, nanos: int) -> 'Duration':
        """Create Duration from nanoseconds."""
        return cls(nanos)
    
    @classmethod
    def from_millis(cls, millis: int) -> 'Duration':
        """Create Duration from milliseconds."""
        return cls(millis * 1_000_000)
    
    @classmethod
    def from_seconds(cls, seconds: int) -> 'Duration':
        """Create Duration from seconds."""
        return cls(seconds * 1_000_000_000)
    
    @classmethod
    def from_minutes(cls, minutes: int) -> 'Duration':
        """Create Duration from minutes."""
        return cls(minutes * 60 * 1_000_000_000)
    
    @classmethod
    def from_hours(cls, hours: int) -> 'Duration':
        """Create Duration from hours."""
        return cls(hours * 3600 * 1_000_000_000)
    
    @classmethod
    def from_days(cls, days: int) -> 'Duration':
        """Create Duration from days."""
        return cls(days * 86400 * 1_000_000_000)
    
    @classmethod
    def from_iso8601(cls, duration_string: str) -> 'Duration':
        """Parse ISO 8601 duration string.
        
        Supports formats like: PT1H2M3S, P1DT2H3M4.5S, PT0.000000001S
        """
        import re
        match = re.match(r'^P(?:(\d+)D)?(?:T(?:(\d+)H)?(?:(\d+)M)?(?:(\d+(?:\.\d+)?)S)?)?$', duration_string)
        if not match:
            raise ValueError(f"Invalid ISO duration format: {duration_string}")
        
        days, hours, minutes, seconds = match.groups()
        
        total_nanos = 0
        
        if days:
            total_nanos += int(days) * 86400 * 1_000_000_000
        if hours:
            total_nanos += int(hours) * 3600 * 1_000_000_000
        if minutes:
            total_nanos += int(minutes) * 60 * 1_000_000_000
        if seconds:
            sec = float(seconds)
            total_nanos += int(sec * 1_000_000_000)
        
        return cls(total_nanos)
    
    def to_iso8601(self) -> str:
        """Convert to ISO 8601 duration string."""
        if self.nanoseconds == 0:
            return 'PT0S'
        
        remaining = abs(self.nanoseconds)  # Work with absolute value
        result = 'P'
        
        # Days
        days = remaining // (86400 * 1_000_000_000)
        if days > 0:
            result += f'{days}D'
            remaining = remaining % (86400 * 1_000_000_000)
        
        if remaining > 0:
            result += 'T'
            
            # Hours
            hours = remaining // (3600 * 1_000_000_000)
            if hours > 0:
                result += f'{hours}H'
                remaining = remaining % (3600 * 1_000_000_000)
            
            # Minutes
            minutes = remaining // (60 * 1_000_000_000)
            if minutes > 0:
                result += f'{minutes}M'
                remaining = remaining % (60 * 1_000_000_000)
            
            # Seconds (with fractional part)
            if remaining > 0:
                seconds = remaining // 1_000_000_000
                nanos_part = remaining % 1_000_000_000
                
                if nanos_part == 0:
                    result += f'{seconds}S'
                else:
                    fractional_str = str(nanos_part).zfill(9).rstrip('0')
                    result += f'{seconds}.{fractional_str}S'
        
        # Handle negative durations
        if self.nanoseconds < 0:
            result = '-' + result
        
        return result
    
    def total_nanos(self) -> int:
        """Get total nanoseconds."""
        return self.nanoseconds
    
    def total_millis(self) -> float:
        """Get total milliseconds."""
        return self.nanoseconds / 1_000_000
    
    def total_seconds(self) -> float:
        """Get total seconds."""
        return self.nanoseconds / 1_000_000_000
    
    def total_minutes(self) -> float:
        """Get total minutes."""
        return self.nanoseconds / (60 * 1_000_000_000)
    
    def total_hours(self) -> float:
        """Get total hours."""
        return self.nanoseconds / (3600 * 1_000_000_000)
    
    def total_days(self) -> float:
        """Get total days."""
        return self.nanoseconds / (86400 * 1_000_000_000)
    
    def __str__(self) -> str:
        """Return ISO 8601 string representation."""
        return self.to_iso8601()
    
    def __repr__(self) -> str:
        """Return Python representation."""
        return f"Duration({self.nanoseconds})"
    
    def __eq__(self, other) -> bool:
        """Compare equality."""
        if isinstance(other, Duration):
            return self.nanoseconds == other.nanoseconds
        return False
    
    def __hash__(self) -> int:
        """Return hash value."""
        return hash(self.nanoseconds)
    
    def __add__(self, other) -> 'Duration':
        """Add durations."""
        if isinstance(other, Duration):
            return Duration(self.nanoseconds + other.nanoseconds)
        return NotImplemented
    
    def __sub__(self, other) -> 'Duration':
        """Subtract durations."""
        if isinstance(other, Duration):
            return Duration(self.nanoseconds - other.nanoseconds)
        return NotImplemented
    
    def __mul__(self, scalar: Union[int, float]) -> 'Duration':
        """Multiply duration by scalar."""
        if isinstance(scalar, (int, float)):
            return Duration(int(self.nanoseconds * scalar))
        return NotImplemented
    
    def __truediv__(self, scalar: Union[int, float]) -> 'Duration':
        """Divide duration by scalar."""
        if isinstance(scalar, (int, float)):
            return Duration(int(self.nanoseconds / scalar))
        return NotImplemented
    
    def __neg__(self) -> 'Duration':
        """Negate duration."""
        return Duration(-self.nanoseconds)
    
    def __abs__(self) -> 'Duration':
        """Absolute value of duration."""
        return Duration(abs(self.nanoseconds))


# Legacy Date class for backward compatibility
class Date:
    """DEPRECATED: Use Instant instead. Legacy Date type with timezone offset support."""
    
    def __init__(self, dt: Union[datetime, str], tz_offset: Optional[int] = None):
        """Initialize Date from datetime or ISO string.
        
        Args:
            dt: datetime object or ISO 8601 string
            tz_offset: timezone offset in minutes (e.g., -480 for PST)
        """
        import warnings
        warnings.warn("Date class is deprecated. Use Instant instead.", DeprecationWarning, stacklevel=2)
        
        if isinstance(dt, str):
            # Parse ISO 8601 string
            if dt.endswith('Z'):
                self.utc = datetime.fromisoformat(dt[:-1]).replace(tzinfo=timezone.utc)
                self.tz_offset = None
            elif '+' in dt or dt.count('-') > 2:  # Has timezone
                # Parse with timezone
                self.utc = datetime.fromisoformat(dt).astimezone(timezone.utc)
                # Calculate offset in minutes
                local_dt = datetime.fromisoformat(dt)
                offset = local_dt.utcoffset()
                if offset:
                    self.tz_offset = int(offset.total_seconds() / 60)
                else:
                    self.tz_offset = None
            else:
                # No timezone, assume UTC
                self.utc = datetime.fromisoformat(dt).replace(tzinfo=timezone.utc)
                self.tz_offset = None
        else:
            # From datetime object
            if dt.tzinfo is None:
                # Naive datetime, assume UTC
                self.utc = dt.replace(tzinfo=timezone.utc)
                self.tz_offset = None
            else:
                # Convert to UTC
                self.utc = dt.astimezone(timezone.utc)
                if tz_offset is not None:
                    self.tz_offset = tz_offset
                else:
                    # Calculate offset from original timezone
                    offset = dt.utcoffset()
                    if offset and offset.total_seconds() != 0:
                        self.tz_offset = int(offset.total_seconds() / 60)
                    else:
                        self.tz_offset = None
    
    def to_iso8601(self) -> str:
        """Convert to ISO 8601 string."""
        if self.tz_offset is None:
            # UTC - use Z suffix
            return self.utc.strftime("%Y-%m-%dT%H:%M:%SZ")
        else:
            # Convert to timezone with offset
            offset_seconds = self.tz_offset * 60
            tz = timezone(timedelta(seconds=offset_seconds))
            local_dt = self.utc.astimezone(tz)
            return local_dt.isoformat()
    
    def __str__(self) -> str:
        """Return ISO 8601 string representation."""
        return self.to_iso8601()
    
    def __repr__(self) -> str:
        """Return Python representation."""
        return f"Date('{self.to_iso8601()}')"
    
    def __eq__(self, other) -> bool:
        """Compare equality."""
        if isinstance(other, Date):
            return self.utc == other.utc and self.tz_offset == other.tz_offset
        return False
    
    def __hash__(self) -> int:
        """Return hash value."""
        return hash((self.utc, self.tz_offset))


def uuid_v4() -> uuid.UUID:
    """Generate a UUID v4 (random)."""
    return uuid.uuid4()


def uuid_v7() -> uuid.UUID:
    """Generate a UUID v7 (timestamp-based)."""
    # Get current timestamp in milliseconds
    now_ms = int(time.time() * 1000)
    
    # Create bytes array
    uuid_bytes = bytearray(16)
    
    # Set timestamp (48 bits)
    uuid_bytes[0] = (now_ms >> 40) & 0xff
    uuid_bytes[1] = (now_ms >> 32) & 0xff
    uuid_bytes[2] = (now_ms >> 24) & 0xff
    uuid_bytes[3] = (now_ms >> 16) & 0xff
    uuid_bytes[4] = (now_ms >> 8) & 0xff
    uuid_bytes[5] = now_ms & 0xff
    
    # Set random bits
    random_bytes = secrets.token_bytes(10)
    uuid_bytes[6:] = random_bytes
    
    # Set version (7) and variant bits
    uuid_bytes[6] = (uuid_bytes[6] & 0x0f) | 0x70  # Version 7
    uuid_bytes[8] = (uuid_bytes[8] & 0x3f) | 0x80  # Variant 10
    
    return uuid.UUID(bytes=bytes(uuid_bytes))