use std::fmt;
use thiserror::Error;

/// Result type for kJSON operations
pub type Result<T> = std::result::Result<T, Error>;

/// Error type for kJSON operations
#[derive(Error, Debug)]
pub enum Error {
    /// Parse error with position information
    #[error("Parse error at position {position}: {message}")]
    ParseError {
        /// Position in the input where the error occurred
        position: usize,
        /// Error message
        message: String,
    },

    /// Invalid number format
    #[error("Invalid number: {0}")]
    InvalidNumber(String),

    /// Invalid BigInt format
    #[error("Invalid BigInt: {0}")]
    InvalidBigInt(String),

    /// Invalid Decimal128 format
    #[error("Invalid Decimal128: {0}")]
    InvalidDecimal128(String),

    /// Invalid UUID format
    #[error("Invalid UUID: {0}")]
    InvalidUuid(String),

    /// Invalid Date format
    #[error("Invalid Date: {0}")]
    InvalidDate(String),

    /// Invalid Instant format
    #[error("Invalid Instant: {0}")]
    InvalidInstant(String),

    /// Invalid Duration format
    #[error("Invalid Duration: {0}")]
    InvalidDuration(String),

    /// Serialization error
    #[error("Serialization error: {0}")]
    SerializationError(String),

    /// Type conversion error
    #[error("Type conversion error: expected {expected}, got {actual}")]
    TypeMismatch {
        /// Expected type
        expected: String,
        /// Actual type
        actual: String,
    },

    /// Unexpected end of input
    #[error("Unexpected end of input")]
    UnexpectedEof,

    /// Custom serde error
    #[error("Serde error: {0}")]
    Custom(String),

    /// IO error
    #[error("IO error: {0}")]
    IoError(#[from] std::io::Error),
}

impl serde::de::Error for Error {
    fn custom<T: fmt::Display>(msg: T) -> Self {
        Error::Custom(msg.to_string())
    }
}

impl serde::ser::Error for Error {
    fn custom<T: fmt::Display>(msg: T) -> Self {
        Error::Custom(msg.to_string())
    }
}