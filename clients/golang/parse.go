package kjson

import (
	"fmt"
	"strconv"
	"strings"
	"time"
	"unicode"

	"github.com/google/uuid"
)

// ParseError represents a parsing error.
type ParseError struct {
	Offset int
	Msg    string
}

func (e *ParseError) Error() string {
	return fmt.Sprintf("kjson parse error at offset %d: %s", e.Offset, e.Msg)
}

// Parser state
type parser struct {
	data   string
	offset int
	length int
}

// parse parses a kJSON string and returns a Value.
func parse(data string) (*Value, error) {
	p := &parser{
		data:   data,
		offset: 0,
		length: len(data),
	}
	
	p.skipWhitespace()
	if p.offset >= p.length {
		return nil, &ParseError{p.offset, "unexpected end of input"}
	}
	
	value, err := p.parseValue()
	if err != nil {
		return nil, err
	}
	
	p.skipWhitespace()
	if p.offset < p.length {
		return nil, &ParseError{p.offset, "unexpected characters after kJSON value"}
	}
	
	return value, nil
}

// parseValue parses any kJSON value.
func (p *parser) parseValue() (*Value, error) {
	p.skipWhitespace()
	
	if p.offset >= p.length {
		return nil, &ParseError{p.offset, "unexpected end of input"}
	}
	
	c := p.data[p.offset]
	
	switch c {
	case 'n':
		return p.parseNull()
	case 't', 'f':
		// Check if this might be a UUID first
		if value, err := p.tryParseUnquotedLiteral(); err == nil {
			return value, nil
		}
		// If not, parse as boolean
		return p.parseBool()
	case '"', '\'', '`':
		return p.parseString()
	case '[':
		return p.parseArray()
	case '{':
		return p.parseObject()
	case '-', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9':
		// Check if this might be a UUID or Date first
		if value, err := p.tryParseUnquotedLiteral(); err == nil {
			return value, nil
		}
		// If not, parse as number
		return p.parseNumber()
	default:
		// Try parsing unquoted literals (UUID, Date)
		return p.parseUnquotedLiteral()
	}
}

// parseNull parses a null value.
func (p *parser) parseNull() (*Value, error) {
	if p.offset+4 > p.length || p.data[p.offset:p.offset+4] != "null" {
		return nil, &ParseError{p.offset, "invalid null value"}
	}
	p.offset += 4
	return &Value{Type: TypeNull}, nil
}

// parseBool parses a boolean value.
func (p *parser) parseBool() (*Value, error) {
	if p.offset+4 <= p.length && p.data[p.offset:p.offset+4] == "true" {
		p.offset += 4
		return &Value{Type: TypeBool, Bool: true}, nil
	}
	if p.offset+5 <= p.length && p.data[p.offset:p.offset+5] == "false" {
		p.offset += 5
		return &Value{Type: TypeBool, Bool: false}, nil
	}
	return nil, &ParseError{p.offset, "invalid boolean value"}
}

// parseString parses a quoted string with single quotes, double quotes, or backticks.
func (p *parser) parseString() (*Value, error) {
	quote := p.data[p.offset]
	if quote != '"' && quote != '\'' && quote != '`' {
		return nil, &ParseError{p.offset, "expected quote character"}
	}
	
	start := p.offset + 1
	p.offset++
	
	for p.offset < p.length {
		c := p.data[p.offset]
		if c == quote {
			str := p.data[start:p.offset]
			p.offset++
			// Unescape string
			unescaped, err := p.unescapeString(str, quote)
			if err != nil {
				return nil, err
			}
			return &Value{Type: TypeString, String: unescaped}, nil
		}
		if c == '\\' {
			p.offset += 2 // Skip escape sequence
		} else {
			p.offset++
		}
	}
	
	return nil, &ParseError{start-1, "unterminated string"}
}

// unescapeString unescapes a string based on the quote character used.
func (p *parser) unescapeString(s string, quote byte) (string, error) {
	// Handle escape sequences
	s = strings.ReplaceAll(s, "\\\\", "\\")
	s = strings.ReplaceAll(s, "\\/", "/")
	s = strings.ReplaceAll(s, "\\b", "\b")
	s = strings.ReplaceAll(s, "\\f", "\f")
	s = strings.ReplaceAll(s, "\\n", "\n")
	s = strings.ReplaceAll(s, "\\r", "\r")
	s = strings.ReplaceAll(s, "\\t", "\t")
	
	// Handle quote-specific escapes
	switch quote {
	case '"':
		s = strings.ReplaceAll(s, "\\\"", "\"")
	case '\'':
		s = strings.ReplaceAll(s, "\\'", "'")
	case '`':
		s = strings.ReplaceAll(s, "\\`", "`")
	}
	
	return s, nil
}

// parseArray parses an array.
func (p *parser) parseArray() (*Value, error) {
	if p.data[p.offset] != '[' {
		return nil, &ParseError{p.offset, "expected '['"}
	}
	p.offset++
	
	var items []*Value
	
	p.skipWhitespace()
	if p.offset < p.length && p.data[p.offset] == ']' {
		p.offset++
		return &Value{Type: TypeArray, Array: items}, nil
	}
	
	for {
		item, err := p.parseValue()
		if err != nil {
			return nil, err
		}
		items = append(items, item)
		
		p.skipWhitespace()
		if p.offset >= p.length {
			return nil, &ParseError{p.offset, "unterminated array"}
		}
		
		c := p.data[p.offset]
		if c == ']' {
			p.offset++
			break
		} else if c == ',' {
			p.offset++
			p.skipWhitespace()
		} else {
			return nil, &ParseError{p.offset, "expected ',' or ']'"}
		}
	}
	
	return &Value{Type: TypeArray, Array: items}, nil
}

// parseObject parses an object.
func (p *parser) parseObject() (*Value, error) {
	if p.data[p.offset] != '{' {
		return nil, &ParseError{p.offset, "expected '{'"}
	}
	p.offset++
	
	obj := make(map[string]*Value)
	
	p.skipWhitespace()
	if p.offset < p.length && p.data[p.offset] == '}' {
		p.offset++
		return &Value{Type: TypeObject, Object: obj}, nil
	}
	
	for {
		// Parse key
		p.skipWhitespace()
		var key string
		
		if p.offset < p.length && (p.data[p.offset] == '"' || p.data[p.offset] == '\'' || p.data[p.offset] == '`') {
			// Quoted key
			keyValue, err := p.parseString()
			if err != nil {
				return nil, err
			}
			key = keyValue.String
		} else {
			// Unquoted key (JSON5 style)
			var err error
			key, err = p.parseUnquotedKey()
			if err != nil {
				return nil, err
			}
		}
		
		// Parse colon
		p.skipWhitespace()
		if p.offset >= p.length || p.data[p.offset] != ':' {
			return nil, &ParseError{p.offset, "expected ':' after object key"}
		}
		p.offset++
		
		// Parse value
		value, err := p.parseValue()
		if err != nil {
			return nil, err
		}
		
		obj[key] = value
		
		p.skipWhitespace()
		if p.offset >= p.length {
			return nil, &ParseError{p.offset, "unterminated object"}
		}
		
		c := p.data[p.offset]
		if c == '}' {
			p.offset++
			break
		} else if c == ',' {
			p.offset++
			p.skipWhitespace()
		} else {
			return nil, &ParseError{p.offset, "expected ',' or '}'"}
		}
	}
	
	return &Value{Type: TypeObject, Object: obj}, nil
}

// parseUnquotedKey parses an unquoted object key (JSON5 style).
func (p *parser) parseUnquotedKey() (string, error) {
	start := p.offset
	
	if p.offset >= p.length {
		return "", &ParseError{p.offset, "expected object key"}
	}
	
	// First character must be letter, underscore, or dollar sign
	c := rune(p.data[p.offset])
	if !unicode.IsLetter(c) && c != '_' && c != '$' {
		return "", &ParseError{p.offset, "invalid unquoted key"}
	}
	
	p.offset++
	
	// Subsequent characters can be letters, digits, underscores, or dollar signs
	for p.offset < p.length {
		c := rune(p.data[p.offset])
		if !unicode.IsLetter(c) && !unicode.IsDigit(c) && c != '_' && c != '$' {
			break
		}
		p.offset++
	}
	
	return p.data[start:p.offset], nil
}

// parseNumber parses a number (regular number, BigInt, or Decimal128).
func (p *parser) parseNumber() (*Value, error) {
	start := p.offset
	
	// Skip negative sign
	if p.offset < p.length && p.data[p.offset] == '-' {
		p.offset++
	}
	
	// Parse digits
	if p.offset >= p.length || !isDigit(p.data[p.offset]) {
		return nil, &ParseError{p.offset, "invalid number"}
	}
	
	// Parse integer part
	if p.data[p.offset] == '0' {
		p.offset++
	} else {
		for p.offset < p.length && isDigit(p.data[p.offset]) {
			p.offset++
		}
	}
	
	// Check for decimal point
	hasDecimal := false
	if p.offset < p.length && p.data[p.offset] == '.' {
		hasDecimal = true
		p.offset++
		if p.offset >= p.length || !isDigit(p.data[p.offset]) {
			return nil, &ParseError{p.offset, "invalid decimal number"}
		}
		for p.offset < p.length && isDigit(p.data[p.offset]) {
			p.offset++
		}
	}
	
	// Check for exponent
	hasExponent := false
	if p.offset < p.length && (p.data[p.offset] == 'e' || p.data[p.offset] == 'E') {
		hasExponent = true
		p.offset++
		if p.offset < p.length && (p.data[p.offset] == '+' || p.data[p.offset] == '-') {
			p.offset++
		}
		if p.offset >= p.length || !isDigit(p.data[p.offset]) {
			return nil, &ParseError{p.offset, "invalid exponent"}
		}
		for p.offset < p.length && isDigit(p.data[p.offset]) {
			p.offset++
		}
	}
	
	// Check for BigInt suffix
	if p.offset < p.length && p.data[p.offset] == 'n' {
		p.offset++
		numStr := p.data[start:p.offset-1]
		return p.parseBigInt(numStr)
	}
	
	// Check for Decimal128 suffix
	if p.offset < p.length && p.data[p.offset] == 'm' {
		p.offset++
		numStr := p.data[start:p.offset-1]
		return p.parseDecimal128(numStr)
	}
	
	// Regular number
	numStr := p.data[start:p.offset]
	if hasDecimal || hasExponent {
		f, err := strconv.ParseFloat(numStr, 64)
		if err != nil {
			return nil, &ParseError{start, "invalid number: "+err.Error()}
		}
		return &Value{Type: TypeNumber, Number: f}, nil
	} else {
		// Integer - try to fit in float64
		f, err := strconv.ParseFloat(numStr, 64)
		if err != nil {
			return nil, &ParseError{start, "invalid number: "+err.Error()}
		}
		return &Value{Type: TypeNumber, Number: f}, nil
	}
}

// parseBigInt parses a BigInt from a number string.
func (p *parser) parseBigInt(numStr string) (*Value, error) {
	negative := false
	if strings.HasPrefix(numStr, "-") {
		negative = true
		numStr = numStr[1:]
	}
	
	// Validate digits
	for _, c := range numStr {
		if !isDigit(byte(c)) {
			return nil, &ParseError{p.offset, "invalid BigInt"}
		}
	}
	
	return &Value{
		Type: TypeBigInt,
		BigInt: &BigInt{
			Negative: negative,
			Digits:   numStr,
		},
	}, nil
}

// parseDecimal128 parses a Decimal128 from a number string.
func (p *parser) parseDecimal128(numStr string) (*Value, error) {
	d, err := NewDecimal128(numStr)
	if err != nil {
		return nil, &ParseError{p.offset, "invalid Decimal128: "+err.Error()}
	}
	
	return &Value{
		Type:    TypeDecimal128,
		Decimal: d,
	}, nil
}

// tryParseUnquotedLiteral attempts to parse unquoted literals without advancing on failure.
func (p *parser) tryParseUnquotedLiteral() (*Value, error) {
	savedOffset := p.offset
	value, err := p.parseUnquotedLiteral()
	if err != nil {
		p.offset = savedOffset // Reset on failure
		return nil, err
	}
	return value, nil
}

// parseUnquotedLiteral parses unquoted literals (UUID, Date).
func (p *parser) parseUnquotedLiteral() (*Value, error) {
	start := p.offset
	
	// Read until whitespace or delimiter
	// Don't stop at ':' for value parsing (needed for ISO dates)
	for p.offset < p.length {
		c := p.data[p.offset]
		if isWhitespace(c) || c == ',' || c == ']' || c == '}' {
			break
		}
		p.offset++
	}
	
	literal := p.data[start:p.offset]
	
	// Try to parse as UUID
	if u, err := uuid.Parse(literal); err == nil {
		return &Value{Type: TypeUUID, UUID: u}, nil
	}
	
	// Try to parse as Date (ISO 8601)
	if t, err := time.Parse(time.RFC3339, literal); err == nil {
		return &Value{Type: TypeDate, Date: NewDate(t)}, nil
	}
	
	// Try other time formats
	timeFormats := []string{
		time.RFC3339Nano,
		"2006-01-02T15:04:05Z",
		"2006-01-02T15:04:05.000Z",
	}
	
	for _, format := range timeFormats {
		if t, err := time.Parse(format, literal); err == nil {
			return &Value{Type: TypeDate, Date: NewDate(t)}, nil
		}
	}
	
	return nil, &ParseError{start, "invalid literal: "+literal}
}

// skipWhitespace skips whitespace characters.
func (p *parser) skipWhitespace() {
	for p.offset < p.length && isWhitespace(p.data[p.offset]) {
		p.offset++
	}
}

// isWhitespace checks if a character is whitespace.
func isWhitespace(c byte) bool {
	return c == ' ' || c == '\t' || c == '\n' || c == '\r'
}

// isDigit checks if a character is a digit.
func isDigit(c byte) bool {
	return c >= '0' && c <= '9'
}