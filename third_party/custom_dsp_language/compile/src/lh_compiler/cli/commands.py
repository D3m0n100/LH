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

from ..function_blocks.registry import FunctionBlockRegistry
from ..utils.logger import setup_logger
from ..compiler import LHCompiler


console = Console()


def _get_registry():
    registry = FunctionBlockRegistry()
    registry.load_defaults()
    return registry


@click.group()
@click.version_option(version="1.0.0")
@click.option('-v', '--verbose', count=True, help='Increase verbosity')
def main(verbose):
    """LH Compiler - A CODESYS-style compiler for LH assembly language"""
    setup_logger(verbose)


@main.command()
@click.argument('input_file', type=click.Path(exists=True))
@click.option('-o', '--output', type=click.Path(), help='Output file path')
def compile(input_file, output):
    """Compile an LH source file"""
    console.print(f"[bold blue]Compiling:[/bold blue] {input_file}")
    compiler = LHCompiler()
    result = compiler.compile_file(input_file, output)
    if not result.success:
        err_msg = "\n".join(result.errors) if result.errors else "Compilation failed"
        raise click.ClickException(f"Compile failed:\n{err_msg}")
    console.print(f"[bold green]Compile success:[/bold green] {output or input_file}")


@main.command(name='list-blocks')
@click.option('--category', help='Filter by category')
@click.option('--search', help='Search in names and descriptions')
def list_blocks(category, search):
    """List available function blocks"""
    registry = _get_registry()

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
    table.add_column("Status", style="bold")
    table.add_column("Description", style="yellow")

    for block in sorted(blocks, key=lambda b: b.name):
        desc = block.description or ""
        status_text = "[green]supported[/green]" if block.is_supported else f"[red]{block.status}[/red]"
        table.add_row(
            block.name,
            str(block.type_id),
            block.category or "",
            status_text,
            desc[:50] + "..." if len(desc) > 50 else desc
        )

    console.print(table)
    console.print(f"\n[bold]Total:[/bold] {len(blocks)} function blocks")


@main.command()
@click.argument('block_name')
def describe(block_name):
    """Show detailed information about a function block"""
    registry = _get_registry()
    block = registry.get(block_name)

    if not block:
        raise click.ClickException(f"Function block '{block_name}' not found")

    # Print block information
    rprint(f"\n[bold cyan]{block.name}[/bold cyan] (Type ID: {block.type_id})")
    rprint(f"[dim]Category: {block.category}[/dim]")
    status_text = "[green]supported[/green]" if block.is_supported else f"[red]{block.status}[/red] ({block.incomplete_reason})"
    rprint(f"Status: {status_text}\n")
    rprint(f"[yellow]{block.description}[/yellow]\n")

    # Parameters table
    if block.parameters:
        param_table = Table(title="Parameters")
        param_table.add_column("Name", style="cyan")
        param_table.add_column("Direction", style="magenta")
        param_table.add_column("Data Type", style="green")
        param_table.add_column("Default Value", style="yellow")
        param_table.add_column("Description")

        for param in block.parameters:
            default_str = str(param.default_value) if param.default_value is not None else "-"
            param_table.add_row(
                param.name,
                param.direction or "IN",
                str(param.data_type),
                default_str,
                param.description or ""
            )

        console.print(param_table)
    rprint()


@main.command()
@click.argument('input_file', type=click.Path(exists=True))
def check(input_file):
    """Check syntax without compiling"""
    console.print(f"[bold blue]Checking:[/bold blue] {input_file}")
    compiler = LHCompiler()
    with open(input_file, 'r', encoding='utf-8', errors='replace') as f:
        src = f.read()
    result = compiler.compile_string(src)
    if not result.success:
        err_msg = "\n".join(result.errors) if result.errors else "Check failed"
        raise click.ClickException(f"Check failed:\n{err_msg}")
    console.print(f"[bold green]Check passed:[/bold green] {input_file}")


@main.command()
def repl():
    """Start interactive REPL"""
    raise click.ClickException("REPL not supported")


@main.command()
def categories():
    """List all function block categories"""
    registry = _get_registry()
    cats = registry.list_categories()

    table = Table(title="Function Block Categories")
    table.add_column("Category", style="cyan")
    table.add_column("Count", style="magenta")

    for cat in sorted(cats):
        blocks = registry.get_category(cat)
        table.add_row(cat, str(len(blocks)))

    console.print(table)


cli = main

if __name__ == '__main__':
    main()
