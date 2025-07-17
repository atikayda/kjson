# Code Comment Standards

## Avoid Unhelpful Comments

Based on recent code cleanup, here are key principles for effective commenting:

### ❌ Comments to Avoid

1. **Obvious Type Declarations**
   ```typescript
   // BAD: States what TypeScript already shows
   // name is a string
   name: string;
   ```

2. **Restating Code Logic**
   ```typescript
   // BAD: Just describes what the code does
   // Check if user exists
   if (user) { ... }
   ```

3. **Redundant Function Descriptions**
   ```typescript
   // BAD: Adds no value beyond the function name
   // Gets the user by ID
   getUserById(id: string) { ... }
   ```

### ✅ Comments to Include

1. **Complex Business Logic**
   ```typescript
   // Apply 15% discount for orders over $100 during weekends
   // as per Q4 2024 promotional campaign rules
   ```

2. **Non-Obvious Implementation Decisions**
   ```typescript
   // Using batch size of 50 to stay within RPC rate limits
   // while maintaining reasonable throughput
   ```

3. **Important Warnings or Edge Cases**
   ```typescript
   // IMPORTANT: This must run before token validation
   // to prevent race conditions in high-volume scenarios
   ```

4. **API Quirks or External Dependencies**
   ```typescript
   // BitQuery returns null for tokens created before block 1000000
   // so we need to handle this edge case explicitly
   ```

## Key Principle

**Comments should explain WHY, not WHAT**. If the code clearly shows what it does, focus on explaining the reasoning, context, or non-obvious implications instead.

## Examples from Recent Cleanup

- Removed 30+ comments that just restated TypeScript types
- Eliminated redundant function descriptions that added no context
- Kept comments explaining rate limits, error handling strategies, and architectural decisions
- Preserved warnings about security considerations and performance optimizations

Remember: Good code is self-documenting through clear naming and structure. Comments should fill in the gaps that code alone cannot express.