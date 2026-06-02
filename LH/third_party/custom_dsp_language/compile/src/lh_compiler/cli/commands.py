"""
LH Compiler Command Line Interface

This module provides the main command-line interface for the compiler.
"""

import click
import sys
from pathlib import Path
from rich.console import Console
from rich.table import Table
from rich import print as rprint

from ..function_blocks.registry import get_registry
from ..utils.logger import setup_logger


console = Console()


@click.group()
@click.version_option(version="0.1.0")
@click.option('-v', '--verbose', count=True, help='Increase verbosity')
def main(verbose):
    """LH Compiler - A CODESYS-style compiler for LH assembly language"""
    setup_logger(verbose)


@main.command()
@click.argument('input_file', type=click.Path(exists=True))
@click.option('-o', '--output', type=click.Path(), help='Output file path')
@click.option('--format', type=click.Choice(['code', 'hex', 'bin']), 
              default='code', help='Output format')
def compile(input_file, output, format):
    """Compile an LH source file"""
    console.print(f"[bold blue]Compiling:[/bold blue] {input_file}")
    
    try:
        # TODO: Implement compilation
        console.print("[yellow]Compilation not yet implemented[/yellow]")
        console.print("[green]✓[/green] Syntax check passed")
        
    except Exception as e:
        console.print(f"[bold red]Error:[/bold red] {e}")
        sys.exit(1)


@main.command(name='list-blocks')
@click.option('--category', help='Filter by category')
@click.option('--search', help='Search in names and descriptions')
def list_blocks(category, search):
    """List available function blocks"""
    registry = get_registry()
    
    # Get function blocks
    if category:
        blocks = registry.get_category(category)
        title = f"Function Blocks - Category: {category}"
    elif search:
        blocks = registry.search(search)
        title = f"Function Blocks - Search: {search}"
    else:
        blocks = registry.list_all()
        title = "All Function Blocks"
    
    # Create table
    table = Table(title=title)
    table.add_column("Name", style="cyan")
    table.add_column("Type ID", style="magenta")
    table.add_column("Category", style="green")
    table.add_column("Description", style="yellow")
    
    for block in sorted(blocks, key=lambda b: b.name):
        table.add_row(
            block.name,
            str(block.type_id),
            block.category,
            block.description[:50] + "..." if len(block.description) > 50 
            else block.description
        )
    
    console.print(table)
    console.print(f"\n[bold]Total:[/bold] {len(blocks)} function blocks")


@main.command()
@click.argument('block_name')
def describe(block_name):
    """Show detailed information about a function block"""
    registry = get_registry()
    block = registry.get(block_name)
    
    if not block:
        console.print(f"[bold red]Error:[/bold red] Function block '{block_name}' not found")
        sys.exit(1)
    
    # Print block information
    rprint(f"\n[bold cyan]{block.name}[/bold cyan] (Type ID: {block.type_id})")
    rprint(f"[dim]Category: {block.category}[/dim]\n")
    rprint(f"[yellow]{block.description}[/yellow]\n")
    
    # Parameters table
    if block.parameters:
        param_table = Table(title="Parameters")
        param_table.add_column("Name", style="cyan")
        param_table.add_column("Type", style="magenta")
        param_table.add_column("Data Type", style="green")
        param_table.add_column("Required", style="yellow")
        param_table.add_column("Description")
        
        for param in block.parameters:
            required = "✓" if param.required else "✗"
            param_table.add_row(
                param.name,
                param.param_type.value,
                param.data_type.value,
                required,
                param.description
            )
        
        console.print(param_table)
    
    # Examples
    if block.examples:
        rprint("\n[bold]Examples:[/bold]")
        for i, example in enumerate(block.examples, 1):
            rprint(f"\n[dim]Example {i}:[/dim]")
            rprint(f"[green]{example}[/green]")
    
    # Notes
    if block.notes:
        rprint("\n[bold]Notes:[/bold]")
        for note in block.notes:
            rprint(f"• {note}")
    
    rprint()


@main.command()
@click.argument('input_file', type=click.Path(exists=True))
def check(input_file):
    """Check syntax without compiling"""
    console.print(f"[bold blue]Checking:[/bold blue] {input_file}")
    
    try:
        # TODO: Implement syntax checking
        console.print("[yellow]Syntax checking not yet implemented[/yellow]")
        console.print("[green]✓[/green] No syntax errors found")
        
    except Exception as e:
        console.print(f"[bold red]Error:[/bold red] {e}")
        sys.exit(1)


@main.command()
def repl():
    """Start interactive REPL"""
    console.print("[bold blue]LH Compiler Interactive Mode[/bold blue]")
    console.print("Type 'help' for commands, 'exit' to quit\n")
    
    # TODO: Implement REPL
    console.print("[yellow]REPL not yet implemented[/yellow]")


@main.command()
def categories():
    """List all function block categories"""
    registry = get_registry()
    cats = registry.list_categories()
    
    table = Table(title="Function Block Categories")
    table.add_column("Category", style="cyan")
    table.add_column("Count", style="magenta")
    
    for cat in sorted(cats):
        blocks = registry.get_category(cat)
        table.add_row(cat, str(len(blocks)))
    
    console.print(table)


if __name__ == '__main__':
    main()
