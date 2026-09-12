# Getting Started with LH Compiler

## Quick Installation

```bash
# 1. Clone the repository
git clone https://github.com/yourusername/lh-compiler.git
cd lh-compiler

# 2. Run setup script
chmod +x scripts/setup.sh
./scripts/setup.sh

# 3. Verify installation
lmc --version
```

## Your First Program

Create a file `hello.lh`:

```pascal
PROGRAM Main
VAR
    system : _System;
END_VAR

// System initialization
system(
    Author := 1,
    Config := 100,
    Date := 2601
);

END_PROGRAM
```

Compile it:

```bash
lmc compile hello.lh -o hello.code
```

## Exploring Function Blocks

### List all available blocks
```bash
lmc list-blocks
```

### Filter by category
```bash
lmc list-blocks --category system
```

### Search for blocks
```bash
lmc list-blocks --search "temperature"
```

### Get detailed information
```bash
lmc describe _System
```

## Project Structure

When you create a new LH project, organize it like this:

```
my_project/
├── src/
│   ├── main.lh          # Main program
│   ├── config.lh        # Configuration
│   └── modules/         # Reusable modules
├── build/               # Compiled output
└── tests/               # Test programs
```

## Next Steps

- [Language Reference](language-reference.md) - Learn the syntax
- [Function Block Library](function-blocks.md) - Browse all 179 blocks
- [Examples](../examples/) - See complete programs
- [Developer Guide](../developer-guide/contributing.md) - Contribute to the project

## Common Commands

```bash
# Compile a program
lmc compile program.lh -o output.code

# Check syntax without compiling
lmc check program.lh

# Interactive mode
lmc repl

# List categories
lmc categories

# Show version
lmc --version

# Get help
lmc --help
```

## Troubleshooting

### Import errors

If you see import errors, make sure you've activated the virtual environment:

```bash
source venv/bin/activate  # Linux/Mac
# or
venv\Scripts\activate  # Windows
```

### Missing dependencies

Reinstall dependencies:

```bash
pip install -r requirements.txt
```

### Function blocks not found

Make sure the metadata files are present:

```bash
ls src/lh_compiler/function_blocks/metadata/
```

You should see `system.json` and other metadata files.

## Getting Help

- **Documentation**: Check the `docs/` directory
- **Issues**: https://github.com/yourusername/lh-compiler/issues
- **Examples**: See `examples/` directory
