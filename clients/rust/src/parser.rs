use crate::error::{Error, Result};
use crate::types::{BigInt, Date, Decimal128};
use crate::value::Value;
use std::collections::HashMap;
use uuid::Uuid;

/// Parser state
pub struct Parser<'a> {
    input: &'a str,
    position: usize,
}

/// Parse a kJSON string into a Value
pub fn parse(input: &str) -> Result<Value> {
    let mut parser = Parser { input, position: 0 };
    parser.skip_whitespace();
    let value = parser.parse_value()?;
    parser.skip_whitespace();
    if parser.position < parser.input.len() {
        return Err(Error::ParseError {
            position: parser.position,
            message: "Unexpected characters after value".to_string(),
        });
    }
    Ok(value)
}

impl<'a> Parser<'a> {
    /// Current character
    fn current(&self) -> Option<char> {
        self.input.chars().nth(self.position)
    }

    /// Peek at character without advancing
    fn peek(&self) -> Option<char> {
        self.current()
    }

    /// Advance position by one character
    fn advance(&mut self) {
        if self.position < self.input.len() {
            self.position += self.current().unwrap().len_utf8();
        }
    }

    /// Skip whitespace and comments
    fn skip_whitespace(&mut self) {
        while let Some(ch) = self.current() {
            match ch {
                ' ' | '\t' | '\n' | '\r' => self.advance(),
                '/' => {
                    let next_pos = self.position + 1;
                    if next_pos < self.input.len() {
                        let next_ch = self.input.chars().nth(next_pos);
                        match next_ch {
                            Some('/') => {
                                // Line comment
                                self.advance(); // Skip first /
                                self.advance(); // Skip second /
                                while let Some(c) = self.current() {
                                    self.advance();
                                    if c == '\n' {
                                        break;
                                    }
                                }
                            }
                            Some('*') => {
                                // Block comment
                                self.advance(); // Skip /
                                self.advance(); // Skip *
                                let mut star_seen = false;
                                while let Some(c) = self.current() {
                                    self.advance();
                                    if star_seen && c == '/' {
                                        break;
                                    }
                                    star_seen = c == '*';
                                }
                            }
                            _ => return,
                        }
                    } else {
                        return;
                    }
                }
                _ => return,
            }
        }
    }

    /// Parse any value
    fn parse_value(&mut self) -> Result<Value> {
        self.skip_whitespace();

        match self.peek() {
            None => Err(Error::UnexpectedEof),
            Some('n') => self.parse_null(),
            Some('t') | Some('f') => {
                // Could be boolean or UUID starting with 't' or 'f'
                if let Ok(literal) = self.try_parse_unquoted_literal() {
                    Ok(literal)
                } else {
                    self.parse_bool()
                }
            }
            Some('"') | Some('\'') | Some('`') => self.parse_string(),
            Some('[') => self.parse_array(),
            Some('{') => self.parse_object(),
            Some('-') | Some('0'..='9') => {
                // Could be number or date/UUID
                if let Ok(literal) = self.try_parse_unquoted_literal() {
                    Ok(literal)
                } else {
                    self.parse_number()
                }
            }
            Some(_) => self.parse_unquoted_literal(),
        }
    }

    /// Parse null
    fn parse_null(&mut self) -> Result<Value> {
        if self.input[self.position..].starts_with("null") {
            self.position += 4;
            Ok(Value::Null)
        } else {
            Err(Error::ParseError {
                position: self.position,
                message: "Invalid null value".to_string(),
            })
        }
    }

    /// Parse boolean
    fn parse_bool(&mut self) -> Result<Value> {
        if self.input[self.position..].starts_with("true") {
            self.position += 4;
            Ok(Value::Bool(true))
        } else if self.input[self.position..].starts_with("false") {
            self.position += 5;
            Ok(Value::Bool(false))
        } else {
            Err(Error::ParseError {
                position: self.position,
                message: "Invalid boolean value".to_string(),
            })
        }
    }

    /// Parse string
    fn parse_string(&mut self) -> Result<Value> {
        let quote_char = match self.current() {
            Some('"') | Some('\'') | Some('`') => self.current().unwrap(),
            _ => {
                return Err(Error::ParseError {
                    position: self.position,
                    message: "Expected quote character".to_string(),
                });
            }
        };
        self.advance(); // Skip opening quote

        let mut result = String::new();
        let mut escape = false;

        while let Some(ch) = self.current() {
            if escape {
                match ch {
                    '"' => result.push('"'),
                    '\'' => result.push('\''),
                    '`' => result.push('`'),
                    '\\' => result.push('\\'),
                    '/' => result.push('/'),
                    'b' => result.push('\u{0008}'),
                    'f' => result.push('\u{000C}'),
                    'n' => result.push('\n'),
                    'r' => result.push('\r'),
                    't' => result.push('\t'),
                    'u' => {
                        // Unicode escape
                        self.advance();
                        let mut hex = String::new();
                        for _ in 0..4 {
                            if let Some(ch) = self.current() {
                                hex.push(ch);
                                self.advance();
                            } else {
                                return Err(Error::ParseError {
                                    position: self.position,
                                    message: "Invalid unicode escape".to_string(),
                                });
                            }
                        }
                        let code_point = u32::from_str_radix(&hex, 16)
                            .map_err(|_| Error::ParseError {
                                position: self.position,
                                message: "Invalid unicode escape".to_string(),
                            })?;
                        if let Some(ch) = char::from_u32(code_point) {
                            result.push(ch);
                        } else {
                            return Err(Error::ParseError {
                                position: self.position,
                                message: "Invalid unicode code point".to_string(),
                            });
                        }
                        escape = false;
                        continue;
                    }
                    _ => {
                        return Err(Error::ParseError {
                            position: self.position,
                            message: format!("Invalid escape sequence: \\{}", ch),
                        })
                    }
                }
                escape = false;
                self.advance();
            } else if ch == '\\' {
                escape = true;
                self.advance();
            } else if ch == quote_char {
                self.advance();
                return Ok(Value::String(result));
            } else {
                result.push(ch);
                self.advance();
            }
        }

        Err(Error::ParseError {
            position: self.position,
            message: "Unterminated string".to_string(),
        })
    }

    /// Parse array
    fn parse_array(&mut self) -> Result<Value> {
        if self.current() != Some('[') {
            return Err(Error::ParseError {
                position: self.position,
                message: "Expected '['".to_string(),
            });
        }
        self.advance();

        let mut items = Vec::new();
        self.skip_whitespace();

        if self.current() == Some(']') {
            self.advance();
            return Ok(Value::Array(items));
        }

        loop {
            items.push(self.parse_value()?);
            self.skip_whitespace();

            match self.current() {
                Some(',') => {
                    self.advance();
                    self.skip_whitespace();
                    // Allow trailing comma
                    if self.current() == Some(']') {
                        self.advance();
                        break;
                    }
                }
                Some(']') => {
                    self.advance();
                    break;
                }
                _ => {
                    return Err(Error::ParseError {
                        position: self.position,
                        message: "Expected ',' or ']'".to_string(),
                    })
                }
            }
        }

        Ok(Value::Array(items))
    }

    /// Parse object
    fn parse_object(&mut self) -> Result<Value> {
        if self.current() != Some('{') {
            return Err(Error::ParseError {
                position: self.position,
                message: "Expected '{'".to_string(),
            });
        }
        self.advance();

        let mut map = HashMap::new();
        self.skip_whitespace();

        if self.current() == Some('}') {
            self.advance();
            return Ok(Value::Object(map));
        }

        loop {
            // Parse key
            self.skip_whitespace();
            let key = match self.current() {
                Some('"') | Some('\'') | Some('`') => {
                    // Quoted key
                    match self.parse_string()? {
                        Value::String(s) => s,
                        _ => unreachable!(),
                    }
                }
                _ => {
                    // Unquoted key (JSON5 style)
                    self.parse_unquoted_key()?
                }
            };

            self.skip_whitespace();
            if self.current() != Some(':') {
                return Err(Error::ParseError {
                    position: self.position,
                    message: "Expected ':' after key".to_string(),
                });
            }
            self.advance();

            // Parse value
            let value = self.parse_value()?;
            map.insert(key, value);

            self.skip_whitespace();
            match self.current() {
                Some(',') => {
                    self.advance();
                    self.skip_whitespace();
                    // Allow trailing comma
                    if self.current() == Some('}') {
                        self.advance();
                        break;
                    }
                }
                Some('}') => {
                    self.advance();
                    break;
                }
                _ => {
                    return Err(Error::ParseError {
                        position: self.position,
                        message: "Expected ',' or '}'".to_string(),
                    })
                }
            }
        }

        Ok(Value::Object(map))
    }

    /// Parse unquoted key (JSON5 style)
    fn parse_unquoted_key(&mut self) -> Result<String> {
        let start = self.position;

        // First character must be letter, underscore, or dollar sign
        match self.current() {
            Some(ch) if ch.is_alphabetic() || ch == '_' || ch == '$' => {
                self.advance();
            }
            _ => {
                return Err(Error::ParseError {
                    position: self.position,
                    message: "Invalid unquoted key".to_string(),
                })
            }
        }

        // Subsequent characters
        while let Some(ch) = self.current() {
            if ch.is_alphanumeric() || ch == '_' || ch == '$' {
                self.advance();
            } else {
                break;
            }
        }

        Ok(self.input[start..self.position].to_string())
    }

    /// Parse number (including BigInt and Decimal128)
    fn parse_number(&mut self) -> Result<Value> {
        let start = self.position;

        // Optional negative sign
        if self.current() == Some('-') {
            self.advance();
        }

        // Integer part
        if self.current() == Some('0') {
            self.advance();
        } else {
            while let Some(ch) = self.current() {
                if ch.is_ascii_digit() {
                    self.advance();
                } else {
                    break;
                }
            }
        }

        // Fractional part
        let has_decimal = self.current() == Some('.');
        if has_decimal {
            self.advance();
            let frac_start = self.position;
            while let Some(ch) = self.current() {
                if ch.is_ascii_digit() {
                    self.advance();
                } else {
                    break;
                }
            }
            if self.position == frac_start {
                return Err(Error::ParseError {
                    position: self.position,
                    message: "Expected digits after decimal point".to_string(),
                });
            }
        }

        // Exponent part
        let has_exponent = matches!(self.current(), Some('e') | Some('E'));
        if has_exponent {
            self.advance();
            if matches!(self.current(), Some('+') | Some('-')) {
                self.advance();
            }
            let exp_start = self.position;
            while let Some(ch) = self.current() {
                if ch.is_ascii_digit() {
                    self.advance();
                } else {
                    break;
                }
            }
            if self.position == exp_start {
                return Err(Error::ParseError {
                    position: self.position,
                    message: "Expected digits in exponent".to_string(),
                });
            }
        }

        // Check for BigInt suffix
        if self.current() == Some('n') {
            self.advance();
            let num_str = &self.input[start..self.position - 1];
            let bigint = BigInt::from_str(num_str)?;
            return Ok(Value::BigInt(bigint));
        }

        // Check for Decimal128 suffix
        if self.current() == Some('m') {
            self.advance();
            let num_str = &self.input[start..self.position - 1];
            let decimal = Decimal128::from_str(num_str)?;
            return Ok(Value::Decimal128(decimal));
        }

        // Regular number
        let num_str = &self.input[start..self.position];
        let num = num_str
            .parse::<f64>()
            .map_err(|_| Error::InvalidNumber(num_str.to_string()))?;
        Ok(Value::Number(num))
    }

    /// Try to parse unquoted literal (UUID, Date)
    fn try_parse_unquoted_literal(&mut self) -> Result<Value> {
        let saved_pos = self.position;
        match self.parse_unquoted_literal() {
            Ok(val) => Ok(val),
            Err(_) => {
                self.position = saved_pos;
                Err(Error::ParseError {
                    position: self.position,
                    message: "Not a valid literal".to_string(),
                })
            }
        }
    }

    /// Parse unquoted literal (UUID, Date)
    fn parse_unquoted_literal(&mut self) -> Result<Value> {
        let start = self.position;

        // Read until delimiter
        while let Some(ch) = self.current() {
            match ch {
                ' ' | '\t' | '\n' | '\r' | ',' | ']' | '}' => break,
                _ => self.advance(),
            }
        }

        let literal = &self.input[start..self.position];

        // Try to parse as UUID
        if let Ok(uuid) = Uuid::parse_str(literal) {
            return Ok(Value::Uuid(uuid));
        }

        // Try to parse as Date
        if let Ok(date) = Date::from_iso8601(literal) {
            return Ok(Value::Date(date));
        }

        Err(Error::ParseError {
            position: start,
            message: format!("Invalid literal: {}", literal),
        })
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_parse_primitives() {
        assert_eq!(parse("null").unwrap(), Value::Null);
        assert_eq!(parse("true").unwrap(), Value::Bool(true));
        assert_eq!(parse("false").unwrap(), Value::Bool(false));
        assert_eq!(parse("123").unwrap(), Value::Number(123.0));
        assert_eq!(parse("3.14").unwrap(), Value::Number(3.14));
        assert_eq!(parse("\"hello\"").unwrap(), Value::String("hello".to_string()));
    }

    #[test]
    fn test_parse_extended_types() {
        // BigInt
        match parse("123456789012345678n").unwrap() {
            Value::BigInt(b) => assert_eq!(b.to_string(), "123456789012345678"),
            _ => panic!("Expected BigInt"),
        }

        // Decimal128
        match parse("99.99m").unwrap() {
            Value::Decimal128(d) => assert_eq!(d.to_string(), "99.99"),
            _ => panic!("Expected Decimal128"),
        }

        // UUID
        match parse("550e8400-e29b-41d4-a716-446655440000").unwrap() {
            Value::Uuid(u) => assert_eq!(u.to_string(), "550e8400-e29b-41d4-a716-446655440000"),
            _ => panic!("Expected UUID"),
        }

        // Date
        match parse("2025-01-10T12:00:00Z").unwrap() {
            Value::Date(_) => (), // Date parsing tested in types module
            _ => panic!("Expected Date"),
        }
    }

    #[test]
    fn test_parse_array() {
        let result = parse("[1, 2, 3]").unwrap();
        match result {
            Value::Array(arr) => {
                assert_eq!(arr.len(), 3);
                assert_eq!(arr[0], Value::Number(1.0));
                assert_eq!(arr[1], Value::Number(2.0));
                assert_eq!(arr[2], Value::Number(3.0));
            }
            _ => panic!("Expected array"),
        }
    }

    #[test]
    fn test_parse_object() {
        let result = parse(r#"{"name": "test", "value": 42}"#).unwrap();
        match result {
            Value::Object(obj) => {
                assert_eq!(obj.get("name"), Some(&Value::String("test".to_string())));
                assert_eq!(obj.get("value"), Some(&Value::Number(42.0)));
            }
            _ => panic!("Expected object"),
        }
    }

    #[test]
    fn test_parse_json5_features() {
        // Unquoted keys
        let result = parse("{name: \"test\", value: 42}").unwrap();
        match result {
            Value::Object(obj) => {
                assert_eq!(obj.get("name"), Some(&Value::String("test".to_string())));
                assert_eq!(obj.get("value"), Some(&Value::Number(42.0)));
            }
            _ => panic!("Expected object"),
        }

        // Trailing commas
        assert!(parse("[1, 2, 3,]").is_ok());
        assert!(parse("{a: 1,}").is_ok());
    }
}