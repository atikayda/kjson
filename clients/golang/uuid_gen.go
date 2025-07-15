package kjson

import (
	"crypto/rand"
	"fmt"
	"time"

	"github.com/google/uuid"
)

// UUIDv4 generates a UUIDv4 (random) that matches the TypeScript implementation.
func UUIDv4() uuid.UUID {
	bytes := make([]byte, 16)
	_, err := rand.Read(bytes)
	if err != nil {
		panic(fmt.Sprintf("failed to generate random bytes for UUIDv4: %v", err))
	}
	
	// Set version (4) and variant bits
	bytes[6] = (bytes[6] & 0x0f) | 0x40 // Version 4
	bytes[8] = (bytes[8] & 0x3f) | 0x80 // Variant 10
	
	return bytesToUUID(bytes)
}

// UUIDv7 generates a UUIDv7 (timestamp-based) that matches the TypeScript implementation.
func UUIDv7() uuid.UUID {
	bytes := make([]byte, 16)
	now := time.Now().UnixMilli()
	
	// Timestamp (48 bits) - split into 6 bytes
	bytes[0] = byte((now >> 40) & 0xff)
	bytes[1] = byte((now >> 32) & 0xff)
	bytes[2] = byte((now >> 24) & 0xff)
	bytes[3] = byte((now >> 16) & 0xff)
	bytes[4] = byte((now >> 8) & 0xff)
	bytes[5] = byte(now & 0xff)
	
	// Random data for the rest (10 bytes)
	random := make([]byte, 10)
	_, err := rand.Read(random)
	if err != nil {
		panic(fmt.Sprintf("failed to generate random bytes for UUIDv7: %v", err))
	}
	
	copy(bytes[6:], random)
	
	// Set version (7) and variant bits
	bytes[6] = (bytes[6] & 0x0f) | 0x70 // Version 7
	bytes[8] = (bytes[8] & 0x3f) | 0x80 // Variant 10
	
	return bytesToUUID(bytes)
}

// bytesToUUID converts 16 bytes to a UUID.
func bytesToUUID(bytes []byte) uuid.UUID {
	if len(bytes) != 16 {
		panic("UUID must be exactly 16 bytes")
	}
	
	var u uuid.UUID
	copy(u[:], bytes)
	return u
}

// UUIDFromBytes creates a UUID from bytes (matches TypeScript fromBytes).
func UUIDFromBytes(bytes []byte) (uuid.UUID, error) {
	if len(bytes) != 16 {
		return uuid.Nil, fmt.Errorf("UUID must be exactly 16 bytes, got %d", len(bytes))
	}
	
	var u uuid.UUID
	copy(u[:], bytes)
	return u, nil
}

// IsValidUUID checks if a string is a valid UUID (matches TypeScript isValid).
func IsValidUUID(s string) bool {
	_, err := uuid.Parse(s)
	return err == nil
}

// UUIDVersion returns the version of a UUID (matches TypeScript version getter).
func UUIDVersion(u uuid.UUID) int {
	return int(u.Version())
}

// UUIDBytes returns the bytes of a UUID (matches TypeScript bytes method).
func UUIDBytes(u uuid.UUID) []byte {
	bytes := make([]byte, 16)
	copy(bytes, u[:])
	return bytes
}