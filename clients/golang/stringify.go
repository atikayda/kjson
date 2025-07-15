package kjson

import (
	"fmt"
	"reflect"
	"strconv"
	"strings"
	"time"

	"github.com/google/uuid"
)

// stringify converts a Value to kJSON bytes.
func stringify(value *Value) ([]byte, error) {
	s, err := stringifyValue(value)
	if err != nil {
		return nil, err
	}
	return []byte(s), nil
}

// stringifyValue converts a Value to kJSON string.
func stringifyValue(value *Value) (string, error) {
	switch value.Type {
	case TypeNull:
		return "null", nil
	case TypeBool:
		if value.Bool {
			return "true", nil
		}
		return "false", nil
	case TypeNumber:
		return strconv.FormatFloat(value.Number, 'g', -1, 64), nil
	case TypeString:
		return stringifyString(value.String), nil
	case TypeBigInt:
		return value.BigInt.String() + "n", nil
	case TypeDecimal128:
		return value.Decimal.String() + "m", nil
	case TypeUUID:
		return value.UUID.String(), nil
	case TypeDate:
		return value.Date.String(), nil
	case TypeArray:
		return stringifyArray(value.Array)
	case TypeObject:
		return stringifyObject(value.Object)
	default:
		return "", fmt.Errorf("unknown value type: %v", value.Type)
	}
}

// stringifyString escapes and quotes a string using smart quote selection.
func stringifyString(s string) string {
	// Count occurrences of each quote type
	singleQuotes := strings.Count(s, "'")
	doubleQuotes := strings.Count(s, `"`)
	backticks := strings.Count(s, "`")
	backslashes := strings.Count(s, "\\")
	
	// Calculate escaping cost for each quote type
	singleCost := singleQuotes + backslashes
	doubleCost := doubleQuotes + backslashes
	backtickCost := backticks + backslashes
	
	// Find minimum cost and choose quote type
	// In case of tie: single > double > backtick
	var quoteChar rune
	minCost := singleCost
	quoteChar = '\''
	
	if doubleCost < minCost {
		minCost = doubleCost
		quoteChar = '"'
	}
	
	if backtickCost < minCost {
		quoteChar = '`'
	}
	
	// Escape the string for the chosen quote type
	escaped := escapeString(s, quoteChar)
	
	return string(quoteChar) + escaped + string(quoteChar)
}

// escapeString escapes a string for the given quote character.
func escapeString(s string, quote rune) string {
	// Handle backslashes first
	escaped := strings.ReplaceAll(s, "\\", "\\\\")
	
	// Handle control characters
	escaped = strings.ReplaceAll(escaped, "\b", "\\b")
	escaped = strings.ReplaceAll(escaped, "\f", "\\f")
	escaped = strings.ReplaceAll(escaped, "\n", "\\n")
	escaped = strings.ReplaceAll(escaped, "\r", "\\r")
	escaped = strings.ReplaceAll(escaped, "\t", "\\t")
	
	// Handle the chosen quote character
	switch quote {
	case '\'':
		escaped = strings.ReplaceAll(escaped, "'", "\\'")
	case '"':
		escaped = strings.ReplaceAll(escaped, `"`, `\\"`)
	case '`':
		escaped = strings.ReplaceAll(escaped, "`", "\\`")
	}
	
	return escaped
}

// stringifyArray converts an array to kJSON string.
func stringifyArray(arr []*Value) (string, error) {
	if len(arr) == 0 {
		return "[]", nil
	}
	
	var parts []string
	for _, item := range arr {
		str, err := stringifyValue(item)
		if err != nil {
			return "", err
		}
		parts = append(parts, str)
	}
	
	return "[" + strings.Join(parts, ",") + "]", nil
}

// stringifyObject converts an object to kJSON string.
func stringifyObject(obj map[string]*Value) (string, error) {
	if len(obj) == 0 {
		return "{}", nil
	}
	
	var parts []string
	for key, value := range obj {
		keyStr := quoteKey(key)
		valueStr, err := stringifyValue(value)
		if err != nil {
			return "", err
		}
		parts = append(parts, keyStr+":"+valueStr)
	}
	
	return "{" + strings.Join(parts, ",") + "}", nil
}

// quoteKey quotes a key if necessary (JSON5 style unquoted keys).
func quoteKey(key string) string {
	// Check if key can be unquoted (simple identifier)
	if isValidUnquotedKey(key) {
		return key
	}
	return stringifyString(key)
}

// isValidUnquotedKey checks if a key can be used without quotes.
func isValidUnquotedKey(key string) bool {
	if len(key) == 0 {
		return false
	}
	
	// First character must be letter, underscore, or dollar sign
	first := rune(key[0])
	if !isLetter(first) && first != '_' && first != '$' {
		return false
	}
	
	// Subsequent characters can be letters, digits, underscores, or dollar signs
	for _, r := range key[1:] {
		if !isLetter(r) && !isDigitRune(r) && r != '_' && r != '$' {
			return false
		}
	}
	
	return true
}

// isLetter checks if a rune is a letter.
func isLetter(r rune) bool {
	return (r >= 'a' && r <= 'z') || (r >= 'A' && r <= 'Z')
}

// isDigitRune checks if a rune is a digit.
func isDigitRune(r rune) bool {
	return r >= '0' && r <= '9'
}

// toKJsonValue converts a Go value to a kJSON Value.
func toKJsonValue(v interface{}) (*Value, error) {
	if v == nil {
		return &Value{Type: TypeNull}, nil
	}
	
	rv := reflect.ValueOf(v)
	return toKJsonValueReflect(rv)
}

// toKJsonValueReflect converts a reflect.Value to a kJSON Value.
func toKJsonValueReflect(rv reflect.Value) (*Value, error) {
	// Handle pointers
	for rv.Kind() == reflect.Ptr {
		if rv.IsNil() {
			return &Value{Type: TypeNull}, nil
		}
		rv = rv.Elem()
	}
	
	// Handle interfaces
	if rv.Kind() == reflect.Interface {
		if rv.IsNil() {
			return &Value{Type: TypeNull}, nil
		}
		rv = rv.Elem()
	}
	
	// Check for special types first before generic kinds
	if rv.Type() == reflect.TypeOf(time.Time{}) {
		t := rv.Interface().(time.Time)
		return &Value{Type: TypeDate, Date: NewDate(t)}, nil
	}
	
	if rv.Type() == reflect.TypeOf(uuid.UUID{}) {
		u := rv.Interface().(uuid.UUID)
		return &Value{Type: TypeUUID, UUID: u}, nil
	}
	
	if rv.Type() == reflect.TypeOf(BigInt{}) {
		b := rv.Interface().(BigInt)
		return &Value{Type: TypeBigInt, BigInt: &b}, nil
	}
	
	if rv.Type() == reflect.TypeOf(&BigInt{}) {
		if rv.IsNil() {
			return &Value{Type: TypeNull}, nil
		}
		b := rv.Interface().(*BigInt)
		return &Value{Type: TypeBigInt, BigInt: b}, nil
	}
	
	if rv.Type() == reflect.TypeOf(Decimal128{}) {
		d := rv.Interface().(Decimal128)
		return &Value{Type: TypeDecimal128, Decimal: &d}, nil
	}
	
	if rv.Type() == reflect.TypeOf(&Decimal128{}) {
		if rv.IsNil() {
			return &Value{Type: TypeNull}, nil
		}
		d := rv.Interface().(*Decimal128)
		return &Value{Type: TypeDecimal128, Decimal: d}, nil
	}
	
	if rv.Type() == reflect.TypeOf(Date{}) {
		d := rv.Interface().(Date)
		return &Value{Type: TypeDate, Date: &d}, nil
	}
	
	if rv.Type() == reflect.TypeOf(&Date{}) {
		if rv.IsNil() {
			return &Value{Type: TypeNull}, nil
		}
		d := rv.Interface().(*Date)
		return &Value{Type: TypeDate, Date: d}, nil
	}

	switch rv.Kind() {
	case reflect.Bool:
		return &Value{Type: TypeBool, Bool: rv.Bool()}, nil
		
	case reflect.Int, reflect.Int8, reflect.Int16, reflect.Int32, reflect.Int64:
		n := rv.Int()
		return &Value{Type: TypeNumber, Number: float64(n)}, nil
		
	case reflect.Uint, reflect.Uint8, reflect.Uint16, reflect.Uint32, reflect.Uint64:
		n := rv.Uint()
		return &Value{Type: TypeNumber, Number: float64(n)}, nil
		
	case reflect.Float32, reflect.Float64:
		return &Value{Type: TypeNumber, Number: rv.Float()}, nil
		
	case reflect.String:
		return &Value{Type: TypeString, String: rv.String()}, nil
		
	case reflect.Slice, reflect.Array:
		return toKJsonArray(rv)
		
	case reflect.Map:
		return toKJsonObject(rv)
		
	case reflect.Struct:
		return toKJsonStruct(rv)
		
	default:
		return nil, fmt.Errorf("unsupported type: %v", rv.Type())
	}
}

// toKJsonArray converts a slice/array to kJSON array.
func toKJsonArray(rv reflect.Value) (*Value, error) {
	length := rv.Len()
	arr := make([]*Value, length)
	
	for i := 0; i < length; i++ {
		item, err := toKJsonValueReflect(rv.Index(i))
		if err != nil {
			return nil, err
		}
		arr[i] = item
	}
	
	return &Value{Type: TypeArray, Array: arr}, nil
}

// toKJsonObject converts a map to kJSON object.
func toKJsonObject(rv reflect.Value) (*Value, error) {
	if rv.Type().Key().Kind() != reflect.String {
		return nil, fmt.Errorf("object keys must be strings")
	}
	
	obj := make(map[string]*Value)
	
	for _, key := range rv.MapKeys() {
		value := rv.MapIndex(key)
		
		kJsonValue, err := toKJsonValueReflect(value)
		if err != nil {
			return nil, err
		}
		
		obj[key.String()] = kJsonValue
	}
	
	return &Value{Type: TypeObject, Object: obj}, nil
}

// toKJsonStruct converts a struct to kJSON object.
func toKJsonStruct(rv reflect.Value) (*Value, error) {
	rt := rv.Type()
	obj := make(map[string]*Value)
	
	for i := 0; i < rt.NumField(); i++ {
		field := rt.Field(i)
		fieldValue := rv.Field(i)
		
		// Skip unexported fields
		if !fieldValue.CanInterface() {
			continue
		}
		
		// Get field name from tags
		fieldName, shouldInclude := getStructTag(field)
		if !shouldInclude {
			continue
		}
		
		// Check omitempty
		if isOmitEmpty(field) && isEmptyValue(fieldValue) {
			continue
		}
		
		kJsonValue, err := toKJsonValueReflect(fieldValue)
		if err != nil {
			return nil, err
		}
		
		obj[fieldName] = kJsonValue
	}
	
	return &Value{Type: TypeObject, Object: obj}, nil
}

// isEmptyValue checks if a reflect.Value represents an empty value.
func isEmptyValue(rv reflect.Value) bool {
	switch rv.Kind() {
	case reflect.Array, reflect.Map, reflect.Slice, reflect.String:
		return rv.Len() == 0
	case reflect.Bool:
		return !rv.Bool()
	case reflect.Int, reflect.Int8, reflect.Int16, reflect.Int32, reflect.Int64:
		return rv.Int() == 0
	case reflect.Uint, reflect.Uint8, reflect.Uint16, reflect.Uint32, reflect.Uint64, reflect.Uintptr:
		return rv.Uint() == 0
	case reflect.Float32, reflect.Float64:
		return rv.Float() == 0
	case reflect.Interface, reflect.Ptr:
		return rv.IsNil()
	}
	return false
}