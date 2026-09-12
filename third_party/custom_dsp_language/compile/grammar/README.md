# LH Grammar - ANTLR4 Implementation

## 📋 Overview

This directory contains the ANTLR4 grammar definition for the LH language (CODESYS-style).

## 📁 Files

- **LH.g4** - Main ANTLR grammar file
- **test_programs.lh** - Test programs for grammar validation
- **README.md** - This file

## 🚀 Quick Start

### Prerequisites

1. **Java Runtime Environment (JRE)**
   ```bash
   java -version  # Should be 8 or higher
   ```

2. **ANTLR4**
   ```bash
   # Download ANTLR4
   cd ~
   wget https://www.antlr.org/download/antlr-4.13.1-complete.jar
   
   # Or use package manager
   # Ubuntu/Debian:
   sudo apt-get install antlr4
   
   # macOS:
   brew install antlr
   ```

### Generate Parser

#### Option 1: Using the Script (Recommended)

```bash
# From project root
./scripts/generate_parser.sh
```

#### Option 2: Manual Generation

```bash
# Set ANTLR classpath (adjust path as needed)
export CLASSPATH=".:/usr/local/lib/antlr-4.13.1-complete.jar:$CLASSPATH"

# Generate Python3 parser
cd grammar
antlr4 -Dlanguage=Python3 -visitor -no-listener LH.g4

# Or using Java directly:
java -jar /path/to/antlr-4.13.1-complete.jar -Dlanguage=Python3 -visitor -no-listener LH.g4
```

**Generated Files:**
- `LHLexer.py` - Lexical analyzer
- `LHParser.py` - Syntax parser
- `LHVisitor.py` - Visitor pattern interface
- `LH.tokens` - Token definitions
- `LHLexer.tokens` - Lexer tokens

### Test the Grammar

#### Visual Parse Tree (GUI)

```bash
# Set classpath
export CLASSPATH=".:/usr/local/lib/antlr-4.13.1-complete.jar:$CLASSPATH"
alias antlr4='java -jar /usr/local/lib/antlr-4.13.1-complete.jar'
alias grun='java org.antlr.v4.gui.TestRig'

# Test with GUI
cd grammar
grun LH program -gui test_programs.lh
```

This will show a visual parse tree!

#### Command Line Test

```bash
# Test parsing
grun LH program -tree test_programs.lh
grun LH program -tokens test_programs.lh
```

### Integration with Python

After generation, the parser can be used in Python:

```python
from antlr4 import *
from grammar.LHLexer import LHLexer
from grammar.LHParser import LHParser

# Parse a program
def parse_lm_program(code):
    input_stream = InputStream(code)
    lexer = LHLexer(input_stream)
    token_stream = CommonTokenStream(lexer)
    parser = LHParser(token_stream)
    
    tree = parser.program()
    return tree

# Example
code = """
PROGRAM Main
VAR
    system : _System;
END_VAR

system(Author := 1, Config := 100, Date := 2601);

END_PROGRAM
"""

tree = parse_lm_program(code)
print(tree.toStringTree(recog=parser))
```

## 📝 Grammar Features (v1.0)

### Supported Constructs

- ✅ PROGRAM structure
- ✅ VAR sections
- ✅ Data types: BOOL, INT, DINT, REAL, STRING
- ✅ Function block types (\_FunctionName)
- ✅ Assignment statements
- ✅ Function block calls with named parameters
- ✅ Expressions (arithmetic, comparison, logical)
- ✅ Comments (// and (* *))

### Not Yet Supported (Future)

- ⏳ IF/THEN/ELSE statements
- ⏳ WHILE loops
- ⏳ FOR loops
- ⏳ FUNCTION definitions
- ⏳ Arrays
- ⏳ STRUCT types

## 🧪 Test Programs

The `test_programs.lh` file contains various test cases:

1. **MinimalTest** - Simplest valid program
2. **VariableTest** - Multiple variable declarations
3. **ArithmeticTest** - Arithmetic expressions
4. **ComparisonTest** - Comparison and logical operations
5. **FunctionBlockTest** - Function block calls
6. **CommentTest** - Comment handling
7. **EmptyStatementTest** - Empty statements
8. **ComplexExpressionTest** - Complex expressions

## 🐛 Debugging

### Common Issues

**Issue 1: "antlr4: command not found"**
```bash
# Solution: Add ANTLR to PATH or use full path
java -jar /path/to/antlr-4.13.1-complete.jar -Dlanguage=Python3 LH.g4
```

**Issue 2: "No such file or directory: LH.g4"**
```bash
# Solution: Make sure you're in the grammar directory
cd grammar
```

**Issue 3: Parse errors**
```bash
# Use -diagnostics flag for detailed error info
grun LH program -diagnostics test_programs.lh
```

### Verbose Mode

```bash
# Generate with verbose output
antlr4 -Dlanguage=Python3 -visitor -no-listener -Xlog LH.g4
```

## 📚 Resources

- [ANTLR4 Documentation](https://github.com/antlr/antlr4/blob/master/doc/index.md)
- [ANTLR4 Python Target](https://github.com/antlr/antlr4/blob/master/doc/python-target.md)
- [Grammar Patterns](https://github.com/antlr/grammars-v4)

## 🔄 Development Workflow

1. **Edit grammar** - Modify `LH.g4`
2. **Regenerate parser** - Run `antlr4 ...`
3. **Test** - Use `grun` or Python tests
4. **Iterate** - Repeat until satisfied

## 💡 Tips

### Quick Test Cycle

Create an alias for quick testing:

```bash
# Add to ~/.bashrc or ~/.zshrc
alias lmtest='antlr4 -Dlanguage=Python3 LH.g4 && grun LH program -gui'

# Usage:
cd grammar
lmtest test_programs.lh
```

### Syntax Highlighting

For VS Code, install:
- "ANTLR4 grammar syntax support" extension

### Grammar Visualization

```bash
# Install railroad diagram generator
npm install -g antlr4-railroad-diagrams

# Generate diagrams
antlr4-railroad-diagrams LH.g4
```

## 📊 Grammar Statistics

**Current version (v1.0):**
- Parser rules: 15
- Lexer rules: 40+
- Keywords: 20+
- Operators: 15+

## 🎯 Next Steps

After successful grammar generation:

1. ✅ Integrate parser into `src/lh_compiler/frontend/parser.py`
2. ✅ Build AST from parse tree
3. ✅ Implement semantic analysis
4. ✅ Connect to code generator

---

**Ready to parse LH programs!** 🚀

