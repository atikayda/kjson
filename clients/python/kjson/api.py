"""Main API functions for kJSON."""

from typing import Any, Optional, Union, Type
from .parser import parse, JSONDecodeError as _JSONDecodeError
from .serializer import dumps as _dumps


# Re-export JSONDecodeError
JSONDecodeError = _JSONDecodeError


class JSONEncoder:
    """kJSON encoder class (for compatibility with json module)."""
    
    def __init__(
        self,
        *,
        skipkeys: bool = False,
        ensure_ascii: bool = False,
        check_circular: bool = True,
        allow_nan: bool = True,
        sort_keys: bool = False,
        indent: Optional[Union[int, str]] = None,
        separators: Optional[tuple] = None,
        default: Optional[callable] = None
    ):
        """Initialize encoder."""
        self.skipkeys = skipkeys
        self.ensure_ascii = ensure_ascii
        self.check_circular = check_circular
        self.allow_nan = allow_nan
        self.sort_keys = sort_keys
        self.indent = indent
        self.separators = separators
        self.default = default
    
    def encode(self, obj: Any) -> str:
        """Encode object to kJSON string."""
        return _dumps(
            obj,
            skipkeys=self.skipkeys,
            ensure_ascii=self.ensure_ascii,
            check_circular=self.check_circular,
            allow_nan=self.allow_nan,
            indent=self.indent,
            separators=self.separators,
            default=self.default,
            sort_keys=self.sort_keys
        )
    
    def iterencode(self, obj: Any, _one_shot: bool = False):
        """Encode object to kJSON string iteratively (yields single string for compatibility)."""
        yield self.encode(obj)


class JSONDecoder:
    """kJSON decoder class (for compatibility with json module)."""
    
    def __init__(
        self,
        *,
        object_hook: Optional[callable] = None,
        parse_float: Optional[callable] = None,
        parse_int: Optional[callable] = None,
        parse_constant: Optional[callable] = None,
        strict: bool = True,
        object_pairs_hook: Optional[callable] = None
    ):
        """Initialize decoder."""
        self.object_hook = object_hook
        self.parse_float = parse_float
        self.parse_int = parse_int
        self.parse_constant = parse_constant
        self.strict = strict
        self.object_pairs_hook = object_pairs_hook
    
    def decode(self, s: str) -> Any:
        """Decode kJSON string."""
        result = parse(s)
        
        # Apply hooks if provided
        if self.object_hook is not None:
            result = self._apply_object_hook(result)
        
        return result
    
    def _apply_object_hook(self, obj: Any) -> Any:
        """Apply object hook recursively."""
        if isinstance(obj, dict):
            # Apply to nested objects first
            obj = {k: self._apply_object_hook(v) for k, v in obj.items()}
            # Then apply hook to this object
            return self.object_hook(obj)
        elif isinstance(obj, list):
            return [self._apply_object_hook(item) for item in obj]
        else:
            return obj


def loads(
    s: str,
    *,
    cls: Optional[Type[JSONDecoder]] = None,
    object_hook: Optional[callable] = None,
    parse_float: Optional[callable] = None,
    parse_int: Optional[callable] = None,
    parse_constant: Optional[callable] = None,
    object_pairs_hook: Optional[callable] = None,
    **kw
) -> Any:
    """Deserialize kJSON text to a Python object.
    
    Args:
        s: kJSON text to parse
        cls: Custom decoder class
        object_hook: Function called with dict objects
        parse_float: Function to parse floats (ignored - kJSON handles extended types)
        parse_int: Function to parse integers (ignored - kJSON handles BigInt)
        parse_constant: Function to parse constants (ignored)
        object_pairs_hook: Function called with object pairs (ignored)
        **kw: Additional keyword arguments (ignored)
    
    Returns:
        Python object
    """
    if cls is None:
        decoder = JSONDecoder(
            object_hook=object_hook,
            parse_float=parse_float,
            parse_int=parse_int,
            parse_constant=parse_constant,
            object_pairs_hook=object_pairs_hook
        )
    else:
        decoder = cls(
            object_hook=object_hook,
            parse_float=parse_float,
            parse_int=parse_int,
            parse_constant=parse_constant,
            object_pairs_hook=object_pairs_hook
        )
    
    return decoder.decode(s)


def dumps(
    obj: Any,
    *,
    skipkeys: bool = False,
    ensure_ascii: bool = False,
    check_circular: bool = True,
    allow_nan: bool = True,
    cls: Optional[Type[JSONEncoder]] = None,
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
        cls: Custom encoder class
        indent: Indentation level (int or string)
        separators: Item and key separators (ignored, always uses kJSON format)
        default: Function to convert non-serializable objects
        sort_keys: Sort object keys
        **kw: Additional keyword arguments (ignored)
    
    Returns:
        kJSON formatted string
    """
    if cls is None:
        return _dumps(
            obj,
            skipkeys=skipkeys,
            ensure_ascii=ensure_ascii,
            check_circular=check_circular,
            allow_nan=allow_nan,
            indent=indent,
            separators=separators,
            default=default,
            sort_keys=sort_keys
        )
    else:
        encoder = cls(
            skipkeys=skipkeys,
            ensure_ascii=ensure_ascii,
            check_circular=check_circular,
            allow_nan=allow_nan,
            sort_keys=sort_keys,
            indent=indent,
            separators=separators,
            default=default
        )
        return encoder.encode(obj)