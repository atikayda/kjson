use crate::error::Result;
use crate::value::Value;
use std::io::Write;

/// Serialize a Value to a kJSON string
pub fn to_string(value: &Value) -> Result<String> {
    let mut buf = Vec::new();
    write_value(&mut buf, value, 0, false)?;
    Ok(String::from_utf8_lossy(&buf).into_owned())
}

/// Serialize a Value to a pretty-printed kJSON string
pub fn to_string_pretty(value: &Value) -> Result<String> {
    let mut buf = Vec::new();
    write_value(&mut buf, value, 0, true)?;
    Ok(String::from_utf8_lossy(&buf).into_owned())
}

/// Write a value to a writer
fn write_value<W: Write>(writer: &mut W, value: &Value, indent: usize, pretty: bool) -> Result<()> {
    match value {
        Value::Null => write!(writer, "null")?,
        Value::Bool(b) => write!(writer, "{}", b)?,
        Value::Number(n) => {
            if n.is_finite() {
                // Write number ensuring proper formatting
                if n.fract() == 0.0 && n.abs() < 1e15 {
                    write!(writer, "{:.0}", n)?;
                } else {
                    write!(writer, "{}", n)?;
                }
            } else {
                write!(writer, "null")?; // JSON doesn't support Infinity/NaN
            }
        }
        Value::String(s) => write_string(writer, s)?,
        Value::Array(arr) => write_array(writer, arr, indent, pretty)?,
        Value::Object(obj) => write_object(writer, obj, indent, pretty)?,
        Value::BigInt(b) => write!(writer, "{}", b.to_kjson_string())?,
        Value::Decimal128(d) => write!(writer, "{}", d.to_kjson_string())?,
        Value::Uuid(u) => write!(writer, "{}", u)?,
        Value::Date(d) => write!(writer, "{}", d.to_iso8601())?,
    }
    Ok(())
}

/// Write a string with smart quote selection
fn write_string<W: Write>(writer: &mut W, s: &str) -> Result<()> {
    let quote_char = select_quote_char(s);
    
    write!(writer, "{}", quote_char)?;
    for ch in s.chars() {
        match ch {
            '\\' => write!(writer, "\\\\")?,
            '\u{0008}' => write!(writer, "\\b")?,
            '\u{000C}' => write!(writer, "\\f")?,
            '\n' => write!(writer, "\\n")?,
            '\r' => write!(writer, "\\r")?,
            '\t' => write!(writer, "\\t")?,
            ch if ch.is_control() => {
                write!(writer, "\\u{:04x}", ch as u32)?;
            }
            ch if ch == quote_char => {
                write!(writer, "\\{}", ch)?;
            }
            ch => write!(writer, "{}", ch)?,
        }
    }
    write!(writer, "{}", quote_char)?;
    Ok(())
}

/// Select the best quote character for a string based on content
fn select_quote_char(s: &str) -> char {
    // Count occurrences of each quote type
    let single_quotes = s.chars().filter(|&c| c == '\'').count();
    let double_quotes = s.chars().filter(|&c| c == '"').count();
    let backticks = s.chars().filter(|&c| c == '`').count();
    let backslashes = s.chars().filter(|&c| c == '\\').count();
    
    // Calculate escaping cost for each quote type
    let single_cost = single_quotes + backslashes;
    let double_cost = double_quotes + backslashes;
    let backtick_cost = backticks + backslashes;
    
    // Find minimum cost and choose quote type
    // In case of tie: single > double > backtick
    let mut min_cost = single_cost;
    let mut quote_char = '\'';
    
    if double_cost < min_cost {
        min_cost = double_cost;
        quote_char = '"';
    }
    
    if backtick_cost < min_cost {
        quote_char = '`';
    }
    
    quote_char
}

/// Write an array
fn write_array<W: Write>(
    writer: &mut W,
    arr: &[Value],
    indent: usize,
    pretty: bool,
) -> Result<()> {
    write!(writer, "[")?;
    
    if arr.is_empty() {
        write!(writer, "]")?;
        return Ok(());
    }

    for (i, item) in arr.iter().enumerate() {
        if pretty {
            write!(writer, "\n{}", "  ".repeat(indent + 1))?;
        }
        
        write_value(writer, item, indent + 1, pretty)?;
        
        if i < arr.len() - 1 {
            write!(writer, ",")?;
            if !pretty {
                write!(writer, " ")?;
            }
        } else if pretty {
            write!(writer, "\n{}", "  ".repeat(indent))?;
        }
    }
    
    write!(writer, "]")?;
    Ok(())
}

/// Write an object
fn write_object<W: Write>(
    writer: &mut W,
    obj: &std::collections::HashMap<String, Value>,
    indent: usize,
    pretty: bool,
) -> Result<()> {
    write!(writer, "{{")?;
    
    if obj.is_empty() {
        write!(writer, "}}")?;
        return Ok(());
    }

    let mut items: Vec<_> = obj.iter().collect();
    items.sort_by_key(|(k, _)| k.as_str());

    for (i, (key, value)) in items.iter().enumerate() {
        if pretty {
            write!(writer, "\n{}", "  ".repeat(indent + 1))?;
        }
        
        // Check if key needs quotes
        if needs_quotes(key) {
            write_string(writer, key)?;
        } else {
            write!(writer, "{}", key)?;
        }
        
        write!(writer, ":")?;
        write!(writer, " ")?;
        
        write_value(writer, value, indent + 1, pretty)?;
        
        if i < items.len() - 1 {
            write!(writer, ",")?;
            if !pretty {
                write!(writer, " ")?;
            }
        } else if pretty {
            write!(writer, "\n{}", "  ".repeat(indent))?;
        }
    }
    
    write!(writer, "}}")?;
    Ok(())
}

/// Check if a key needs quotes (JSON5 style)
fn needs_quotes(key: &str) -> bool {
    if key.is_empty() {
        return true;
    }

    let mut chars = key.chars();
    let first = chars.next().unwrap();
    
    // First character must be letter, underscore, or dollar sign
    if !first.is_alphabetic() && first != '_' && first != '$' {
        return true;
    }

    // Rest must be alphanumeric, underscore, or dollar sign
    for ch in chars {
        if !ch.is_alphanumeric() && ch != '_' && ch != '$' {
            return true;
        }
    }

    false
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::types::{BigInt, Decimal128};
    use std::collections::HashMap;

    #[test]
    fn test_serialize_primitives() {
        assert_eq!(to_string(&Value::Null).unwrap(), "null");
        assert_eq!(to_string(&Value::Bool(true)).unwrap(), "true");
        assert_eq!(to_string(&Value::Bool(false)).unwrap(), "false");
        assert_eq!(to_string(&Value::Number(42.0)).unwrap(), "42");
        assert_eq!(to_string(&Value::Number(3.14)).unwrap(), "3.14");
        assert_eq!(to_string(&Value::String("hello".to_string())).unwrap(), "'hello'");
    }

    #[test]
    fn test_serialize_extended_types() {
        let bigint = BigInt::from_i64(123456789012345678);
        assert_eq!(to_string(&Value::BigInt(bigint)).unwrap(), "123456789012345678n");

        let decimal = Decimal128::from_str("99.99").unwrap();
        assert_eq!(to_string(&Value::Decimal128(decimal)).unwrap(), "99.99m");

        let uuid = uuid::Uuid::parse_str("550e8400-e29b-41d4-a716-446655440000").unwrap();
        assert_eq!(
            to_string(&Value::Uuid(uuid)).unwrap(),
            "550e8400-e29b-41d4-a716-446655440000"
        );
    }

    #[test]
    fn test_serialize_array() {
        let arr = vec![
            Value::Number(1.0),
            Value::Number(2.0),
            Value::Number(3.0),
        ];
        assert_eq!(to_string(&Value::Array(arr)).unwrap(), "[1, 2, 3]");
    }

    #[test]
    fn test_serialize_object() {
        let mut obj = HashMap::new();
        obj.insert("name".to_string(), Value::String("test".to_string()));
        obj.insert("value".to_string(), Value::Number(42.0));
        
        let result = to_string(&Value::Object(obj)).unwrap();
        // Object keys are sorted
        assert_eq!(result, "{name: 'test', value: 42}");
    }

    #[test]
    fn test_serialize_pretty() {
        let mut obj = HashMap::new();
        obj.insert("a".to_string(), Value::Number(1.0));
        obj.insert("b".to_string(), Value::Array(vec![
            Value::Number(2.0),
            Value::Number(3.0),
        ]));
        
        let result = to_string_pretty(&Value::Object(obj)).unwrap();
        let expected = "{\n  a: 1,\n  b: [\n    2,\n    3\n  ]\n}";
        assert_eq!(result, expected);
    }

    #[test]
    fn test_string_escaping() {
        let s = "Hello\n\"World\"\t\\";
        let result = to_string(&Value::String(s.to_string())).unwrap();
        // String has double quotes, so single quotes should be used
        assert_eq!(result, r#"'Hello\n"World"\t\\'"#);
    }

    #[test]
    fn test_key_quoting() {
        let mut obj = HashMap::new();
        obj.insert("validKey".to_string(), Value::Number(1.0));
        obj.insert("needs-quotes".to_string(), Value::Number(2.0));
        obj.insert("123invalid".to_string(), Value::Number(3.0));
        
        let result = to_string(&Value::Object(obj)).unwrap();
        // Keys with hyphens use single quotes (smart quote selection)
        assert!(result.contains("'123invalid': 3"));
        assert!(result.contains("'needs-quotes': 2"));
        assert!(result.contains("validKey: 1"));
    }

    #[test]
    fn test_smart_quote_selection() {
        // No quotes - use single quotes
        let result = to_string(&Value::String("hello".to_string())).unwrap();
        assert_eq!(result, "'hello'");
        
        // Has single quotes - use double quotes
        let result = to_string(&Value::String("it's nice".to_string())).unwrap();
        assert_eq!(result, r#""it's nice""#);
        
        // Has double quotes - use single quotes
        let result = to_string(&Value::String(r#"He said "hi""#.to_string())).unwrap();
        assert_eq!(result, r#"'He said "hi"'"#);
        
        // Has both single and double quotes - use backticks
        let result = to_string(&Value::String(r#"He said "hello" and 'hi'"#.to_string())).unwrap();
        assert_eq!(result, r#"`He said "hello" and 'hi'`"#);
    }

    #[test]
    fn test_backtick_strings() {
        // Template string with both quote types
        let result = to_string(&Value::String("Mix 'both' \"types\"".to_string())).unwrap();
        assert_eq!(result, "`Mix 'both' \"types\"`");
        
        // String with backticks uses different quote (single wins in tie)
        let result = to_string(&Value::String("template `string`".to_string())).unwrap();
        assert_eq!(result, "'template `string`'");
    }
}