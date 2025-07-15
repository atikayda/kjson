"""kJSON serializer implementation."""

import json
import uuid
from typing import Any, Dict, Optional, Union
from .types import BigInt, Decimal128, Date


def serialize_string(s: str, ensure_ascii: bool = True) -> str:
    """Serialize string with smart quote selection."""
    # Count occurrences of each quote type
    single_quotes = s.count("'")
    double_quotes = s.count('"')
    backticks = s.count('`')
    backslashes = s.count('\\')
    
    # Calculate escaping cost for each quote type
    single_cost = single_quotes + backslashes
    double_cost = double_quotes + backslashes
    backtick_cost = backticks + backslashes
    
    # Find minimum cost and choose quote type
    # In case of tie: single > double > backtick
    min_cost = single_cost
    quote_char = "'"
    
    if double_cost < min_cost:
        min_cost = double_cost
        quote_char = '"'
    
    if backtick_cost < min_cost:
        quote_char = '`'
    
    # Escape the string for the chosen quote type
    escaped = escape_string(s, quote_char, ensure_ascii)
    
    return quote_char + escaped + quote_char


def escape_string(s: str, quote_char: str, ensure_ascii: bool = True) -> str:
    """Escape a string for the given quote character."""
    # Handle backslashes first
    escaped = s.replace('\\', '\\\\')
    
    # Handle control characters
    escaped = escaped.replace('\b', '\\b')
    escaped = escaped.replace('\f', '\\f')
    escaped = escaped.replace('\n', '\\n')
    escaped = escaped.replace('\r', '\\r')
    escaped = escaped.replace('\t', '\\t')
    
    # Handle the chosen quote character
    if quote_char == "'":
        escaped = escaped.replace("'", "\\'")
    elif quote_char == '"':
        escaped = escaped.replace('"', '\\"')
    elif quote_char == '`':
        escaped = escaped.replace('`', '\\`')
    
    # Handle Unicode escaping if needed
    if ensure_ascii:
        # Use Python's built-in encoding to handle unicode escapes
        encoded = escaped.encode('unicode_escape').decode('ascii')
        # Fix double-escaping of backslashes
        encoded = encoded.replace('\\\\', '\\')
        return encoded
    
    return escaped


def serialize_value(
    obj: Any,
    indent: int = 0,
    current_indent: int = 0,
    sort_keys: bool = False,
    ensure_ascii: bool = True
) -> str:
    """Serialize a value to kJSON format."""
    if obj is None:
        return "null"
    
    elif isinstance(obj, bool):
        return "true" if obj else "false"
    
    elif isinstance(obj, BigInt):
        return obj.to_kjson_string()
    
    elif isinstance(obj, Decimal128):
        return obj.to_kjson_string()
    
    elif isinstance(obj, Date):
        return obj.to_iso8601()
    
    elif isinstance(obj, uuid.UUID):
        return str(obj)
    
    elif isinstance(obj, (int, float)):
        # Check for special float values
        if isinstance(obj, float):
            if obj != obj:  # NaN
                return "null"
            elif obj == float('inf'):
                return "null"
            elif obj == float('-inf'):
                return "null"
        return json.dumps(obj)
    
    elif isinstance(obj, str):
        # Use smart quote selection
        return serialize_string(obj, ensure_ascii)
    
    elif isinstance(obj, (list, tuple)):
        return serialize_array(obj, indent, current_indent, sort_keys, ensure_ascii)
    
    elif isinstance(obj, dict):
        return serialize_object(obj, indent, current_indent, sort_keys, ensure_ascii)
    
    else:
        # For other types, try to convert to dict
        if hasattr(obj, '__dict__'):
            return serialize_object(obj.__dict__, indent, current_indent, sort_keys, ensure_ascii)
        else:
            # Fallback to string representation
            return json.dumps(str(obj), ensure_ascii=ensure_ascii)


def serialize_array(
    arr: Union[list, tuple],
    indent: int,
    current_indent: int,
    sort_keys: bool,
    ensure_ascii: bool
) -> str:
    """Serialize array to kJSON format."""
    if not arr:
        return "[]"
    
    if indent > 0:
        # Pretty print
        inner_indent = current_indent + indent
        items = []
        
        for item in arr:
            item_str = serialize_value(item, indent, inner_indent, sort_keys, ensure_ascii)
            items.append(' ' * inner_indent + item_str)
        
        return '[\n' + ',\n'.join(items) + '\n' + ' ' * current_indent + ']'
    else:
        # Compact
        items = [serialize_value(item, 0, 0, sort_keys, ensure_ascii) for item in arr]
        return '[' + ', '.join(items) + ']'


def serialize_object(
    obj: Dict[str, Any],
    indent: int,
    current_indent: int,
    sort_keys: bool,
    ensure_ascii: bool
) -> str:
    """Serialize object to kJSON format."""
    if not obj:
        return "{}"
    
    keys = sorted(obj.keys()) if sort_keys else obj.keys()
    
    if indent > 0:
        # Pretty print
        inner_indent = current_indent + indent
        items = []
        
        for key in keys:
            # Use unquoted keys if they're valid identifiers
            if key.isidentifier() and not key.startswith('$'):
                key_str = key
            else:
                key_str = serialize_string(key, ensure_ascii)
            
            value = obj[key]
            value_str = serialize_value(value, indent, inner_indent, sort_keys, ensure_ascii)
            items.append(f"{' ' * inner_indent}{key_str}: {value_str}")
        
        return '{\n' + ',\n'.join(items) + '\n' + ' ' * current_indent + '}'
    else:
        # Compact
        items = []
        for key in keys:
            # Use unquoted keys if they're valid identifiers
            if key.isidentifier() and not key.startswith('$'):
                key_str = key
            else:
                key_str = serialize_string(key, ensure_ascii)
            
            value = obj[key]
            value_str = serialize_value(value, 0, 0, sort_keys, ensure_ascii)
            items.append(f"{key_str}: {value_str}")
        
        return '{' + ', '.join(items) + '}'


def dumps(
    obj: Any,
    *,
    skipkeys: bool = False,
    ensure_ascii: bool = False,
    check_circular: bool = True,
    allow_nan: bool = True,
    cls: Optional[type] = None,
    indent: Optional[Union[int, str]] = None,
    separators: Optional[tuple] = None,
    default: Optional[callable] = None,
    sort_keys: bool = False,
    **kw
) -> str:
    """Serialize obj to a kJSON formatted string.
    
    Args:
        obj: Object to serialize
        skipkeys: Skip keys that are not basic types (ignored for compatibility)
        ensure_ascii: Escape non-ASCII characters
        check_circular: Check for circular references (ignored for compatibility)
        allow_nan: Allow NaN/Infinity values (converted to null)
        cls: Custom encoder class (ignored for compatibility)
        indent: Indentation level (int or string)
        separators: Item and key separators (ignored, always uses kJSON format)
        default: Function to convert non-serializable objects
        sort_keys: Sort object keys
        **kw: Additional keyword arguments (ignored)
    
    Returns:
        kJSON formatted string
    """
    # Convert indent to int
    indent_level = 0
    if indent is not None:
        if isinstance(indent, str):
            indent_level = len(indent)
        else:
            indent_level = int(indent)
    
    # Apply default function if provided
    if default is not None:
        obj = _apply_default(obj, default)
    
    return serialize_value(obj, indent_level, 0, sort_keys, ensure_ascii)


def _apply_default(obj: Any, default: callable) -> Any:
    """Apply default function to non-serializable objects."""
    if obj is None or isinstance(obj, (bool, int, float, str, BigInt, Decimal128, Date, uuid.UUID)):
        return obj
    elif isinstance(obj, (list, tuple)):
        return [_apply_default(item, default) for item in obj]
    elif isinstance(obj, dict):
        return {k: _apply_default(v, default) for k, v in obj.items()}
    else:
        try:
            return default(obj)
        except TypeError:
            return obj