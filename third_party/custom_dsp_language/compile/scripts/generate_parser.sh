#!/bin/bash
# ============================================================================
# ANTLR Parser Generation Script
# ============================================================================
# This script generates Python parser code from the ANTLR grammar
# ============================================================================

set -e  # Exit on error

echo "========================================="
echo "LH Compiler - Parser Generation"
echo "========================================="
echo ""

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Check if ANTLR is available
echo "Checking for ANTLR4..."

# Try different methods to find ANTLR
ANTLR_JAR=""

# Method 1: Check common locations
if [ -f "/usr/local/lib/antlr-4.13.2-complete.jar" ]; then
    ANTLR_JAR="/usr/local/lib/antlr-4.13.2-complete.jar"
elif [ -f "$HOME/antlr-4.13.2-complete.jar" ]; then
    ANTLR_JAR="$HOME/antlr-4.13.2-complete.jar"
elif [ -f "./antlr-4.13.2-complete.jar" ]; then
    ANTLR_JAR="./antlr-4.13.2-complete.jar"
fi

# Method 2: Check if antlr4 command exists
if command -v antlr4 &> /dev/null; then
    echo -e "${GREEN}✓ Found ANTLR4 command${NC}"
    USE_COMMAND=true
elif [ -n "$ANTLR_JAR" ]; then
    echo -e "${GREEN}✓ Found ANTLR JAR: $ANTLR_JAR${NC}"
    USE_COMMAND=false
else
    echo -e "${RED}✗ ANTLR4 not found!${NC}"
    echo ""
    echo "Please install ANTLR4:"
    echo ""
    echo "Option 1: Download JAR file"
    echo "  wget https://www.antlr.org/download/antlr-4.13.2-complete.jar"
    echo "  mv antlr-4.13.2-complete.jar /usr/local/lib/"
    echo ""
    echo "Option 2: Use package manager"
    echo "  # Ubuntu/Debian:"
    echo "  sudo apt-get install antlr4"
    echo ""
    echo "  # macOS:"
    echo "  brew install antlr"
    echo ""
    exit 1
fi

# Check for Java
echo "Checking for Java..."
if ! command -v java &> /dev/null; then
    echo -e "${RED}✗ Java not found!${NC}"
    echo "Please install Java Runtime Environment (JRE) 8 or higher"
    exit 1
fi

JAVA_VERSION=$(java -version 2>&1 | head -n 1 | cut -d'"' -f2)
echo -e "${GREEN}✓ Java version: $JAVA_VERSION${NC}"
echo ""

# Navigate to grammar directory
cd "$(dirname "$0")/.."
GRAMMAR_DIR="grammar"

if [ ! -d "$GRAMMAR_DIR" ]; then
    echo -e "${RED}✗ Grammar directory not found: $GRAMMAR_DIR${NC}"
    exit 1
fi

cd "$GRAMMAR_DIR"

# Check if grammar file exists
if [ ! -f "LH.g4" ]; then
    echo -e "${RED}✗ Grammar file not found: LH.g4${NC}"
    exit 1
fi

echo "Generating parser from LH.g4..."
echo ""

# Generate parser
if [ "$USE_COMMAND" = true ]; then
    # Use antlr4 command
    antlr4 -Dlanguage=Python3 -visitor -no-listener LH.g4
else
    # Use JAR file
    java -jar "$ANTLR_JAR" -Dlanguage=Python3 -visitor -no-listener LH.g4
fi

# Check if generation was successful
if [ $? -eq 0 ]; then
    echo ""
    echo -e "${GREEN}✓ Parser generated successfully!${NC}"
    echo ""
    echo "Generated files:"
    ls -lh LH*.py LH*.tokens 2>/dev/null | awk '{print "  " $9 " (" $5 ")"}'
    echo ""
else
    echo -e "${RED}✗ Parser generation failed!${NC}"
    exit 1
fi

# Create __init__.py if it doesn't exist
if [ ! -f "__init__.py" ]; then
    touch __init__.py
    echo -e "${GREEN}✓ Created __init__.py${NC}"
fi

# Test import (optional)
echo "Testing parser import..."
cd ..
python3 -c "
import sys
sys.path.insert(0, 'grammar')
try:
    from LHLexer import LHLexer
    from LHParser import LHParser
    print('${GREEN}✓ Parser imports successfully!${NC}')
except ImportError as e:
    print('${RED}✗ Import failed: ' + str(e) + '${NC}')
    sys.exit(1)
" 

if [ $? -eq 0 ]; then
    echo ""
    echo "========================================="
    echo -e "${GREEN}✓ Parser generation complete!${NC}"
    echo "========================================="
    echo ""
    echo "Next steps:"
    echo "  1. Test the parser:"
    echo "     cd grammar"
    echo "     python3 -c 'from LHParser import LHParser; print(\"OK\")'"
    echo ""
    echo "  2. View parse tree (if Java GUI available):"
    echo "     grun LH program -gui test_programs.lh"
    echo ""
    echo "  3. Integrate into compiler:"
    echo "     Edit src/lh_compiler/frontend/parser.py"
    echo ""
else
    echo -e "${RED}✗ Import test failed${NC}"
    exit 1
fi

