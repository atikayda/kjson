"""kJSON parser implementation."""

import re
import uuid
from typing import Any, Dict, List, Optional, Union
from .types import BigInt, Decimal128, Date


class JSONDecodeError(ValueError):
    """JSON decoding error."""
    
    def __init__(self, msg: str, doc: str, pos: int):
        """Initialize with error details."""
        super().__init__(msg)
        self.msg = msg
        self.doc = doc
        self.pos = pos
        
        # Calculate line and column
        lines = doc[:pos].split('\n')
        self.lineno = len(lines)
        self.colno = len(lines[-1]) + 1 if lines else 1


class Parser:
    """kJSON parser with JSON5 support."""
    
    def __init__(self, text: str):
        """Initialize parser with text."""
        self.text = text
        self.pos = 0
        self.length = len(text)
    
    def parse(self) -> Any:
        """Parse the JSON text."""
        self.skip_whitespace()
        result = self.parse_value()
        self.skip_whitespace()
        
        if self.pos < self.length:
            raise JSONDecodeError(
                f"Extra data after JSON value",
                self.text,
                self.pos
            )
        
        return result
    
    def current_char(self) -> Optional[str]:
        """Get current character."""
        if self.pos < self.length:
            return self.text[self.pos]
        return None
    
    def peek_char(self, offset: int = 0) -> Optional[str]:
        """Peek at character at offset."""
        pos = self.pos + offset
        if pos < self.length:
            return self.text[pos]
        return None
    
    def advance(self, count: int = 1):
        """Advance position."""
        self.pos = min(self.pos + count, self.length)
    
    def skip_whitespace(self):
        """Skip whitespace and comments."""
        while self.pos < self.length:
            ch = self.text[self.pos]
            
            if ch in ' \t\n\r':
                self.advance()
            elif ch == '/' and self.peek_char(1) == '/':
                # Line comment
                self.advance(2)
                while self.pos < self.length and self.text[self.pos] != '\n':
                    self.advance()
            elif ch == '/' and self.peek_char(1) == '*':
                # Block comment
                self.advance(2)
                while self.pos < self.length - 1:
                    if self.text[self.pos] == '*' and self.text[self.pos + 1] == '/':
                        self.advance(2)
                        break
                    self.advance()
            else:
                break
    
    def parse_value(self) -> Any:
        """Parse any JSON value."""
        self.skip_whitespace()
        
        if self.pos >= self.length:
            raise JSONDecodeError("Unexpected end of JSON input", self.text, self.pos)
        
        ch = self.text[self.pos]
        
        # null
        if ch == 'n':
            return self.parse_null()
        
        # boolean or unquoted literal
        elif ch == 't' or ch == 'f':
            # Try unquoted literal first (could be UUID)
            saved_pos = self.pos
            try:
                return self.parse_unquoted_literal()
            except:
                self.pos = saved_pos
                return self.parse_boolean()
        
        # string
        elif ch == '"' or ch == "'" or ch == '`':
            return self.parse_string()
        
        # array
        elif ch == '[':
            return self.parse_array()
        
        # object
        elif ch == '{':
            return self.parse_object()
        
        # number or date/uuid
        elif ch == '-' or ch.isdigit():
            # Need to look ahead to determine if this is a number or UUID/Date
            # UUIDs have format: 8-4-4-4-12 hex digits
            # Dates have format: YYYY-MM-DD
            saved_pos = self.pos
            
            # Try to detect UUID pattern (8 hex chars followed by -)
            is_uuid = False
            if ch.isdigit() or (ch >= 'a' and ch <= 'f'):
                # Count hex digits
                hex_count = 0
                temp_pos = self.pos
                while temp_pos < self.length and hex_count < 9:
                    c = self.text[temp_pos]
                    if (c >= '0' and c <= '9') or (c >= 'a' and c <= 'f') or (c >= 'A' and c <= 'F'):
                        hex_count += 1
                        temp_pos += 1
                    elif c == '-' and hex_count == 8:
                        is_uuid = True
                        break
                    else:
                        break
            
            # Try to detect date pattern (YYYY-MM-DD)
            is_date = False
            if not is_uuid and ch.isdigit():
                # Check for YYYY-MM pattern
                if self.pos + 4 < self.length and self.text[self.pos + 4] == '-':
                    is_date = True
            
            if is_uuid or is_date:
                try:
                    return self.parse_unquoted_literal()
                except:
                    self.pos = saved_pos
                    return self.parse_number()
            else:
                return self.parse_number()
        
        # unquoted literal (UUID, Date)
        else:
            return self.parse_unquoted_literal()
    
    def parse_null(self) -> None:
        """Parse null value."""
        if self.text[self.pos:self.pos+4] == 'null':
            self.advance(4)
            return None
        raise JSONDecodeError("Invalid null value", self.text, self.pos)
    
    def parse_boolean(self) -> bool:
        """Parse boolean value."""
        if self.text[self.pos:self.pos+4] == 'true':
            self.advance(4)
            return True
        elif self.text[self.pos:self.pos+5] == 'false':
            self.advance(5)
            return False
        raise JSONDecodeError("Invalid boolean value", self.text, self.pos)
    
    def parse_string(self) -> str:
        """Parse string value."""
        quote = self.text[self.pos]
        if quote not in '"\'`':
            raise JSONDecodeError("Expected string quote", self.text, self.pos)
        
        self.advance()  # Skip opening quote
        result = []
        
        while self.pos < self.length:
            ch = self.text[self.pos]
            
            if ch == quote:
                self.advance()  # Skip closing quote
                return ''.join(result)
            
            elif ch == '\\':
                self.advance()
                if self.pos >= self.length:
                    raise JSONDecodeError("Unterminated string escape", self.text, self.pos)
                
                escape_ch = self.text[self.pos]
                if escape_ch == '"':
                    result.append('"')
                elif escape_ch == "'":
                    result.append("'")
                elif escape_ch == '`':
                    result.append('`')
                elif escape_ch == '\\':
                    result.append('\\')
                elif escape_ch == '/':
                    result.append('/')
                elif escape_ch == 'b':
                    result.append('\b')
                elif escape_ch == 'f':
                    result.append('\f')
                elif escape_ch == 'n':
                    result.append('\n')
                elif escape_ch == 'r':
                    result.append('\r')
                elif escape_ch == 't':
                    result.append('\t')
                elif escape_ch == 'u':
                    # Unicode escape
                    self.advance()
                    if self.pos + 4 > self.length:
                        raise JSONDecodeError("Invalid unicode escape", self.text, self.pos)
                    hex_digits = self.text[self.pos:self.pos+4]
                    try:
                        code_point = int(hex_digits, 16)
                        result.append(chr(code_point))
                        self.advance(3)  # We already advanced 1
                    except ValueError:
                        raise JSONDecodeError("Invalid unicode escape", self.text, self.pos)
                else:
                    raise JSONDecodeError(f"Invalid escape sequence: \\{escape_ch}", self.text, self.pos)
                
                self.advance()
            
            else:
                result.append(ch)
                self.advance()
        
        raise JSONDecodeError("Unterminated string", self.text, self.pos)
    
    def parse_number(self) -> Union[int, float, BigInt, Decimal128]:
        """Parse number value."""
        start = self.pos
        
        # Optional negative
        if self.current_char() == '-':
            self.advance()
        
        # Integer part
        if self.current_char() == '0':
            self.advance()
        elif self.current_char() and self.current_char().isdigit():
            while self.current_char() and self.current_char().isdigit():
                self.advance()
        else:
            raise JSONDecodeError("Invalid number", self.text, self.pos)
        
        # Fractional part
        has_fraction = False
        if self.current_char() == '.':
            has_fraction = True
            self.advance()
            if not self.current_char() or not self.current_char().isdigit():
                raise JSONDecodeError("Invalid number: expected digits after decimal", self.text, self.pos)
            while self.current_char() and self.current_char().isdigit():
                self.advance()
        
        # Exponent part
        has_exponent = False
        ch = self.current_char()
        if ch and ch in 'eE':
            has_exponent = True
            self.advance()
            ch = self.current_char()
            if ch and ch in '+-':
                self.advance()
            if not self.current_char() or not self.current_char().isdigit():
                raise JSONDecodeError("Invalid number: expected digits in exponent", self.text, self.pos)
            while self.current_char() and self.current_char().isdigit():
                self.advance()
        
        # Check for BigInt suffix
        if self.current_char() == 'n':
            self.advance()
            if has_fraction or has_exponent:
                raise JSONDecodeError("BigInt cannot have fractional or exponent parts", self.text, start)
            return BigInt(self.text[start:self.pos-1])
        
        # Check for Decimal128 suffix
        if self.current_char() == 'm':
            self.advance()
            return Decimal128(self.text[start:self.pos-1])
        
        # Regular number
        num_str = self.text[start:self.pos]
        if has_fraction or has_exponent:
            return float(num_str)
        else:
            value = int(num_str)
            # Keep as int if it fits in Python int range
            return value
    
    def parse_array(self) -> List[Any]:
        """Parse array value."""
        if self.current_char() != '[':
            raise JSONDecodeError("Expected '['", self.text, self.pos)
        
        self.advance()  # Skip '['
        result = []
        
        self.skip_whitespace()
        
        # Empty array
        if self.current_char() == ']':
            self.advance()
            return result
        
        while True:
            # Parse value
            result.append(self.parse_value())
            self.skip_whitespace()
            
            ch = self.current_char()
            if ch == ',':
                self.advance()
                self.skip_whitespace()
                # Allow trailing comma
                if self.current_char() == ']':
                    self.advance()
                    return result
            elif ch == ']':
                self.advance()
                return result
            else:
                raise JSONDecodeError("Expected ',' or ']' in array", self.text, self.pos)
    
    def parse_object(self) -> Dict[str, Any]:
        """Parse object value."""
        if self.current_char() != '{':
            raise JSONDecodeError("Expected '{'", self.text, self.pos)
        
        self.advance()  # Skip '{'
        result = {}
        
        self.skip_whitespace()
        
        # Empty object
        if self.current_char() == '}':
            self.advance()
            return result
        
        while True:
            # Parse key
            self.skip_whitespace()
            
            ch = self.current_char()
            if ch == '"' or ch == "'" or ch == '`':
                # Quoted key
                key = self.parse_string()
            else:
                # Unquoted key (JSON5)
                key = self.parse_unquoted_key()
            
            self.skip_whitespace()
            
            # Expect colon
            if self.current_char() != ':':
                raise JSONDecodeError("Expected ':' after object key", self.text, self.pos)
            self.advance()
            
            # Parse value
            value = self.parse_value()
            result[key] = value
            
            self.skip_whitespace()
            
            ch = self.current_char()
            if ch == ',':
                self.advance()
                self.skip_whitespace()
                # Allow trailing comma
                if self.current_char() == '}':
                    self.advance()
                    return result
            elif ch == '}':
                self.advance()
                return result
            else:
                raise JSONDecodeError("Expected ',' or '}' in object", self.text, self.pos)
    
    def parse_unquoted_key(self) -> str:
        """Parse unquoted object key (JSON5)."""
        start = self.pos
        
        # For object keys, we need to check if it could be a literal value
        # Try to parse as unquoted literal first
        saved_pos = self.pos
        try:
            # Check if it's a valid literal (UUID/Date)
            literal = self.parse_unquoted_literal()
            # If we got here, it's a literal value, not a key
            # This shouldn't happen in valid JSON - reset and treat as key
            self.pos = saved_pos
        except:
            # Not a literal, continue as key
            self.pos = saved_pos
        
        # First character must be letter, underscore, or dollar
        ch = self.current_char()
        if not ch or not (ch.isalpha() or ch in '_$'):
            raise JSONDecodeError("Invalid unquoted key", self.text, self.pos)
        
        self.advance()
        
        # Subsequent characters
        while self.current_char() and (self.current_char().isalnum() or self.current_char() in '_$' or self.current_char() == '-'):
            self.advance()
        
        return self.text[start:self.pos]
    
    def parse_unquoted_literal(self) -> Union[uuid.UUID, Date]:
        """Parse unquoted literal (UUID or Date)."""
        start = self.pos
        
        # Read until delimiter
        while self.pos < self.length:
            ch = self.text[self.pos]
            if ch in ' \t\n\r,]}':
                break
            self.advance()
        
        literal = self.text[start:self.pos]
        
        # Try UUID
        try:
            return uuid.UUID(literal)
        except ValueError:
            pass
        
        # Try Date
        try:
            return Date(literal)
        except:
            pass
        
        raise JSONDecodeError(f"Invalid unquoted literal: {literal}", self.text, start)


def parse(text: str) -> Any:
    """Parse kJSON text."""
    parser = Parser(text)
    return parser.parse()