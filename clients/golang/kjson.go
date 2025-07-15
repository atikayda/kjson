// Package kjson provides extended JSON support with BigInt, Decimal128, UUID, and Date types.
// It follows the same semantics as encoding/json but supports kjson struct tags.
package kjson

import (
	"fmt"
	"reflect"
	"regexp"
	"strconv"
	"strings"
	"time"

	"github.com/google/uuid"
)

// Marshal returns the kJSON encoding of v.
// It follows the same semantics as json.Marshal but supports kjson struct tags.
func Marshal(v interface{}) ([]byte, error) {
	value, err := toKJsonValue(v)
	if err != nil {
		return nil, err
	}
	return stringify(value)
}

// Unmarshal parses the kJSON-encoded data and stores the result in the value pointed to by v.
// It follows the same semantics as json.Unmarshal but supports kjson struct tags.
func Unmarshal(data []byte, v interface{}) error {
	value, err := parse(string(data))
	if err != nil {
		return err
	}
	return fromKJsonValue(value, v)
}

// MarshalIndent is like Marshal but applies Indent to format the output.
func MarshalIndent(v interface{}, prefix, indent string) ([]byte, error) {
	b, err := Marshal(v)
	if err != nil {
		return nil, err
	}
	// For now, return as-is. We can implement pretty printing later
	return b, nil
}

// BigInt represents an arbitrary precision integer for kJSON.
type BigInt struct {
	Negative bool
	Digits   string
}

// NewBigInt creates a BigInt from an int64.
func NewBigInt(n int64) *BigInt {
	negative := n < 0
	if negative {
		n = -n
	}
	return &BigInt{
		Negative: negative,
		Digits:   fmt.Sprintf("%d", n),
	}
}

// String returns the string representation of the BigInt.
func (b *BigInt) String() string {
	if b.Negative {
		return "-" + b.Digits
	}
	return b.Digits
}

// Decimal128 represents a high-precision decimal for kJSON.
// It stores the decimal as a string with sign and exponent information.
type Decimal128 struct {
	Negative bool
	Digits   string
	Exponent int32
}

// NewDecimal128 creates a Decimal128 from a string representation.
func NewDecimal128(s string) (*Decimal128, error) {
	// Parse the decimal string
	negative := false
	if strings.HasPrefix(s, "-") {
		negative = true
		s = s[1:]
	}
	
	// Find decimal point
	dotIndex := strings.Index(s, ".")
	exponent := int32(0)
	digits := s
	
	if dotIndex != -1 {
		// Has decimal point
		integerPart := s[:dotIndex]
		fractionalPart := s[dotIndex+1:]
		
		// Remove leading zeros from integer part
		integerPart = strings.TrimLeft(integerPart, "0")
		if integerPart == "" {
			integerPart = "0"
		}
		
		// Remove trailing zeros from fractional part and calculate exponent
		fractionalPart = strings.TrimRight(fractionalPart, "0")
		exponent = -int32(len(fractionalPart))
		
		if fractionalPart == "" {
			digits = integerPart
		} else {
			digits = integerPart + fractionalPart
		}
	}
	
	// Handle scientific notation
	if eIndex := strings.IndexAny(s, "eE"); eIndex != -1 {
		mantissa := s[:eIndex]
		expStr := s[eIndex+1:]
		
		exp, err := strconv.ParseInt(expStr, 10, 32)
		if err != nil {
			return nil, fmt.Errorf("invalid exponent: %s", expStr)
		}
		
		// Parse mantissa
		dotIndex := strings.Index(mantissa, ".")
		if dotIndex != -1 {
			integerPart := mantissa[:dotIndex]
			fractionalPart := mantissa[dotIndex+1:]
			digits = strings.TrimLeft(integerPart, "0") + fractionalPart
			exponent = int32(exp) - int32(len(fractionalPart))
		} else {
			digits = strings.TrimLeft(mantissa, "0")
			exponent = int32(exp)
		}
	}
	
	// Remove leading zeros
	digits = strings.TrimLeft(digits, "0")
	if digits == "" {
		digits = "0"
		exponent = 0
		negative = false
	}
	
	return &Decimal128{
		Negative: negative,
		Digits:   digits,
		Exponent: exponent,
	}, nil
}

// NewDecimal128FromFloat creates a Decimal128 from a float64.
func NewDecimal128FromFloat(f float64) *Decimal128 {
	s := strconv.FormatFloat(f, 'f', -1, 64)
	d, _ := NewDecimal128(s) // Should not error for valid float
	return d
}

// String returns the string representation of the Decimal128.
func (d *Decimal128) String() string {
	result := ""
	if d.Negative {
		result += "-"
	}
	
	// Simple formatting - can be enhanced later
	if d.Exponent == 0 {
		result += d.Digits
	} else if d.Exponent > 0 {
		result += d.Digits
		for i := int32(0); i < d.Exponent; i++ {
			result += "0"
		}
	} else {
		// Negative exponent - add decimal point
		if len(d.Digits) <= int(-d.Exponent) {
			result += "0."
			for i := 0; i < int(-d.Exponent)-len(d.Digits); i++ {
				result += "0"
			}
			result += d.Digits
		} else {
			pos := len(d.Digits) + int(d.Exponent)
			result += d.Digits[:pos] + "." + d.Digits[pos:]
		}
	}
	
	return result
}

// Instant represents a nanosecond-precision timestamp in Zulu time (UTC).
type Instant struct {
	Nanoseconds int64 // Nanoseconds since Unix epoch (UTC)
}

// NewInstant creates an Instant from nanoseconds since epoch.
func NewInstant(nanoseconds int64) *Instant {
	return &Instant{Nanoseconds: nanoseconds}
}

// NewInstantFromMillis creates an Instant from milliseconds since epoch.
func NewInstantFromMillis(milliseconds int64) *Instant {
	return &Instant{Nanoseconds: milliseconds * 1_000_000}
}

// NewInstantFromSeconds creates an Instant from seconds since epoch.
func NewInstantFromSeconds(seconds int64) *Instant {
	return &Instant{Nanoseconds: seconds * 1_000_000_000}
}

// NewInstantFromTime creates an Instant from a time.Time.
func NewInstantFromTime(t time.Time) *Instant {
	return &Instant{Nanoseconds: t.UnixNano()}
}

// Now returns the current Instant.
func InstantNow() *Instant {
	return &Instant{Nanoseconds: time.Now().UnixNano()}
}

// ParseInstant parses an ISO 8601 string to Instant.
func ParseInstant(isoString string) (*Instant, error) {
	// Parse using time.Parse with RFC3339Nano format
	t, err := time.Parse(time.RFC3339Nano, isoString)
	if err != nil {
		// Try with regular RFC3339 format
		t, err = time.Parse(time.RFC3339, isoString)
		if err != nil {
			return nil, fmt.Errorf("invalid ISO date string: %s", isoString)
		}
	}
	
	// Convert to UTC
	utc := t.UTC()
	return &Instant{Nanoseconds: utc.UnixNano()}, nil
}

// ToISO8601 converts Instant to ISO 8601 string with nanosecond precision.
func (i *Instant) ToISO8601() string {
	t := time.Unix(0, i.Nanoseconds).UTC()
	return t.Format(time.RFC3339Nano)
}

// ToTime converts Instant to time.Time.
func (i *Instant) ToTime() time.Time {
	return time.Unix(0, i.Nanoseconds).UTC()
}

// EpochNanos returns nanoseconds since epoch.
func (i *Instant) EpochNanos() int64 {
	return i.Nanoseconds
}

// EpochMillis returns milliseconds since epoch.
func (i *Instant) EpochMillis() int64 {
	return i.Nanoseconds / 1_000_000
}

// EpochSeconds returns seconds since epoch.
func (i *Instant) EpochSeconds() int64 {
	return i.Nanoseconds / 1_000_000_000
}

// String returns the ISO 8601 string representation.
func (i *Instant) String() string {
	return i.ToISO8601()
}

// Duration represents a time span with nanosecond precision.
type Duration struct {
	Nanoseconds int64 // Duration in nanoseconds
}

// NewDuration creates a Duration from nanoseconds.
func NewDuration(nanoseconds int64) *Duration {
	return &Duration{Nanoseconds: nanoseconds}
}

// NewDurationFromMillis creates a Duration from milliseconds.
func NewDurationFromMillis(milliseconds int64) *Duration {
	return &Duration{Nanoseconds: milliseconds * 1_000_000}
}

// NewDurationFromSeconds creates a Duration from seconds.
func NewDurationFromSeconds(seconds int64) *Duration {
	return &Duration{Nanoseconds: seconds * 1_000_000_000}
}

// NewDurationFromMinutes creates a Duration from minutes.
func NewDurationFromMinutes(minutes int64) *Duration {
	return &Duration{Nanoseconds: minutes * 60 * 1_000_000_000}
}

// NewDurationFromHours creates a Duration from hours.
func NewDurationFromHours(hours int64) *Duration {
	return &Duration{Nanoseconds: hours * 3600 * 1_000_000_000}
}

// NewDurationFromDays creates a Duration from days.
func NewDurationFromDays(days int64) *Duration {
	return &Duration{Nanoseconds: days * 86400 * 1_000_000_000}
}

// NewDurationFromGoDuration creates a Duration from Go's time.Duration.
func NewDurationFromGoDuration(d time.Duration) *Duration {
	return &Duration{Nanoseconds: int64(d)}
}

// ParseDuration parses an ISO 8601 duration string.
func ParseDuration(durationString string) (*Duration, error) {
	// Match ISO 8601 duration format: P[nD]T[nH][nM][nS]
	re := regexp.MustCompile(`^P(?:(\d+)D)?(?:T(?:(\d+)H)?(?:(\d+)M)?(?:(\d+(?:\.\d+)?)S)?)?$`)
	matches := re.FindStringSubmatch(durationString)
	if matches == nil {
		return nil, fmt.Errorf("invalid ISO duration format: %s", durationString)
	}
	
	var totalNanos int64
	
	// Days
	if matches[1] != "" {
		days, err := strconv.ParseInt(matches[1], 10, 64)
		if err != nil {
			return nil, fmt.Errorf("invalid days in duration: %s", durationString)
		}
		totalNanos += days * 86400 * 1_000_000_000
	}
	
	// Hours
	if matches[2] != "" {
		hours, err := strconv.ParseInt(matches[2], 10, 64)
		if err != nil {
			return nil, fmt.Errorf("invalid hours in duration: %s", durationString)
		}
		totalNanos += hours * 3600 * 1_000_000_000
	}
	
	// Minutes
	if matches[3] != "" {
		minutes, err := strconv.ParseInt(matches[3], 10, 64)
		if err != nil {
			return nil, fmt.Errorf("invalid minutes in duration: %s", durationString)
		}
		totalNanos += minutes * 60 * 1_000_000_000
	}
	
	// Seconds
	if matches[4] != "" {
		seconds, err := strconv.ParseFloat(matches[4], 64)
		if err != nil {
			return nil, fmt.Errorf("invalid seconds in duration: %s", durationString)
		}
		totalNanos += int64(seconds * 1_000_000_000)
	}
	
	return &Duration{Nanoseconds: totalNanos}, nil
}

// ToISO8601 converts Duration to ISO 8601 duration string.
func (d *Duration) ToISO8601() string {
	if d.Nanoseconds == 0 {
		return "PT0S"
	}
	
	remaining := d.Nanoseconds
	negative := remaining < 0
	if negative {
		remaining = -remaining
	}
	
	var result strings.Builder
	result.WriteString("P")
	
	// Days
	days := remaining / (86400 * 1_000_000_000)
	if days > 0 {
		result.WriteString(fmt.Sprintf("%dD", days))
		remaining %= 86400 * 1_000_000_000
	}
	
	if remaining > 0 {
		result.WriteString("T")
		
		// Hours
		hours := remaining / (3600 * 1_000_000_000)
		if hours > 0 {
			result.WriteString(fmt.Sprintf("%dH", hours))
			remaining %= 3600 * 1_000_000_000
		}
		
		// Minutes
		minutes := remaining / (60 * 1_000_000_000)
		if minutes > 0 {
			result.WriteString(fmt.Sprintf("%dM", minutes))
			remaining %= 60 * 1_000_000_000
		}
		
		// Seconds (with fractional part)
		if remaining > 0 {
			seconds := remaining / 1_000_000_000
			nanosPart := remaining % 1_000_000_000
			
			if nanosPart == 0 {
				result.WriteString(fmt.Sprintf("%dS", seconds))
			} else {
				// Format nanoseconds (remove trailing zeros)
				fractionalStr := strings.TrimRight(fmt.Sprintf("%09d", nanosPart), "0")
				result.WriteString(fmt.Sprintf("%d.%sS", seconds, fractionalStr))
			}
		}
	}
	
	if negative {
		return "-" + result.String()
	}
	return result.String()
}

// ToGoDuration converts to Go's time.Duration.
func (d *Duration) ToGoDuration() time.Duration {
	return time.Duration(d.Nanoseconds)
}

// TotalNanos returns total nanoseconds.
func (d *Duration) TotalNanos() int64 {
	return d.Nanoseconds
}

// TotalMillis returns total milliseconds.
func (d *Duration) TotalMillis() float64 {
	return float64(d.Nanoseconds) / 1_000_000.0
}

// TotalSeconds returns total seconds.
func (d *Duration) TotalSeconds() float64 {
	return float64(d.Nanoseconds) / 1_000_000_000.0
}

// TotalMinutes returns total minutes.
func (d *Duration) TotalMinutes() float64 {
	return float64(d.Nanoseconds) / (60.0 * 1_000_000_000.0)
}

// TotalHours returns total hours.
func (d *Duration) TotalHours() float64 {
	return float64(d.Nanoseconds) / (3600.0 * 1_000_000_000.0)
}

// TotalDays returns total days.
func (d *Duration) TotalDays() float64 {
	return float64(d.Nanoseconds) / (86400.0 * 1_000_000_000.0)
}

// Add adds two durations.
func (d *Duration) Add(other *Duration) *Duration {
	return &Duration{Nanoseconds: d.Nanoseconds + other.Nanoseconds}
}

// Sub subtracts two durations.
func (d *Duration) Sub(other *Duration) *Duration {
	return &Duration{Nanoseconds: d.Nanoseconds - other.Nanoseconds}
}

// Mul multiplies duration by scalar.
func (d *Duration) Mul(scalar float64) *Duration {
	return &Duration{Nanoseconds: int64(float64(d.Nanoseconds) * scalar)}
}

// Div divides duration by scalar.
func (d *Duration) Div(scalar float64) *Duration {
	return &Duration{Nanoseconds: int64(float64(d.Nanoseconds) / scalar)}
}

// Neg negates duration.
func (d *Duration) Neg() *Duration {
	return &Duration{Nanoseconds: -d.Nanoseconds}
}

// Abs returns absolute value of duration.
func (d *Duration) Abs() *Duration {
	if d.Nanoseconds < 0 {
		return &Duration{Nanoseconds: -d.Nanoseconds}
	}
	return &Duration{Nanoseconds: d.Nanoseconds}
}

// IsZero checks if duration is zero.
func (d *Duration) IsZero() bool {
	return d.Nanoseconds == 0
}

// IsNegative checks if duration is negative.
func (d *Duration) IsNegative() bool {
	return d.Nanoseconds < 0
}

// String returns the ISO 8601 string representation.
func (d *Duration) String() string {
	return d.ToISO8601()
}

// Date represents a date with timezone for kJSON (DEPRECATED: use Instant instead).
type Date struct {
	Time     time.Time
	TzOffset int16 // Timezone offset in minutes
}

// NewDate creates a Date from a time.Time.
func NewDate(t time.Time) *Date {
	_, offset := t.Zone()
	return &Date{
		Time:     t,
		TzOffset: int16(offset / 60), // Convert seconds to minutes
	}
}

// String returns the ISO 8601 string representation of the Date.
func (d *Date) String() string {
	return d.Time.Format(time.RFC3339)
}

// Value represents a kJSON value of any type.
type Value struct {
	Type     ValueType
	Null     interface{}
	Bool     bool
	Number   float64
	String   string
	BigInt   *BigInt
	Decimal  *Decimal128
	UUID     uuid.UUID
	Instant  *Instant
	Duration *Duration
	Date     *Date
	Array    []*Value
	Object   map[string]*Value
}

// ValueType represents the type of a kJSON value.
type ValueType int

const (
	TypeNull ValueType = iota
	TypeBool
	TypeNumber
	TypeString
	TypeBigInt
	TypeDecimal128
	TypeUUID
	TypeInstant
	TypeDuration
	TypeDate
	TypeArray
	TypeObject
)

// String returns the string representation of the ValueType.
func (t ValueType) String() string {
	switch t {
	case TypeNull:
		return "null"
	case TypeBool:
		return "bool"
	case TypeNumber:
		return "number"
	case TypeString:
		return "string"
	case TypeBigInt:
		return "bigint"
	case TypeDecimal128:
		return "decimal128"
	case TypeUUID:
		return "uuid"
	case TypeInstant:
		return "instant"
	case TypeDuration:
		return "duration"
	case TypeDate:
		return "date"
	case TypeArray:
		return "array"
	case TypeObject:
		return "object"
	default:
		return "unknown"
	}
}

// getStructTag returns the field name to use for a struct field.
// It checks for "kjson" tag first, then "json" tag, then uses the field name.
func getStructTag(field reflect.StructField) (string, bool) {
	// Check kjson tag first
	if tag := field.Tag.Get("kjson"); tag != "" {
		if tag == "-" {
			return "", false // Skip this field
		}
		// Parse tag (handle ",omitempty" etc.)
		if comma := findComma(tag); comma != -1 {
			return tag[:comma], true
		}
		return tag, true
	}
	
	// Fall back to json tag
	if tag := field.Tag.Get("json"); tag != "" {
		if tag == "-" {
			return "", false // Skip this field
		}
		// Parse tag (handle ",omitempty" etc.)
		if comma := findComma(tag); comma != -1 {
			return tag[:comma], true
		}
		return tag, true
	}
	
	// Use field name if no tags
	return field.Name, true
}

// findComma finds the first comma in a string.
func findComma(s string) int {
	for i, c := range s {
		if c == ',' {
			return i
		}
	}
	return -1
}

// isOmitEmpty checks if a struct tag contains "omitempty".
func isOmitEmpty(field reflect.StructField) bool {
	// Check kjson tag first
	if tag := field.Tag.Get("kjson"); tag != "" {
		return containsOmitEmpty(tag)
	}
	
	// Fall back to json tag
	if tag := field.Tag.Get("json"); tag != "" {
		return containsOmitEmpty(tag)
	}
	
	return false
}

// containsOmitEmpty checks if a tag contains "omitempty".
func containsOmitEmpty(tag string) bool {
	if comma := findComma(tag); comma != -1 {
		options := tag[comma+1:]
		// Simple check for omitempty
		return options == "omitempty" || 
			   (len(options) > 9 && options[:10] == "omitempty,") ||
			   (len(options) > 9 && options[len(options)-10:] == ",omitempty")
	}
	return false
}