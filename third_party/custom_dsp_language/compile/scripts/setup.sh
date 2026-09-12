#!/bin/bash
# Quick setup script for LH Compiler development

set -e

echo "========================================="
echo "LH Compiler - Quick Setup"
echo "========================================="
echo ""

# Check Python version
python_version=$(python3 --version 2>&1 | awk '{print $2}')
echo "✓ Python version: $python_version"

# Create virtual environment
if [ ! -d "venv" ]; then
    echo "Creating virtual environment..."
    python3 -m venv venv
    echo "✓ Virtual environment created"
else
    echo "✓ Virtual environment already exists"
fi

# Activate virtual environment
echo "Activating virtual environment..."
source venv/bin/activate || . venv/Scripts/activate

# Install dependencies
echo "Installing dependencies..."
pip install --upgrade pip
pip install -r requirements.txt
echo "✓ Dependencies installed"

# Install package in development mode
echo "Installing lh-compiler in development mode..."
pip install -e .
echo "✓ Package installed"

# Test installation
echo ""
echo "Testing installation..."
lmc --version
echo ""

echo "========================================="
echo "✓ Setup complete!"
echo "========================================="
echo ""
echo "Next steps:"
echo "  1. Activate the virtual environment:"
echo "     source venv/bin/activate"
echo ""
echo "  2. List available function blocks:"
echo "     lmc list-blocks"
echo ""
echo "  3. Describe a function block:"
echo "     lmc describe _System"
echo ""
echo "  4. Run tests:"
echo "     pytest"
echo ""
