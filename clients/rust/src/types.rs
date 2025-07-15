use crate::error::{Error, Result};
use chrono::{DateTime, FixedOffset, TimeZone, Utc, Offset};
use num_bigint::BigInt as NumBigInt;
use num_traits::Num;
use std::fmt;
use std::str::FromStr;

/// BigInt type for arbitrary precision integers
#[derive(Debug, Clone, PartialEq, Eq, Hash)]
pub struct BigInt {
    value: NumBigInt,
}

impl BigInt {
    /// Create a new BigInt from an i64
    pub fn from_i64(n: i64) -> Self {
        BigInt {
            value: NumBigInt::from(n),
        }
    }

    /// Create a new BigInt from a string
    pub fn from_str(s: &str) -> Result<Self> {
        let s = s.trim_end_matches('n');
        match NumBigInt::from_str_radix(s, 10) {
            Ok(value) => Ok(BigInt { value }),
            Err(_) => Err(Error::InvalidBigInt(s.to_string())),
        }
    }

    /// Convert to string representation without suffix
    pub fn to_string(&self) -> String {
        self.value.to_string()
    }

    /// Convert to kJSON string representation with 'n' suffix
    pub fn to_kjson_string(&self) -> String {
        format!("{}n", self.value)
    }
}

impl fmt::Display for BigInt {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{}", self.value)
    }
}

impl FromStr for BigInt {
    type Err = Error;

    fn from_str(s: &str) -> Result<Self> {
        BigInt::from_str(s)
    }
}

/// Decimal128 type for high-precision decimal numbers
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Decimal128 {
    /// The digits of the decimal number (without decimal point)
    digits: String,
    /// The exponent (negative for decimal places)
    exponent: i32,
    /// Whether the number is negative
    negative: bool,
}

impl Decimal128 {
    /// Create a new Decimal128 from a string
    pub fn from_str(s: &str) -> Result<Self> {
        let s = s.trim_end_matches('m');
        let negative = s.starts_with('-');
        let s = s.trim_start_matches('-');

        // Find decimal point
        if let Some(dot_pos) = s.find('.') {
            let integer_part = &s[..dot_pos];
            let decimal_part = &s[dot_pos + 1..];
            let digits = format!("{}{}", integer_part, decimal_part);
            let exponent = -(decimal_part.len() as i32);

            Ok(Decimal128 {
                digits,
                exponent,
                negative,
            })
        } else {
            Ok(Decimal128 {
                digits: s.to_string(),
                exponent: 0,
                negative,
            })
        }
    }

    /// Create from float64
    pub fn from_f64(f: f64) -> Self {
        let s = format!("{}", f);
        Self::from_str(&s).unwrap_or_else(|_| Decimal128 {
            digits: "0".to_string(),
            exponent: 0,
            negative: false,
        })
    }

    /// Convert to string representation without suffix
    pub fn to_string(&self) -> String {
        if self.exponent == 0 {
            format!("{}{}", if self.negative { "-" } else { "" }, self.digits)
        } else if self.exponent < 0 {
            let exp = (-self.exponent) as usize;
            let len = self.digits.len();
            let result = if exp >= len {
                let zeros = "0".repeat(exp - len + 1);
                format!("0.{}{}", zeros, self.digits)
            } else {
                let (integer, decimal) = self.digits.split_at(len - exp);
                format!("{}.{}", integer, decimal)
            };
            format!("{}{}", if self.negative { "-" } else { "" }, result)
        } else {
            // Positive exponent
            let zeros = "0".repeat(self.exponent as usize);
            format!(
                "{}{}{}",
                if self.negative { "-" } else { "" },
                self.digits,
                zeros
            )
        }
    }

    /// Convert to kJSON string representation with 'm' suffix
    pub fn to_kjson_string(&self) -> String {
        format!("{}m", self.to_string())
    }
}

impl fmt::Display for Decimal128 {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{}", self.to_string())
    }
}

impl FromStr for Decimal128 {
    type Err = Error;

    fn from_str(s: &str) -> Result<Self> {
        Decimal128::from_str(s)
    }
}

/// Instant type representing a nanosecond-precision timestamp in Zulu time (UTC)
#[derive(Debug, Clone, PartialEq, Eq, PartialOrd, Ord)]
pub struct Instant {
    /// Nanoseconds since Unix epoch (UTC)
    pub nanoseconds: i64,
}

impl Instant {
    /// Create a new Instant from nanoseconds since epoch
    pub fn from_nanos(nanoseconds: i64) -> Self {
        Instant { nanoseconds }
    }

    /// Create a new Instant from milliseconds since epoch
    pub fn from_millis(milliseconds: i64) -> Self {
        Instant {
            nanoseconds: milliseconds * 1_000_000,
        }
    }

    /// Create a new Instant from seconds since epoch
    pub fn from_seconds(seconds: i64) -> Self {
        Instant {
            nanoseconds: seconds * 1_000_000_000,
        }
    }

    /// Get the current instant
    pub fn now() -> Self {
        use std::time::{SystemTime, UNIX_EPOCH};
        let duration = SystemTime::now()
            .duration_since(UNIX_EPOCH)
            .unwrap_or_default();
        Instant::from_nanos(duration.as_nanos() as i64)
    }

    /// Parse ISO 8601 string to Instant
    pub fn from_iso8601(s: &str) -> Result<Self> {
        // Convert to Zulu time if it has a timezone
        let zulu_string = if s.contains('+') || (s.matches('-').count() > 2) {
            // Has timezone offset, convert to Zulu
            let dt = DateTime::parse_from_rfc3339(s)
                .map_err(|_| Error::InvalidDate(s.to_string()))?;
            dt.with_timezone(&Utc).format("%Y-%m-%dT%H:%M:%S%.9fZ").to_string()
        } else if !s.ends_with('Z') {
            // No timezone specified, assume Zulu
            format!("{}Z", s)
        } else {
            s.to_string()
        };

        // Parse the Zulu string manually to preserve nanosecond precision
        let re = regex::Regex::new(r"^(\d{4})-(\d{2})-(\d{2})T(\d{2}):(\d{2}):(\d{2})(?:\.(\d+))?Z$")
            .map_err(|_| Error::InvalidDate(s.to_string()))?;
        
        let captures = re.captures(&zulu_string)
            .ok_or_else(|| Error::InvalidDate(s.to_string()))?;

        let year: i32 = captures[1].parse()
            .map_err(|_| Error::InvalidDate(s.to_string()))?;
        let month: u32 = captures[2].parse()
            .map_err(|_| Error::InvalidDate(s.to_string()))?;
        let day: u32 = captures[3].parse()
            .map_err(|_| Error::InvalidDate(s.to_string()))?;
        let hour: u32 = captures[4].parse()
            .map_err(|_| Error::InvalidDate(s.to_string()))?;
        let minute: u32 = captures[5].parse()
            .map_err(|_| Error::InvalidDate(s.to_string()))?;
        let second: u32 = captures[6].parse()
            .map_err(|_| Error::InvalidDate(s.to_string()))?;

        // Create datetime for the main parts
        let dt = Utc.with_ymd_and_hms(year, month, day, hour, minute, second)
            .single()
            .ok_or_else(|| Error::InvalidDate(s.to_string()))?;

        let mut nanos = dt.timestamp_nanos_opt()
            .ok_or_else(|| Error::InvalidDate(s.to_string()))?;

        // Handle fractional seconds
        if let Some(fraction_str) = captures.get(7) {
            // Pad or truncate to 9 digits (nanoseconds)
            let padded_fraction = format!("{:<09}", fraction_str.as_str());
            let truncated_fraction = &padded_fraction[..9];
            let fraction_nanos: i64 = truncated_fraction.parse()
                .map_err(|_| Error::InvalidDate(s.to_string()))?;
            
            // Remove existing nanoseconds and add the precise ones
            let seconds_part = nanos / 1_000_000_000;
            nanos = seconds_part * 1_000_000_000 + fraction_nanos;
        }

        Ok(Instant { nanoseconds: nanos })
    }

    /// Convert to ISO 8601 string with nanosecond precision
    pub fn to_iso8601(&self) -> String {
        let seconds = self.nanoseconds / 1_000_000_000;
        let nanos_remainder = self.nanoseconds % 1_000_000_000;

        // Create datetime from seconds
        let dt = DateTime::from_timestamp(seconds, 0)
            .unwrap_or_else(|| Utc::now());

        if nanos_remainder == 0 {
            dt.format("%Y-%m-%dT%H:%M:%SZ").to_string()
        } else {
            // Format nanoseconds (remove trailing zeros)
            let fractional_str = format!("{:09}", nanos_remainder).trim_end_matches('0');
            dt.format(&format!("%Y-%m-%dT%H:%M:%S.{}Z", fractional_str)).to_string()
        }
    }

    /// Convert to DateTime<Utc> (loses nanosecond precision)
    pub fn to_datetime(&self) -> DateTime<Utc> {
        let seconds = self.nanoseconds / 1_000_000_000;
        let nanos_remainder = (self.nanoseconds % 1_000_000_000) as u32;
        DateTime::from_timestamp(seconds, nanos_remainder)
            .unwrap_or_else(|| Utc::now())
    }

    /// Get nanoseconds since epoch
    pub fn epoch_nanos(&self) -> i64 {
        self.nanoseconds
    }

    /// Get milliseconds since epoch
    pub fn epoch_millis(&self) -> i64 {
        self.nanoseconds / 1_000_000
    }

    /// Get seconds since epoch
    pub fn epoch_seconds(&self) -> i64 {
        self.nanoseconds / 1_000_000_000
    }
}

impl std::fmt::Display for Instant {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.to_iso8601())
    }
}

impl FromStr for Instant {
    type Err = Error;

    fn from_str(s: &str) -> Result<Self> {
        Instant::from_iso8601(s)
    }
}

/// Duration type representing a time span with nanosecond precision
#[derive(Debug, Clone, PartialEq, Eq, PartialOrd, Ord)]
pub struct Duration {
    /// Duration in nanoseconds
    pub nanoseconds: i64,
}

impl Duration {
    /// Create a new Duration from nanoseconds
    pub fn from_nanos(nanoseconds: i64) -> Self {
        Duration { nanoseconds }
    }

    /// Create a new Duration from milliseconds
    pub fn from_millis(milliseconds: i64) -> Self {
        Duration {
            nanoseconds: milliseconds * 1_000_000,
        }
    }

    /// Create a new Duration from seconds
    pub fn from_seconds(seconds: i64) -> Self {
        Duration {
            nanoseconds: seconds * 1_000_000_000,
        }
    }

    /// Create a new Duration from minutes
    pub fn from_minutes(minutes: i64) -> Self {
        Duration {
            nanoseconds: minutes * 60 * 1_000_000_000,
        }
    }

    /// Create a new Duration from hours
    pub fn from_hours(hours: i64) -> Self {
        Duration {
            nanoseconds: hours * 3600 * 1_000_000_000,
        }
    }

    /// Create a new Duration from days
    pub fn from_days(days: i64) -> Self {
        Duration {
            nanoseconds: days * 86400 * 1_000_000_000,
        }
    }

    /// Parse ISO 8601 duration string
    pub fn from_iso8601(s: &str) -> Result<Self> {
        let re = regex::Regex::new(r"^P(?:(\d+)D)?(?:T(?:(\d+)H)?(?:(\d+)M)?(?:(\d+(?:\.\d+)?)S)?)?$")
            .map_err(|_| Error::InvalidDuration(s.to_string()))?;
        
        let captures = re.captures(s)
            .ok_or_else(|| Error::InvalidDuration(s.to_string()))?;

        let mut total_nanos = 0i64;

        // Days
        if let Some(days_str) = captures.get(1) {
            let days: i64 = days_str.as_str().parse()
                .map_err(|_| Error::InvalidDuration(s.to_string()))?;
            total_nanos += days * 86400 * 1_000_000_000;
        }

        // Hours
        if let Some(hours_str) = captures.get(2) {
            let hours: i64 = hours_str.as_str().parse()
                .map_err(|_| Error::InvalidDuration(s.to_string()))?;
            total_nanos += hours * 3600 * 1_000_000_000;
        }

        // Minutes
        if let Some(minutes_str) = captures.get(3) {
            let minutes: i64 = minutes_str.as_str().parse()
                .map_err(|_| Error::InvalidDuration(s.to_string()))?;
            total_nanos += minutes * 60 * 1_000_000_000;
        }

        // Seconds
        if let Some(seconds_str) = captures.get(4) {
            let seconds: f64 = seconds_str.as_str().parse()
                .map_err(|_| Error::InvalidDuration(s.to_string()))?;
            total_nanos += (seconds * 1_000_000_000.0) as i64;
        }

        Ok(Duration { nanoseconds: total_nanos })
    }

    /// Convert to ISO 8601 duration string
    pub fn to_iso8601(&self) -> String {
        if self.nanoseconds == 0 {
            return "PT0S".to_string();
        }

        let mut remaining = self.nanoseconds.abs();
        let mut result = String::from("P");

        // Days
        let days = remaining / (86400 * 1_000_000_000);
        if days > 0 {
            result.push_str(&format!("{}D", days));
            remaining %= 86400 * 1_000_000_000;
        }

        if remaining > 0 {
            result.push('T');

            // Hours
            let hours = remaining / (3600 * 1_000_000_000);
            if hours > 0 {
                result.push_str(&format!("{}H", hours));
                remaining %= 3600 * 1_000_000_000;
            }

            // Minutes
            let minutes = remaining / (60 * 1_000_000_000);
            if minutes > 0 {
                result.push_str(&format!("{}M", minutes));
                remaining %= 60 * 1_000_000_000;
            }

            // Seconds (with fractional part)
            if remaining > 0 {
                let seconds = remaining / 1_000_000_000;
                let nanos_part = remaining % 1_000_000_000;

                if nanos_part == 0 {
                    result.push_str(&format!("{}S", seconds));
                } else {
                    let fractional_str = format!("{:09}", nanos_part).trim_end_matches('0');
                    result.push_str(&format!("{}.{}S", seconds, fractional_str));
                }
            }
        }

        // Handle negative durations
        if self.nanoseconds < 0 {
            format!("-{}", result)
        } else {
            result
        }
    }

    /// Get total nanoseconds
    pub fn total_nanos(&self) -> i64 {
        self.nanoseconds
    }

    /// Get total milliseconds
    pub fn total_millis(&self) -> f64 {
        self.nanoseconds as f64 / 1_000_000.0
    }

    /// Get total seconds
    pub fn total_seconds(&self) -> f64 {
        self.nanoseconds as f64 / 1_000_000_000.0
    }

    /// Get total minutes
    pub fn total_minutes(&self) -> f64 {
        self.nanoseconds as f64 / (60.0 * 1_000_000_000.0)
    }

    /// Get total hours
    pub fn total_hours(&self) -> f64 {
        self.nanoseconds as f64 / (3600.0 * 1_000_000_000.0)
    }

    /// Get total days
    pub fn total_days(&self) -> f64 {
        self.nanoseconds as f64 / (86400.0 * 1_000_000_000.0)
    }

    /// Add two durations
    pub fn add(&self, other: &Duration) -> Duration {
        Duration {
            nanoseconds: self.nanoseconds + other.nanoseconds,
        }
    }

    /// Subtract two durations
    pub fn sub(&self, other: &Duration) -> Duration {
        Duration {
            nanoseconds: self.nanoseconds - other.nanoseconds,
        }
    }

    /// Multiply duration by scalar
    pub fn mul(&self, scalar: f64) -> Duration {
        Duration {
            nanoseconds: (self.nanoseconds as f64 * scalar) as i64,
        }
    }

    /// Divide duration by scalar
    pub fn div(&self, scalar: f64) -> Duration {
        Duration {
            nanoseconds: (self.nanoseconds as f64 / scalar) as i64,
        }
    }

    /// Negate duration
    pub fn neg(&self) -> Duration {
        Duration {
            nanoseconds: -self.nanoseconds,
        }
    }

    /// Absolute value of duration
    pub fn abs(&self) -> Duration {
        Duration {
            nanoseconds: self.nanoseconds.abs(),
        }
    }

    /// Check if duration is zero
    pub fn is_zero(&self) -> bool {
        self.nanoseconds == 0
    }

    /// Check if duration is negative
    pub fn is_negative(&self) -> bool {
        self.nanoseconds < 0
    }
}

impl std::fmt::Display for Duration {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.to_iso8601())
    }
}

impl FromStr for Duration {
    type Err = Error;

    fn from_str(s: &str) -> Result<Self> {
        Duration::from_iso8601(s)
    }
}

/// Legacy Date type with timezone offset support (DEPRECATED: use Instant instead)
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Date {
    /// UTC timestamp
    pub utc: DateTime<Utc>,
    /// Timezone offset in minutes (e.g., -480 for PST)
    pub tz_offset: Option<i16>,
}

impl Date {
    /// Create a new Date from a DateTime<Utc>
    pub fn from_utc(dt: DateTime<Utc>) -> Self {
        Date {
            utc: dt,
            tz_offset: None,
        }
    }

    /// Create a new Date from a DateTime with timezone
    pub fn from_datetime<Tz: TimeZone>(dt: DateTime<Tz>) -> Self {
        let offset_seconds = dt.offset().fix().local_minus_utc();
        let offset_minutes = offset_seconds / 60;
        Date {
            utc: dt.with_timezone(&Utc),
            tz_offset: Some(offset_minutes as i16),
        }
    }

    /// Parse from ISO 8601 string
    pub fn from_iso8601(s: &str) -> Result<Self> {
        // Try parsing with timezone
        if let Ok(dt) = DateTime::parse_from_rfc3339(s) {
            // If it's UTC (Z suffix), store without offset
            if s.ends_with('Z') {
                Ok(Self::from_utc(dt.with_timezone(&Utc)))
            } else {
                Ok(Self::from_datetime(dt))
            }
        } else if let Ok(dt) = s.parse::<DateTime<Utc>>() {
            Ok(Self::from_utc(dt))
        } else {
            Err(Error::InvalidDate(s.to_string()))
        }
    }

    /// Convert to ISO 8601 string
    pub fn to_iso8601(&self) -> String {
        if let Some(offset_minutes) = self.tz_offset {
            let offset_seconds = offset_minutes as i32 * 60;
            let offset = FixedOffset::east_opt(offset_seconds).unwrap();
            let dt = self.utc.with_timezone(&offset);
            dt.to_rfc3339()
        } else {
            // Format as "Z" instead of "+00:00"
            self.utc.format("%Y-%m-%dT%H:%M:%SZ").to_string()
        }
    }
}

impl fmt::Display for Date {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{}", self.to_iso8601())
    }
}

impl FromStr for Date {
    type Err = Error;

    fn from_str(s: &str) -> Result<Self> {
        Date::from_iso8601(s)
    }
}

// UUID generation functions

/// Generate a UUID v4 (random)
pub fn uuid_v4() -> uuid::Uuid {
    uuid::Uuid::new_v4()
}

/// Generate a UUID v7 (timestamp-based)
pub fn uuid_v7() -> uuid::Uuid {
    use std::time::{SystemTime, UNIX_EPOCH};

    let now = SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .unwrap()
        .as_millis() as u64;

    let mut bytes = [0u8; 16];

    // Timestamp (48 bits)
    bytes[0] = ((now >> 40) & 0xff) as u8;
    bytes[1] = ((now >> 32) & 0xff) as u8;
    bytes[2] = ((now >> 24) & 0xff) as u8;
    bytes[3] = ((now >> 16) & 0xff) as u8;
    bytes[4] = ((now >> 8) & 0xff) as u8;
    bytes[5] = (now & 0xff) as u8;

    // Random bits for the rest
    use rand::Rng;
    let mut rng = rand::thread_rng();
    rng.fill(&mut bytes[6..]);

    // Set version (7) and variant bits
    bytes[6] = (bytes[6] & 0x0f) | 0x70; // Version 7
    bytes[8] = (bytes[8] & 0x3f) | 0x80; // Variant 10

    uuid::Uuid::from_bytes(bytes)
}

// Add rand dependency for uuid_v7
use rand;

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_bigint() {
        let bi = BigInt::from_i64(123456789012345678);
        assert_eq!(bi.to_string(), "123456789012345678");
        assert_eq!(bi.to_kjson_string(), "123456789012345678n");

        let parsed = BigInt::from_str("123456789012345678n").unwrap();
        assert_eq!(parsed.to_string(), "123456789012345678");
    }

    #[test]
    fn test_decimal128() {
        let d = Decimal128::from_str("99.99").unwrap();
        assert_eq!(d.to_string(), "99.99");
        assert_eq!(d.to_kjson_string(), "99.99m");

        let d2 = Decimal128::from_str("99.99m").unwrap();
        assert_eq!(d2.to_string(), "99.99");
    }

    #[test]
    fn test_date() {
        let dt = Utc::now();
        let date = Date::from_utc(dt);
        let iso = date.to_iso8601();
        let parsed = Date::from_iso8601(&iso).unwrap();
        assert_eq!(date.utc.timestamp(), parsed.utc.timestamp());
    }

    #[test]
    fn test_uuid_generation() {
        let u4 = uuid_v4();
        let u7 = uuid_v7();
        assert_ne!(u4, u7);
        assert_eq!(u4.get_version_num(), 4);
        assert_eq!(u7.get_version_num(), 7);
    }
}