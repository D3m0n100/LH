# -*- coding: utf-8 -*-
"""
LM 编译器后端
负责内存分配、代码生成和 .code 文件输出
"""

from .memory import MemoryAllocator
from .codegen import CodeGenerator
from .emitter import CodeEmitter

__all__ = ['MemoryAllocator', 'CodeGenerator', 'CodeEmitter']
