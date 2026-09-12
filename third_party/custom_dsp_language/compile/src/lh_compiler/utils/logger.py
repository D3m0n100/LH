"""
Logging utilities for the LH compiler
"""

import logging
import sys
from typing import Optional


def setup_logger(verbosity: int = 0) -> logging.Logger:
    """
    Setup the root logger
    
    Args:
        verbosity: Verbosity level (0=WARNING, 1=INFO, 2=DEBUG)
    
    Returns:
        Configured logger instance
    """
    # Determine log level
    if verbosity == 0:
        level = logging.WARNING
    elif verbosity == 1:
        level = logging.INFO
    else:
        level = logging.DEBUG
    
    # Configure root logger
    logging.basicConfig(
        level=level,
        format='%(levelname)s: %(message)s' if verbosity < 2 
               else '%(asctime)s - %(name)s - %(levelname)s - %(message)s',
        handlers=[logging.StreamHandler(sys.stderr)]
    )
    
    return logging.getLogger('lh_compiler')


def get_logger(name: Optional[str] = None) -> logging.Logger:
    """
    Get a logger instance
    
    Args:
        name: Logger name (defaults to 'lh_compiler')
    
    Returns:
        Logger instance
    """
    return logging.getLogger(name or 'lh_compiler')
