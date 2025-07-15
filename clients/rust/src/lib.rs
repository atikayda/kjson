//! kJSON (Kind JSON) - Extended JSON with native type support
//!
//! This crate provides a Rust implementation of the kJSON specification,
//! supporting extended types like BigInt, Decimal128, UUID, and Date.

#![warn(missing_docs)]

mod error;
mod parser;
mod serializer;
mod types;
mod value;

pub use error::{Error, Result};
pub use parser::parse;
pub use serializer::{to_string, to_string_pretty as serializer_to_string_pretty};
pub use types::{BigInt, Instant, Duration, Date, Decimal128, uuid_v4, uuid_v7};
pub use value::{from_value, to_value, Value};

// Re-export UUID type
pub use uuid::Uuid;

// Public convenience functions
/// Parse a kJSON string into a Rust value
pub fn from_str<T>(s: &str) -> Result<T>
where
    T: for<'de> serde::Deserialize<'de>,
{
    let value = parse(s)?;
    from_value(value)
}

/// Serialize a Rust value to a kJSON string
pub fn to_string_pretty<T>(value: &T) -> Result<String>
where
    T: serde::Serialize,
{
    let val = to_value(value)?;
    serializer::to_string_pretty(&val)
}

// Feature-gated derive macro re-export (coming soon)
// #[cfg(feature = "derive")]
// pub use kjson_derive::{Deserialize, Serialize};

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_basic_parse() {
        let result: f64 = from_str("123").unwrap();
        assert_eq!(result, 123.0);
    }
}