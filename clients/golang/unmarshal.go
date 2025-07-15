package kjson

import (
	"fmt"
	"reflect"
	"strconv"
	"time"

	"github.com/google/uuid"
)

// fromKJsonValue converts a kJSON Value to a Go value.
func fromKJsonValue(value *Value, v interface{}) error {
	rv := reflect.ValueOf(v)
	if rv.Kind() != reflect.Ptr || rv.IsNil() {
		return fmt.Errorf("kjson: Unmarshal target must be a non-nil pointer")
	}
	
	return fromKJsonValueReflect(value, rv.Elem())
}

// fromKJsonValueReflect converts a kJSON Value to a reflect.Value.
func fromKJsonValueReflect(value *Value, rv reflect.Value) error {
	// Handle null values
	if value.Type == TypeNull {
		if rv.Kind() == reflect.Ptr || rv.Kind() == reflect.Interface || 
		   rv.Kind() == reflect.Map || rv.Kind() == reflect.Slice {
			rv.Set(reflect.Zero(rv.Type()))
			return nil
		}
		// For non-pointer types, null becomes zero value
		rv.Set(reflect.Zero(rv.Type()))
		return nil
	}
	
	// Handle pointers
	if rv.Kind() == reflect.Ptr {
		if rv.IsNil() {
			rv.Set(reflect.New(rv.Type().Elem()))
		}
		return fromKJsonValueReflect(value, rv.Elem())
	}
	
	// Handle interfaces
	if rv.Kind() == reflect.Interface {
		// Set the interface to the appropriate Go type
		goValue, err := valueToGoInterface(value)
		if err != nil {
			return err
		}
		rv.Set(reflect.ValueOf(goValue))
		return nil
	}
	
	switch value.Type {
	case TypeBool:
		return setBool(rv, value.Bool)
		
	case TypeNumber:
		return setNumber(rv, value.Number)
		
	case TypeString:
		return setString(rv, value.String)
		
	case TypeBigInt:
		return setBigInt(rv, value.BigInt)
		
	case TypeDecimal128:
		return setDecimal128(rv, value.Decimal)
		
	case TypeUUID:
		return setUUID(rv, value.UUID)
		
	case TypeDate:
		return setDate(rv, value.Date)
		
	case TypeArray:
		return setArray(rv, value.Array)
		
	case TypeObject:
		return setObject(rv, value.Object)
		
	default:
		return fmt.Errorf("unknown kJSON type: %v", value.Type)
	}
}

// valueToGoInterface converts a kJSON Value to a Go interface{}.
func valueToGoInterface(value *Value) (interface{}, error) {
	switch value.Type {
	case TypeNull:
		return nil, nil
	case TypeBool:
		return value.Bool, nil
	case TypeNumber:
		return value.Number, nil
	case TypeString:
		return value.String, nil
	case TypeBigInt:
		return value.BigInt, nil
	case TypeDecimal128:
		return value.Decimal, nil
	case TypeUUID:
		return value.UUID, nil
	case TypeDate:
		return value.Date, nil
	case TypeArray:
		arr := make([]interface{}, len(value.Array))
		for i, item := range value.Array {
			val, err := valueToGoInterface(item)
			if err != nil {
				return nil, err
			}
			arr[i] = val
		}
		return arr, nil
	case TypeObject:
		obj := make(map[string]interface{})
		for key, val := range value.Object {
			goVal, err := valueToGoInterface(val)
			if err != nil {
				return nil, err
			}
			obj[key] = goVal
		}
		return obj, nil
	default:
		return nil, fmt.Errorf("unknown kJSON type: %v", value.Type)
	}
}

// setBool sets a boolean value.
func setBool(rv reflect.Value, b bool) error {
	switch rv.Kind() {
	case reflect.Bool:
		rv.SetBool(b)
		return nil
	default:
		return fmt.Errorf("cannot unmarshal bool into %v", rv.Type())
	}
}

// setNumber sets a numeric value.
func setNumber(rv reflect.Value, n float64) error {
	switch rv.Kind() {
	case reflect.Int, reflect.Int8, reflect.Int16, reflect.Int32, reflect.Int64:
		rv.SetInt(int64(n))
		return nil
	case reflect.Uint, reflect.Uint8, reflect.Uint16, reflect.Uint32, reflect.Uint64:
		rv.SetUint(uint64(n))
		return nil
	case reflect.Float32, reflect.Float64:
		rv.SetFloat(n)
		return nil
	default:
		return fmt.Errorf("cannot unmarshal number into %v", rv.Type())
	}
}

// setString sets a string value.
func setString(rv reflect.Value, s string) error {
	switch rv.Kind() {
	case reflect.String:
		rv.SetString(s)
		return nil
	default:
		return fmt.Errorf("cannot unmarshal string into %v", rv.Type())
	}
}

// setBigInt sets a BigInt value.
func setBigInt(rv reflect.Value, b *BigInt) error {
	if rv.Type() == reflect.TypeOf(BigInt{}) {
		rv.Set(reflect.ValueOf(*b))
		return nil
	}
	
	if rv.Type() == reflect.TypeOf(&BigInt{}) {
		rv.Set(reflect.ValueOf(b))
		return nil
	}
	
	// Try to convert to numeric types if possible
	if rv.Kind() >= reflect.Int && rv.Kind() <= reflect.Uint64 {
		// Parse the BigInt as int64
		val, err := strconv.ParseInt(b.String(), 10, 64)
		if err != nil {
			return fmt.Errorf("BigInt too large for %v: %s", rv.Type(), b.String())
		}
		
		switch rv.Kind() {
		case reflect.Int, reflect.Int8, reflect.Int16, reflect.Int32, reflect.Int64:
			rv.SetInt(val)
		case reflect.Uint, reflect.Uint8, reflect.Uint16, reflect.Uint32, reflect.Uint64:
			if val < 0 {
				return fmt.Errorf("negative BigInt cannot be assigned to unsigned type %v", rv.Type())
			}
			rv.SetUint(uint64(val))
		}
		return nil
	}
	
	return fmt.Errorf("cannot unmarshal BigInt into %v", rv.Type())
}

// setDecimal128 sets a Decimal128 value.
func setDecimal128(rv reflect.Value, d *Decimal128) error {
	if rv.Type() == reflect.TypeOf(Decimal128{}) {
		rv.Set(reflect.ValueOf(*d))
		return nil
	}
	
	if rv.Type() == reflect.TypeOf(&Decimal128{}) {
		rv.Set(reflect.ValueOf(d))
		return nil
	}
	
	// Try to convert to numeric types
	if rv.Kind() >= reflect.Float32 && rv.Kind() <= reflect.Float64 {
		val, err := strconv.ParseFloat(d.String(), 64)
		if err != nil {
			return fmt.Errorf("cannot convert Decimal128 to float: %s", d.String())
		}
		rv.SetFloat(val)
		return nil
	}
	
	return fmt.Errorf("cannot unmarshal Decimal128 into %v", rv.Type())
}

// setUUID sets a UUID value.
func setUUID(rv reflect.Value, u uuid.UUID) error {
	if rv.Type() == reflect.TypeOf(uuid.UUID{}) {
		rv.Set(reflect.ValueOf(u))
		return nil
	}
	
	if rv.Kind() == reflect.String {
		rv.SetString(u.String())
		return nil
	}
	
	return fmt.Errorf("cannot unmarshal UUID into %v", rv.Type())
}

// setDate sets a Date value.
func setDate(rv reflect.Value, d *Date) error {
	if rv.Type() == reflect.TypeOf(Date{}) {
		rv.Set(reflect.ValueOf(*d))
		return nil
	}
	
	if rv.Type() == reflect.TypeOf(&Date{}) {
		rv.Set(reflect.ValueOf(d))
		return nil
	}
	
	if rv.Type() == reflect.TypeOf(time.Time{}) {
		rv.Set(reflect.ValueOf(d.Time))
		return nil
	}
	
	if rv.Kind() == reflect.String {
		rv.SetString(d.String())
		return nil
	}
	
	return fmt.Errorf("cannot unmarshal Date into %v", rv.Type())
}

// setArray sets an array value.
func setArray(rv reflect.Value, arr []*Value) error {
	switch rv.Kind() {
	case reflect.Slice:
		// Create new slice
		slice := reflect.MakeSlice(rv.Type(), len(arr), len(arr))
		for i, item := range arr {
			if err := fromKJsonValueReflect(item, slice.Index(i)); err != nil {
				return err
			}
		}
		rv.Set(slice)
		return nil
		
	case reflect.Array:
		// Check length
		if len(arr) != rv.Len() {
			return fmt.Errorf("array length mismatch: got %d, expected %d", len(arr), rv.Len())
		}
		for i, item := range arr {
			if err := fromKJsonValueReflect(item, rv.Index(i)); err != nil {
				return err
			}
		}
		return nil
		
	default:
		return fmt.Errorf("cannot unmarshal array into %v", rv.Type())
	}
}

// setObject sets an object value.
func setObject(rv reflect.Value, obj map[string]*Value) error {
	switch rv.Kind() {
	case reflect.Map:
		// Create new map
		if rv.Type().Key().Kind() != reflect.String {
			return fmt.Errorf("map key must be string, got %v", rv.Type().Key())
		}
		
		mapValue := reflect.MakeMap(rv.Type())
		for key, value := range obj {
			valueReflect := reflect.New(rv.Type().Elem()).Elem()
			if err := fromKJsonValueReflect(value, valueReflect); err != nil {
				return err
			}
			mapValue.SetMapIndex(reflect.ValueOf(key), valueReflect)
		}
		rv.Set(mapValue)
		return nil
		
	case reflect.Struct:
		return setStruct(rv, obj)
		
	default:
		return fmt.Errorf("cannot unmarshal object into %v", rv.Type())
	}
}

// setStruct sets a struct value from an object.
func setStruct(rv reflect.Value, obj map[string]*Value) error {
	rt := rv.Type()
	
	// Create a map of field names to field indices
	fieldMap := make(map[string]int)
	for i := 0; i < rt.NumField(); i++ {
		field := rt.Field(i)
		
		// Skip unexported fields
		if !rv.Field(i).CanSet() {
			continue
		}
		
		fieldName, shouldInclude := getStructTag(field)
		if shouldInclude {
			fieldMap[fieldName] = i
		}
	}
	
	// Set field values
	for key, value := range obj {
		if fieldIndex, exists := fieldMap[key]; exists {
			fieldValue := rv.Field(fieldIndex)
			if err := fromKJsonValueReflect(value, fieldValue); err != nil {
				return fmt.Errorf("field %s: %v", key, err)
			}
		}
		// Ignore unknown fields (like encoding/json)
	}
	
	return nil
}