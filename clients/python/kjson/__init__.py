"""kJSON (Kind JSON) - Extended JSON with BigInt, Decimal128, UUID, Instant, and Duration support."""

from .api import dumps, loads, JSONDecodeError, JSONEncoder, JSONDecoder
from .types import BigInt, Decimal128, Instant, Duration, Date, uuid_v4, uuid_v7

__version__ = "0.1.0"
__all__ = [
    "loads",
    "dumps",
    "JSONDecodeError",
    "JSONEncoder",
    "JSONDecoder",
    "BigInt",
    "Decimal128",
    "Instant",
    "Duration",
    "Date",
    "uuid_v4",
    "uuid_v7",
]