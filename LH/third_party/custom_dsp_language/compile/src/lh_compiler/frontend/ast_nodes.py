# -*- coding: utf-8 -*-
"""
AST Node Definitions for LH Compiler
定义所有抽象语法树节点类型
兼容 Python 3.13+
"""

from dataclasses import dataclass, field
from typing import List, Optional, Any
from enum import Enum


class DataType(Enum):
    """数据类型枚举"""
    BOOL = "BOOL"
    INT = "INT"
    UINT = "UINT"
    SINT = "SINT"
    USINT = "USINT"
    DINT = "DINT"
    UDINT = "UDINT"
    WORD = "WORD"
    DWORD = "DWORD"
    BYTE = "BYTE"
    REAL = "REAL"
    LREAL = "LREAL"
    STRING = "STRING"
    TIME = "TIME"
    DT = "DT"
    DATE = "DATE"
    TOD = "TOD"
    ARRAY = "ARRAY"
    POINTER = "POINTER"
    FUNCTION_BLOCK = "FUNCTION_BLOCK"
    UNKNOWN = "UNKNOWN"


class BinaryOperator(Enum):
    """二元运算符"""
    ADD = "+"
    SUB = "-"
    MUL = "*"
    DIV = "/"
    MOD = "MOD"
    EQ = "="
    NEQ = "<>"
    LT = "<"
    GT = ">"
    LE = "<="
    GE = ">="
    AND = "AND"
    OR = "OR"
    XOR = "XOR"


class UnaryOperator(Enum):
    """一元运算符"""
    NOT = "NOT"
    NEG = "-"
    POS = "+"


@dataclass
class ASTNode:
    """AST 节点基类"""
    line: int = 0
    column: int = 0

    def accept(self, visitor):
        method_name = f'visit_{self.__class__.__name__}'
        visitor_method = getattr(visitor, method_name, None)
        if visitor_method:
            return visitor_method(self)
        return visitor.generic_visit(self)


@dataclass
class Expression:
    """表达式基类"""
    line: int = 0
    column: int = 0

    def accept(self, visitor):
        method_name = f'visit_{self.__class__.__name__}'
        visitor_method = getattr(visitor, method_name, None)
        if visitor_method:
            return visitor_method(self)
        return visitor.generic_visit(self)


@dataclass
class Statement:
    """语句基类"""
    line: int = 0
    column: int = 0

    def accept(self, visitor):
        method_name = f'visit_{self.__class__.__name__}'
        visitor_method = getattr(visitor, method_name, None)
        if visitor_method:
            return visitor_method(self)
        return visitor.generic_visit(self)


@dataclass
class Program:
    name: str
    variables: List['Variable'] = field(default_factory=list)
    statements: List[Any] = field(default_factory=list)
    line: int = 0
    column: int = 0

    def accept(self, visitor):
        m = getattr(visitor, 'visit_Program', None)
        return m(self) if m else visitor.generic_visit(self)


@dataclass
class Variable:
    name: str
    data_type: str
    initial_value: Optional[Any] = None
    line: int = 0
    column: int = 0

    def accept(self, visitor):
        m = getattr(visitor, 'visit_Variable', None)
        return m(self) if m else visitor.generic_visit(self)


@dataclass
class Assignment:
    target: Any
    value: Any
    line: int = 0
    column: int = 0

    def accept(self, visitor):
        m = getattr(visitor, 'visit_Assignment', None)
        return m(self) if m else visitor.generic_visit(self)


@dataclass
class FunctionBlockCall:
    instance_name: str
    parameters: List['Parameter'] = field(default_factory=list)
    line: int = 0
    column: int = 0

    def accept(self, visitor):
        m = getattr(visitor, 'visit_FunctionBlockCall', None)
        return m(self) if m else visitor.generic_visit(self)


FunctionCallStatement = FunctionBlockCall


@dataclass
class Parameter:
    name: str
    value: Any
    is_output: bool = False
    line: int = 0
    column: int = 0

    def accept(self, visitor):
        m = getattr(visitor, 'visit_Parameter', None)
        return m(self) if m else visitor.generic_visit(self)


@dataclass
class BinaryOp:
    operator: BinaryOperator
    left: Any
    right: Any
    line: int = 0
    column: int = 0

    def accept(self, visitor):
        m = getattr(visitor, 'visit_BinaryOp', None)
        return m(self) if m else visitor.generic_visit(self)


@dataclass
class UnaryOp:
    operator: UnaryOperator
    operand: Any
    line: int = 0
    column: int = 0

    def accept(self, visitor):
        m = getattr(visitor, 'visit_UnaryOp', None)
        return m(self) if m else visitor.generic_visit(self)


@dataclass
class Literal:
    value: Any
    data_type: DataType
    line: int = 0
    column: int = 0

    def accept(self, visitor):
        m = getattr(visitor, 'visit_Literal', None)
        return m(self) if m else visitor.generic_visit(self)


@dataclass
class Identifier:
    name: str
    line: int = 0
    column: int = 0

    def accept(self, visitor):
        m = getattr(visitor, 'visit_Identifier', None)
        return m(self) if m else visitor.generic_visit(self)


@dataclass
class MemberAccess:
    base: Any
    member: str
    line: int = 0
    column: int = 0

    def accept(self, visitor):
        m = getattr(visitor, 'visit_MemberAccess', None)
        return m(self) if m else visitor.generic_visit(self)


@dataclass
class ArrayIndex:
    base: Any
    index: Any
    line: int = 0
    column: int = 0

    def accept(self, visitor):
        m = getattr(visitor, 'visit_ArrayIndex', None)
        return m(self) if m else visitor.generic_visit(self)


@dataclass
class FuncCallExpr:
    name: str
    parameters: List['Parameter'] = field(default_factory=list)
    line: int = 0
    column: int = 0

    def accept(self, visitor):
        m = getattr(visitor, 'visit_FuncCallExpr', None)
        return m(self) if m else visitor.generic_visit(self)


@dataclass
class IfStatement:
    condition: Any
    then_statements: List[Any] = field(default_factory=list)
    elseif_clauses: List[Any] = field(default_factory=list)
    else_statements: List[Any] = field(default_factory=list)
    line: int = 0
    column: int = 0

    def accept(self, visitor):
        m = getattr(visitor, 'visit_IfStatement', None)
        return m(self) if m else visitor.generic_visit(self)


@dataclass
class ElseIfClause:
    condition: Any
    statements: List[Any] = field(default_factory=list)
    line: int = 0
    column: int = 0

    def accept(self, visitor):
        m = getattr(visitor, 'visit_ElseIfClause', None)
        return m(self) if m else visitor.generic_visit(self)


@dataclass
class CaseStatement:
    expression: Any
    clauses: List['CaseClause'] = field(default_factory=list)
    else_statements: List[Any] = field(default_factory=list)
    line: int = 0
    column: int = 0

    def accept(self, visitor):
        m = getattr(visitor, 'visit_CaseStatement', None)
        return m(self) if m else visitor.generic_visit(self)


@dataclass
class CaseClause:
    labels: List[Any] = field(default_factory=list)
    statements: List[Any] = field(default_factory=list)
    line: int = 0
    column: int = 0

    def accept(self, visitor):
        m = getattr(visitor, 'visit_CaseClause', None)
        return m(self) if m else visitor.generic_visit(self)


@dataclass
class ForStatement:
    variable: str
    start: Any
    end: Any
    step: Optional[Any] = None
    body: List[Any] = field(default_factory=list)
    line: int = 0
    column: int = 0

    def accept(self, visitor):
        m = getattr(visitor, 'visit_ForStatement', None)
        return m(self) if m else visitor.generic_visit(self)


@dataclass
class WhileStatement:
    condition: Any
    body: List[Any] = field(default_factory=list)
    line: int = 0
    column: int = 0

    def accept(self, visitor):
        m = getattr(visitor, 'visit_WhileStatement', None)
        return m(self) if m else visitor.generic_visit(self)


@dataclass
class RepeatStatement:
    body: List[Any] = field(default_factory=list)
    condition: Any = None
    line: int = 0
    column: int = 0

    def accept(self, visitor):
        m = getattr(visitor, 'visit_RepeatStatement', None)
        return m(self) if m else visitor.generic_visit(self)


@dataclass
class ExitStatement:
    line: int = 0
    column: int = 0

    def accept(self, visitor):
        m = getattr(visitor, 'visit_ExitStatement', None)
        return m(self) if m else visitor.generic_visit(self)


@dataclass
class ReturnStatement:
    line: int = 0
    column: int = 0

    def accept(self, visitor):
        m = getattr(visitor, 'visit_ReturnStatement', None)
        return m(self) if m else visitor.generic_visit(self)


@dataclass
class ContinueStatement:
    line: int = 0
    column: int = 0

    def accept(self, visitor):
        m = getattr(visitor, 'visit_ContinueStatement', None)
        return m(self) if m else visitor.generic_visit(self)


class ASTVisitor:
    """AST 访问器基类"""

    def visit(self, node):
        if node is None:
            return None
        return node.accept(self)

    def generic_visit(self, node):
        pass

    def visit_Program(self, node):
        for var in node.variables:
            self.visit(var)
        for stmt in node.statements:
            self.visit(stmt)

    def visit_Variable(self, node):
        if node.initial_value:
            self.visit(node.initial_value)

    def visit_Assignment(self, node):
        self.visit(node.value)

    def visit_FunctionBlockCall(self, node):
        for param in node.parameters:
            self.visit(param)

    def visit_Parameter(self, node):
        if not node.is_output:
            self.visit(node.value)

    def visit_BinaryOp(self, node):
        self.visit(node.left)
        self.visit(node.right)

    def visit_UnaryOp(self, node):
        self.visit(node.operand)

    def visit_Literal(self, node):
        pass

    def visit_Identifier(self, node):
        pass

    def visit_MemberAccess(self, node):
        self.visit(node.base)

    def visit_ArrayIndex(self, node):
        self.visit(node.base)
        self.visit(node.index)

    def visit_FuncCallExpr(self, node):
        for param in node.parameters:
            self.visit(param)

    def visit_IfStatement(self, node):
        self.visit(node.condition)
        for stmt in node.then_statements:
            self.visit(stmt)
        for clause in node.elseif_clauses:
            self.visit(clause)
        for stmt in node.else_statements:
            self.visit(stmt)

    def visit_ElseIfClause(self, node):
        self.visit(node.condition)
        for stmt in node.statements:
            self.visit(stmt)

    def visit_CaseStatement(self, node):
        self.visit(node.expression)
        for clause in node.clauses:
            self.visit(clause)
        for stmt in node.else_statements:
            self.visit(stmt)

    def visit_CaseClause(self, node):
        for stmt in node.statements:
            self.visit(stmt)

    def visit_ForStatement(self, node):
        self.visit(node.start)
        self.visit(node.end)
        if node.step:
            self.visit(node.step)
        for stmt in node.body:
            self.visit(stmt)

    def visit_WhileStatement(self, node):
        self.visit(node.condition)
        for stmt in node.body:
            self.visit(stmt)

    def visit_RepeatStatement(self, node):
        for stmt in node.body:
            self.visit(stmt)
        self.visit(node.condition)

    def visit_ExitStatement(self, node):
        pass

    def visit_ReturnStatement(self, node):
        pass

    def visit_ContinueStatement(self, node):
        pass


class ASTPrinter(ASTVisitor):
    """打印 AST 树结构（调试用）"""

    def __init__(self):
        self.indent = 0

    def _print(self, text):
        print("  " * self.indent + text)

    def visit_Program(self, node):
        self._print(f"Program: {node.name}")
        self.indent += 1
        if node.variables:
            self._print("Variables:")
            self.indent += 1
            for var in node.variables:
                self.visit(var)
            self.indent -= 1
        if node.statements:
            self._print("Statements:")
            self.indent += 1
            for stmt in node.statements:
                self.visit(stmt)
            self.indent -= 1
        self.indent -= 1

    def visit_Variable(self, node):
        self._print(f"{node.name}: {node.data_type}")

    def visit_Assignment(self, node):
        self._print(f"Assignment: {node.target} :=")
        self.indent += 1
        self.visit(node.value)
        self.indent -= 1

    def visit_FunctionBlockCall(self, node):
        self._print(f"FunctionBlockCall: {node.instance_name}")
        self.indent += 1
        for param in node.parameters:
            self.visit(param)
        self.indent -= 1

    def visit_Parameter(self, node):
        arrow = "=>" if node.is_output else ":="
        self._print(f"Param {node.name} {arrow}")

    def visit_BinaryOp(self, node):
        self._print(f"BinaryOp: {node.operator.value}")
        self.indent += 1
        self.visit(node.left)
        self.visit(node.right)
        self.indent -= 1

    def visit_UnaryOp(self, node):
        self._print(f"UnaryOp: {node.operator.value}")
        self.indent += 1
        self.visit(node.operand)
        self.indent -= 1

    def visit_Literal(self, node):
        type_name = node.data_type.value if node.data_type else "UNKNOWN"
        self._print(f"Literal: {node.value} ({type_name})")

    def visit_Identifier(self, node):
        self._print(f"Identifier: {node.name}")

    def visit_IfStatement(self, node):
        self._print("IfStatement")

    def visit_CaseStatement(self, node):
        self._print("CaseStatement")

    def visit_ForStatement(self, node):
        self._print(f"ForStatement: {node.variable}")

    def visit_WhileStatement(self, node):
        self._print("WhileStatement")

    def visit_RepeatStatement(self, node):
        self._print("RepeatStatement")

    def visit_ExitStatement(self, node):
        self._print("EXIT")

    def visit_ReturnStatement(self, node):
        self._print("RETURN")

    def visit_ContinueStatement(self, node):
        self._print("CONTINUE")
