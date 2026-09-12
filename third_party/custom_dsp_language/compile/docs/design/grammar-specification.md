# LH Language Grammar Specification

## 📋 Document Information

- **Version**: 1.0 (Minimal Viable Version)
- **Date**: 2026-01-22
- **Status**: Initial Implementation
- **Based on**: IEC 61131-3, CODESYS Standards

---

## 🎯 Design Goals

1. **Easy to Read**: Clear, modern syntax
2. **Industrial Standard**: Based on CODESYS/IEC 61131-3
3. **Type Safe**: Strong typing with validation
4. **Compatible**: Compiles to original LH bytecode

---

## 📝 Grammar Overview

### Program Structure

Every LH program follows this structure:

```
PROGRAM <name>
    [VAR sections]
    [statements]
END_PROGRAM
```

### Minimal Example

```pascal
PROGRAM Main
VAR
    system : _System;
END_VAR

system(Author := 1, Config := 100, Date := 2601);

END_PROGRAM
```

---

## 🔤 Lexical Elements

### Keywords (Reserved Words)

**Program Structure:**
- `PROGRAM`, `END_PROGRAM`
- `VAR`, `END_VAR`

**Control Flow:**
- `IF`, `THEN`, `ELSE`, `ELSIF`, `END_IF`
- `WHILE`, `DO`, `END_WHILE`
- `FOR`, `TO`, `BY`, `END_FOR`
- `RETURN`, `EXIT`

**Data Types:**
- `BOOL`, `INT`, `DINT`, `REAL`, `STRING`, `TIME`

**Operators:**
- Logical: `AND`, `OR`, `NOT`
- Arithmetic: `+`, `-`, `*`, `/`, `MOD`
- Comparison: `=`, `<>`, `<`, `>`, `<=`, `>=`

### Identifiers

**Rules:**
- Start with letter or underscore
- Contain letters, digits, underscores
- Case-sensitive

**Examples:**
```pascal
system      // valid
myVariable  // valid
_System     // valid (function block)
temp_1      // valid
123abc      // INVALID (starts with digit)
my-var      // INVALID (contains hyphen)
```

### Literals

**Integer Literals:**
```pascal
0
42
1000
```

**Real Literals:**
```pascal
0.0
3.14
2.5e10
1.5E-3
```

**Boolean Literals:**
```pascal
TRUE
FALSE
```

**String Literals:**
```pascal
'Hello World'
"Double quotes also work"
'It''s a string'  // Escaped quote
```

**Time Literals:**
```pascal
T#5s        // 5 seconds
TIME#100ms  // 100 milliseconds
T#1h        // 1 hour
```

### Comments

**Single-line:**
```pascal
// This is a comment
```

**Multi-line:**
```pascal
(*
  This is a
  multi-line comment
*)
```

---

## 📐 Syntax Rules

### 1. Program Declaration

**Syntax:**
```
PROGRAM <identifier>
    <var-sections>
    <statements>
END_PROGRAM
```

**Example:**
```pascal
PROGRAM MotorControl
VAR
    speed : REAL;
END_VAR

speed := 100.0;

END_PROGRAM
```

### 2. Variable Declaration

**Syntax:**
```
VAR
    <identifier> : <type> ;
    <identifier> : <type> ;
    ...
END_VAR
```

**Examples:**
```pascal
VAR
    temperature : REAL;
    counter : INT;
    is_running : BOOL;
    system : _System;
END_VAR
```

**Supported Types:**
- `BOOL` - Boolean (TRUE/FALSE)
- `INT` - 16-bit signed integer
- `DINT` - 32-bit signed integer
- `REAL` - 32-bit floating point
- `STRING` - String type
- `_FunctionBlockName` - Function block types

### 3. Assignment Statement

**Syntax:**
```
<identifier> := <expression> ;
```

**Examples:**
```pascal
temperature := 25.5;
counter := counter + 1;
is_running := TRUE;
result := (a + b) * 2.0;
```

### 4. Function Block Call

**Syntax:**
```
<identifier> ( <parameter-list> ) ;
```

**Parameter List:**
```
<param-name> := <value> , <param-name> := <value> , ...
```

**Examples:**
```pascal
// Simple call
system(Author := 1, Config := 100, Date := 2601);

// With variable references
pid_controller(
    SetPoint := target_temp,
    ProcessValue := current_temp,
    Kp := 1.5,
    Ki := 0.1,
    Kd := 0.05
);

// No parameters
timer_start();
```

### 5. Expressions

**Arithmetic:**
```pascal
result := a + b;
result := (x * 2.0) - y;
area := width * height;
```

**Comparison:**
```pascal
is_hot := temperature > 50.0;
is_equal := value1 = value2;
is_different := status <> 0;
```

**Logical:**
```pascal
condition := (temp > 20.0) AND (pressure < 100.0);
alarm := error1 OR error2;
enabled := NOT disabled;
```

**Operator Precedence (highest to lowest):**
1. `( )` - Parentheses
2. `NOT` - Logical NOT
3. `*`, `/`, `MOD` - Multiplication, Division
4. `+`, `-` - Addition, Subtraction
5. `=`, `<>`, `<`, `>`, `<=`, `>=` - Comparison
6. `AND` - Logical AND
7. `OR` - Logical OR

---

## 🚧 Future Extensions (Not in v1.0)

### Control Flow (To be added)

**IF Statement:**
```pascal
IF temperature > 50.0 THEN
    heater_power := 0.0;
ELSIF temperature < 20.0 THEN
    heater_power := 100.0;
ELSE
    heater_power := 50.0;
END_IF;
```

**WHILE Loop:**
```pascal
WHILE counter < 10 DO
    counter := counter + 1;
END_WHILE;
```

**FOR Loop:**
```pascal
FOR i := 1 TO 10 BY 1 DO
    sum := sum + i;
END_FOR;
```

---

## 📊 Complete Grammar (EBNF Notation)

```ebnf
program = "PROGRAM" identifier
          var-section*
          statement*
          "END_PROGRAM" ;

var-section = "VAR"
              variable-declaration+
              "END_VAR" ;

variable-declaration = identifier ":" data-type ";" ;

data-type = "BOOL" | "INT" | "DINT" | "REAL" | "STRING"
          | function-block-type ;

function-block-type = "_" identifier ;

statement = assignment-statement
          | function-block-call
          | ";" ;

assignment-statement = identifier ":=" expression ";" ;

function-block-call = identifier "(" parameter-list? ")" ";" ;

parameter-list = parameter ("," parameter)* ;

parameter = identifier ":=" expression ;

expression = literal
           | identifier
           | expression ("*" | "/") expression
           | expression ("+" | "-") expression
           | expression ("=" | "<>" | "<" | ">" | "<=" | ">=") expression
           | expression ("AND" | "OR") expression
           | "NOT" expression
           | "(" expression ")" ;

literal = integer-literal
        | real-literal
        | bool-literal
        | string-literal ;
```

---

## 🎯 Design Decisions

### Why CODESYS-style?

**Pros:**
- ✅ Familiar to industrial programmers
- ✅ Clear structure with BEGIN/END blocks
- ✅ Strong typing
- ✅ Standard-based (IEC 61131-3)

**Comparison:**

| Feature | Original LH | CODESYS Style |
|---------|-------------|---------------|
| Variables | No declaration | `VAR ... END_VAR` |
| Function calls | `_Func[Name] params;` | `name(param := value);` |
| Readability | ⚠️ Medium | ✅ High |
| Type safety | ⚠️ Implicit | ✅ Explicit |

### Type System

**Explicit typing:**
```pascal
VAR
    counter : INT;      // Must declare type
    temp : REAL;        // Type is clear
END_VAR
```

**Type checking at compile time:**
- Integer and Real are distinct
- No implicit conversions
- Function block types validated

### Parameter Passing

**Named parameters (clearer):**
```pascal
system(
    Author := 1,      // Clear what each parameter is
    Config := 100,
    Date := 2601
);
```

**vs Original (positional, unclear):**
```pascal
_System[Sys] 1, 100, 2601;  // What do these numbers mean?
```

---

## 🔍 Examples

### Example 1: Hello World (Minimal)

```pascal
PROGRAM Main
VAR
    system : _System;
END_VAR

system(Author := 1, Config := 100, Date := 2601);

END_PROGRAM
```

### Example 2: Simple I/O

```pascal
PROGRAM IOTest
VAR
    system : _System;
    temperature : REAL;
    heater_power : REAL;
END_VAR

// Initialize system
system(Author := 1, Config := 100, Date := 2601);

// Read temperature (will be implemented later)
// temperature := ADC_Read(Channel := 1);

// Simple control logic
temperature := 25.5;
heater_power := 50.0;

END_PROGRAM
```

### Example 3: Multiple Variables

```pascal
PROGRAM MultiVar
VAR
    system : _System;
    counter : INT;
    is_running : BOOL;
    speed : REAL;
    status : INT;
END_VAR

system(Author := 1, Config := 100, Date := 2601);

counter := 0;
is_running := TRUE;
speed := 100.0;
status := 1;

END_PROGRAM
```

---

## 📚 Reference

### ANTLR Grammar File

The complete implementation is in `grammar/LH.g4`

### Related Documents

- IEC 61131-3 Standard
- CODESYS Programming Manual
- LH Function Block Reference

---

## 🔄 Version History

### v1.0 (2026-01-22)
- Initial minimal viable grammar
- Support for PROGRAM structure
- Variable declarations
- Basic types (BOOL, INT, DINT, REAL, STRING)
- Function block calls
- Assignment statements
- Expressions (arithmetic, comparison, logical)
- Comments

### Planned for v1.1
- Control flow (IF, WHILE, FOR)
- Function definitions
- Arrays
- More operators

---

**Status**: ✅ Ready for implementation and testing

