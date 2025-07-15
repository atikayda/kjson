use crate::error::{Error, Result};
use crate::types::{BigInt, Date, Decimal128};
use serde::{Deserialize, Serialize};
use std::collections::HashMap;
use uuid::Uuid;

/// kJSON Value enum representing all possible kJSON types
#[derive(Debug, Clone, PartialEq)]
pub enum Value {
    /// Null value
    Null,
    /// Boolean value
    Bool(bool),
    /// Number value (f64)
    Number(f64),
    /// String value
    String(String),
    /// Array of values
    Array(Vec<Value>),
    /// Object (key-value pairs)
    Object(HashMap<String, Value>),
    /// BigInt value
    BigInt(BigInt),
    /// Decimal128 value
    Decimal128(Decimal128),
    /// UUID value
    Uuid(Uuid),
    /// Date value
    Date(Date),
}

impl Value {
    /// Check if value is null
    pub fn is_null(&self) -> bool {
        matches!(self, Value::Null)
    }

    /// Try to get as bool
    pub fn as_bool(&self) -> Option<bool> {
        match self {
            Value::Bool(b) => Some(*b),
            _ => None,
        }
    }

    /// Try to get as number
    pub fn as_f64(&self) -> Option<f64> {
        match self {
            Value::Number(n) => Some(*n),
            _ => None,
        }
    }

    /// Try to get as string
    pub fn as_str(&self) -> Option<&str> {
        match self {
            Value::String(s) => Some(s),
            _ => None,
        }
    }

    /// Try to get as array
    pub fn as_array(&self) -> Option<&Vec<Value>> {
        match self {
            Value::Array(a) => Some(a),
            _ => None,
        }
    }

    /// Try to get as object
    pub fn as_object(&self) -> Option<&HashMap<String, Value>> {
        match self {
            Value::Object(o) => Some(o),
            _ => None,
        }
    }

    /// Try to get as BigInt
    pub fn as_bigint(&self) -> Option<&BigInt> {
        match self {
            Value::BigInt(b) => Some(b),
            _ => None,
        }
    }

    /// Try to get as Decimal128
    pub fn as_decimal128(&self) -> Option<&Decimal128> {
        match self {
            Value::Decimal128(d) => Some(d),
            _ => None,
        }
    }

    /// Try to get as UUID
    pub fn as_uuid(&self) -> Option<&Uuid> {
        match self {
            Value::Uuid(u) => Some(u),
            _ => None,
        }
    }

    /// Try to get as Date
    pub fn as_date(&self) -> Option<&Date> {
        match self {
            Value::Date(d) => Some(d),
            _ => None,
        }
    }

    /// Get the type name of this value
    pub fn type_name(&self) -> &'static str {
        match self {
            Value::Null => "null",
            Value::Bool(_) => "boolean",
            Value::Number(_) => "number",
            Value::String(_) => "string",
            Value::Array(_) => "array",
            Value::Object(_) => "object",
            Value::BigInt(_) => "bigint",
            Value::Decimal128(_) => "decimal128",
            Value::Uuid(_) => "uuid",
            Value::Date(_) => "date",
        }
    }
}

/// Convert a serde-serializable value to a kJSON Value
pub fn to_value<T>(value: T) -> Result<Value>
where
    T: Serialize,
{
    // This is a simplified implementation
    // In a full implementation, we'd use a custom serializer
    let json_value = serde_json::to_value(value)
        .map_err(|e| Error::SerializationError(e.to_string()))?;
    json_value_to_kjson_value(json_value)
}

/// Convert a kJSON Value to a serde-deserializable type
pub fn from_value<T>(value: Value) -> Result<T>
where
    T: for<'de> Deserialize<'de>,
{
    // This is a simplified implementation
    // In a full implementation, we'd use a custom deserializer
    let json_value = kjson_value_to_json_value(value)?;
    serde_json::from_value(json_value)
        .map_err(|e| Error::Custom(e.to_string()))
}

// Helper function to convert serde_json::Value to kJSON Value
fn json_value_to_kjson_value(value: serde_json::Value) -> Result<Value> {
    match value {
        serde_json::Value::Null => Ok(Value::Null),
        serde_json::Value::Bool(b) => Ok(Value::Bool(b)),
        serde_json::Value::Number(n) => {
            if let Some(f) = n.as_f64() {
                Ok(Value::Number(f))
            } else {
                Err(Error::InvalidNumber(n.to_string()))
            }
        }
        serde_json::Value::String(s) => Ok(Value::String(s)),
        serde_json::Value::Array(arr) => {
            let mut result = Vec::new();
            for item in arr {
                result.push(json_value_to_kjson_value(item)?);
            }
            Ok(Value::Array(result))
        }
        serde_json::Value::Object(obj) => {
            let mut result = HashMap::new();
            for (key, val) in obj {
                result.insert(key, json_value_to_kjson_value(val)?);
            }
            Ok(Value::Object(result))
        }
    }
}

// Helper function to convert kJSON Value to serde_json::Value
fn kjson_value_to_json_value(value: Value) -> Result<serde_json::Value> {
    match value {
        Value::Null => Ok(serde_json::Value::Null),
        Value::Bool(b) => Ok(serde_json::Value::Bool(b)),
        Value::Number(n) => Ok(serde_json::json!(n)),
        Value::String(s) => Ok(serde_json::Value::String(s)),
        Value::Array(arr) => {
            let mut result = Vec::new();
            for item in arr {
                result.push(kjson_value_to_json_value(item)?);
            }
            Ok(serde_json::Value::Array(result))
        }
        Value::Object(obj) => {
            let mut result = serde_json::Map::new();
            for (key, val) in obj {
                result.insert(key, kjson_value_to_json_value(val)?);
            }
            Ok(serde_json::Value::Object(result))
        }
        // Extended types are serialized as strings for JSON compatibility
        Value::BigInt(b) => Ok(serde_json::Value::String(b.to_kjson_string())),
        Value::Decimal128(d) => Ok(serde_json::Value::String(d.to_kjson_string())),
        Value::Uuid(u) => Ok(serde_json::Value::String(u.to_string())),
        Value::Date(d) => Ok(serde_json::Value::String(d.to_iso8601())),
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_value_types() {
        let null = Value::Null;
        assert!(null.is_null());
        assert_eq!(null.type_name(), "null");

        let bool_val = Value::Bool(true);
        assert_eq!(bool_val.as_bool(), Some(true));
        assert_eq!(bool_val.type_name(), "boolean");

        let num_val = Value::Number(42.0);
        assert_eq!(num_val.as_f64(), Some(42.0));
        assert_eq!(num_val.type_name(), "number");
    }
}