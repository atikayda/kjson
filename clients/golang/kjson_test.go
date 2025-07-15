package kjson

import (
	"testing"
	"time"

	"github.com/google/uuid"
)

// Test basic types
func TestBasicTypes(t *testing.T) {
	tests := []struct {
		name     string
		input    interface{}
		expected string
	}{
		{"null", nil, "null"},
		{"bool true", true, "true"},
		{"bool false", false, "false"},
		{"number int", 42, "42"},
		{"number float", 3.14, "3.14"},
		{"string", "hello", "'hello'"},
		{"empty array", []interface{}{}, "[]"},
		{"empty object", map[string]interface{}{}, "{}"},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			result, err := Marshal(tt.input)
			if err != nil {
				t.Fatalf("Marshal failed: %v", err)
			}
			if string(result) != tt.expected {
				t.Errorf("Marshal() = %s, want %s", string(result), tt.expected)
			}
		})
	}
}

// Test BigInt
func TestBigInt(t *testing.T) {
	bigint := NewBigInt(123456789012345678)
	
	result, err := Marshal(bigint)
	if err != nil {
		t.Fatalf("Marshal BigInt failed: %v", err)
	}
	
	expected := "123456789012345678n"
	if string(result) != expected {
		t.Errorf("Marshal BigInt = %s, want %s", string(result), expected)
	}
	
	// Test parsing
	var parsed BigInt
	err = Unmarshal(result, &parsed)
	if err != nil {
		t.Fatalf("Unmarshal BigInt failed: %v", err)
	}
	
	if parsed.String() != "123456789012345678" {
		t.Errorf("Parsed BigInt = %s, want 123456789012345678", parsed.String())
	}
}

// Test Decimal128
func TestDecimal128(t *testing.T) {
	decimal, err := NewDecimal128("99.99")
	if err != nil {
		t.Fatalf("NewDecimal128 failed: %v", err)
	}
	
	result, err := Marshal(decimal)
	if err != nil {
		t.Fatalf("Marshal Decimal128 failed: %v", err)
	}
	
	expected := "99.99m"
	if string(result) != expected {
		t.Errorf("Marshal Decimal128 = %s, want %s", string(result), expected)
	}
	
	// Test parsing
	var parsed Decimal128
	err = Unmarshal(result, &parsed)
	if err != nil {
		t.Fatalf("Unmarshal Decimal128 failed: %v", err)
	}
	
	if parsed.String() != "99.99" {
		t.Errorf("Parsed Decimal128 = %s, want 99.99", parsed.String())
	}
}

// Test UUID
func TestUUID(t *testing.T) {
	// Use a predictable UUID that doesn't start with 't' or 'f'
	u, _ := uuid.Parse("550e8400-e29b-41d4-a716-446655440000")
	
	result, err := Marshal(u)
	if err != nil {
		t.Fatalf("Marshal UUID failed: %v", err)
	}
	
	expected := u.String()
	if string(result) != expected {
		t.Errorf("Marshal UUID = %s, want %s", string(result), expected)
	}
	
	// Test parsing
	var parsed uuid.UUID
	err = Unmarshal(result, &parsed)
	if err != nil {
		t.Fatalf("Unmarshal UUID failed: %v", err)
	}
	
	if parsed != u {
		t.Errorf("Parsed UUID = %s, want %s", parsed.String(), u.String())
	}
}

// Test Date
func TestDate(t *testing.T) {
	now := time.Now().UTC()
	date := NewDate(now)
	
	result, err := Marshal(date)
	if err != nil {
		t.Fatalf("Marshal Date failed: %v", err)
	}
	
	expected := now.Format(time.RFC3339)
	if string(result) != expected {
		t.Errorf("Marshal Date = %s, want %s", string(result), expected)
	}
	
	// Test parsing
	var parsed Date
	err = Unmarshal(result, &parsed)
	if err != nil {
		t.Fatalf("Unmarshal Date failed: %v", err)
	}
	
	// Compare times (allowing for small differences due to formatting)
	if parsed.Time.Unix() != now.Unix() {
		t.Errorf("Parsed Date = %v, want %v", parsed.Time, now)
	}
}

// Test struct with kjson tags
func TestStructWithKJsonTags(t *testing.T) {
	type TestStruct struct {
		ID          int     `kjson:"id"`
		Name        string  `kjson:"name"`
		Value       float64 `json:"value"`  // Should use json tag as fallback
		Internal    string  `kjson:"-"`     // Should be skipped
		NoTag       string                  // Should use field name
	}
	
	s := TestStruct{
		ID:       123,
		Name:     "test",
		Value:    99.99,
		Internal: "hidden",
		NoTag:    "visible",
	}
	
	result, err := Marshal(s)
	if err != nil {
		t.Fatalf("Marshal struct failed: %v", err)
	}
	
	// Parse back to verify structure
	var parsed TestStruct
	err = Unmarshal(result, &parsed)
	if err != nil {
		t.Fatalf("Unmarshal struct failed: %v", err)
	}
	
	if parsed.ID != s.ID {
		t.Errorf("ID = %d, want %d", parsed.ID, s.ID)
	}
	if parsed.Name != s.Name {
		t.Errorf("Name = %s, want %s", parsed.Name, s.Name)
	}
	if parsed.Value != s.Value {
		t.Errorf("Value = %f, want %f", parsed.Value, s.Value)
	}
	if parsed.Internal != "" {
		t.Errorf("Internal should be empty, got %s", parsed.Internal)
	}
	if parsed.NoTag != s.NoTag {
		t.Errorf("NoTag = %s, want %s", parsed.NoTag, s.NoTag)
	}
}

// Test complex object with extended types
func TestComplexObject(t *testing.T) {
	type ComplexObject struct {
		ID        uuid.UUID    `kjson:"id"`
		BigNum    *BigInt      `kjson:"bigNum"`
		Price     *Decimal128  `kjson:"price"`
		Created   *Date        `kjson:"created"`
		Active    bool         `kjson:"active"`
		Tags      []string     `kjson:"tags"`
	}
	
	obj := ComplexObject{
		ID:      UUIDv4(),
		BigNum:  NewBigInt(123456789012345678),
		Price:   mustNewDecimal128("99.99"),
		Created: NewDate(time.Now().UTC()),
		Active:  true,
		Tags:    []string{"new", "sale"},
	}
	
	result, err := Marshal(obj)
	if err != nil {
		t.Fatalf("Marshal complex object failed: %v", err)
	}
	
	// Verify we can parse it back
	var parsed ComplexObject
	err = Unmarshal(result, &parsed)
	if err != nil {
		t.Fatalf("Unmarshal complex object failed: %v", err)
	}
	
	if parsed.ID != obj.ID {
		t.Errorf("ID mismatch")
	}
	if parsed.BigNum.String() != obj.BigNum.String() {
		t.Errorf("BigNum mismatch")
	}
	if parsed.Price.String() != obj.Price.String() {
		t.Errorf("Price mismatch")
	}
	if !parsed.Active {
		t.Errorf("Active should be true")
	}
	if len(parsed.Tags) != 2 || parsed.Tags[0] != "new" || parsed.Tags[1] != "sale" {
		t.Errorf("Tags mismatch: %v", parsed.Tags)
	}
}

// Test UUIDv4 and UUIDv7 generation
func TestUUIDGeneration(t *testing.T) {
	// Test UUIDv4
	u4 := UUIDv4()
	if UUIDVersion(u4) != 4 {
		t.Errorf("UUIDv4 version = %d, want 4", UUIDVersion(u4))
	}
	
	// Test UUIDv7
	u7 := UUIDv7()
	if UUIDVersion(u7) != 7 {
		t.Errorf("UUIDv7 version = %d, want 7", UUIDVersion(u7))
	}
	
	// Test they're different
	if u4 == u7 {
		t.Error("UUIDv4 and UUIDv7 should be different")
	}
	
	// Test multiple generations are different
	u4_2 := UUIDv4()
	if u4 == u4_2 {
		t.Error("Multiple UUIDv4 generations should be different")
	}
}

// Test parsing of kJSON string with extended types
func TestParseExtendedTypes(t *testing.T) {
	input := `{
		"id": 550e8400-e29b-41d4-a716-446655440000,
		"bigNum": 123456789012345678901234567890n,
		"price": 99.99m,
		"created": 2025-01-10T12:00:00Z,
		"active": true,
		"tags": ["new", "sale"]
	}`
	
	var result map[string]interface{}
	err := Unmarshal([]byte(input), &result)
	if err != nil {
		t.Fatalf("Parse extended types failed: %v", err)
	}
	
	// Verify types
	if _, ok := result["id"].(uuid.UUID); !ok {
		t.Error("id should be UUID")
	}
	if _, ok := result["bigNum"].(*BigInt); !ok {
		t.Error("bigNum should be BigInt")
	}
	if _, ok := result["price"].(*Decimal128); !ok {
		t.Error("price should be Decimal128")
	}
	if _, ok := result["created"].(*Date); !ok {
		t.Error("created should be Date")
	}
	if result["active"] != true {
		t.Error("active should be true")
	}
}

func TestBacktickQuotes(t *testing.T) {
	// Test parsing backtick strings
	jsonStr := "{backtick: `template string`, mixed: `He said \"Hello\" and 'Hi'`}"
	var result map[string]interface{}
	err := Unmarshal([]byte(jsonStr), &result)
	if err != nil {
		t.Fatal(err)
	}
	
	if result["backtick"] != "template string" {
		t.Error("backtick string not parsed correctly")
	}
	if result["mixed"] != `He said "Hello" and 'Hi'` {
		t.Error("mixed quotes in backticks not parsed correctly")
	}
}

func TestSmartQuoteSelection(t *testing.T) {
	tests := []struct {
		input    string
		expected string
	}{
		{"hello", "'hello'"},                               // no quotes - use single
		{"it's nice", `"it's nice"`},                       // has single - use double
		{`He said "hi"`, `'He said "hi"'`},                 // has double - use single
		{`Mix 'both' "types"`, "`Mix 'both' \"types\"`"}, // has both - use backtick
	}
	
	for _, test := range tests {
		data := map[string]interface{}{"text": test.input}
		result, err := Marshal(data)
		if err != nil {
			t.Fatal(err)
		}
		
		expected := "{text:" + test.expected + "}"
		if string(result) != expected {
			t.Errorf("Smart quote failed for %q: got %s, want %s", 
				test.input, string(result), expected)
		}
	}
}

func TestMultilineStrings(t *testing.T) {
	// Test multiline strings with different quote types
	multiline := "Line 1\nLine 2\nLine 3"
	
	// Test parsing
	jsonStr := "`Line 1\nLine 2\nLine 3`"
	var result string
	err := Unmarshal([]byte(jsonStr), &result)
	if err != nil {
		t.Fatal(err)
	}
	
	if result != multiline {
		t.Error("multiline string not parsed correctly")
	}
}

// Helper function for tests
func mustNewDecimal128(s string) *Decimal128 {
	d, err := NewDecimal128(s)
	if err != nil {
		panic(err)
	}
	return d
}