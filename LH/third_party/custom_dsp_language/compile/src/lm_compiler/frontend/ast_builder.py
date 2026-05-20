# -*- coding: utf-8 -*-
"""
AST Builder for LM Compiler
完全匹配 LM.g4 语法的版本
修复版 v2.1：
  - 补充 visitCaseStatement / visitRepeatStatement / visitExitStatement 等
  - 补充 visitXorExpr / visitUnaryMinus / visitUnaryPlus
  - 补充 visitAtomExprRule / visitFuncCallExpr
  - visitParameter 支持 output (=>) 和 positional 参数
  - visitLiteral 支持 TIME_LITERAL
"""

import sys
import os

current_dir = os.path.dirname(os.path.abspath(__file__))
project_root = os.path.dirname(os.path.dirname(os.path.dirname(current_dir)))
grammar_path = os.path.join(project_root, 'grammar')
if grammar_path not in sys.path:
    sys.path.insert(0, grammar_path)

# 尝试导入 ANTLR 访问器基类
_LMVisitorBase = None
try:
    from grammar.LMVisitor import LMVisitor as _LMVisitorBase
except ImportError:
    pass

if _LMVisitorBase is None:
    try:
        from LMVisitor import LMVisitor as _LMVisitorBase
    except ImportError:
        pass


class _FallbackVisitor:
    """当 ANTLR LMVisitor 不可用时的备用基类"""

    def visit(self, tree):
        if tree is None:
            return None
        if hasattr(tree, 'accept'):
            return tree.accept(self)
        return None

    def visitChildren(self, ctx):
        if not hasattr(ctx, 'children') or not ctx.children:
            return None
        result = None
        for child in ctx.children:
            if hasattr(child, 'accept'):
                child_result = child.accept(self)
                result = self.aggregateResult(result, child_result)
        return result

    def aggregateResult(self, aggregate, nextResult):
        return nextResult

    def defaultResult(self):
        return None


# 选择基类
if _LMVisitorBase is not None:
    _BaseClass = _LMVisitorBase
else:
    _BaseClass = _FallbackVisitor


from .ast_nodes import (
    Program, Variable, Assignment, FunctionBlockCall, FunctionCallStatement,
    Parameter, BinaryOp, UnaryOp, Literal, Identifier,
    BinaryOperator, UnaryOperator, DataType,
    IfStatement, CaseStatement, CaseClause,
    ForStatement, WhileStatement, RepeatStatement,
    ExitStatement, ReturnStatement, ContinueStatement,
    MemberAccess, ArrayIndex, FuncCallExpr
)


class ASTBuilder(_BaseClass):
    """AST 构建器 - 完整匹配 LM.g4"""

    def __init__(self):
        self.errors = []

    # ========================================================================
    # 基础 visit
    # ========================================================================

    def visit(self, tree):
        if tree is None:
            return None
        if hasattr(tree, 'accept'):
            return tree.accept(self)
        return None

    # ========================================================================
    # 程序结构
    # ========================================================================

    def visitProgram(self, ctx):
        """program: PROGRAM identifier varSection* statementList END_PROGRAM EOF"""
        name = "Unknown"
        try:
            if ctx.identifier():
                name = ctx.identifier().getText()
        except Exception:
            pass

        variables = []
        try:
            var_sections = ctx.varSection()
            if var_sections:
                if not isinstance(var_sections, list):
                    var_sections = [var_sections]
                for vs in var_sections:
                    result = self.visitVarSection(vs)
                    if result:
                        variables.extend(result)
        except Exception as e:
            self.errors.append(f"解析变量声明出错: {e}")

        statements = []
        try:
            if ctx.statementList():
                result = self.visitStatementList(ctx.statementList())
                if result:
                    statements = result
        except Exception as e:
            self.errors.append(f"解析语句列表出错: {e}")

        return Program(
            name=name,
            variables=variables,
            statements=statements,
            line=ctx.start.line if hasattr(ctx, 'start') else 0,
            column=ctx.start.column if hasattr(ctx, 'start') else 0
        )

    def visitVarSection(self, ctx):
        """varSection: (VAR | VAR_CONSTANT) variableDeclaration+ END_VAR"""
        variables = []
        try:
            var_decls = ctx.variableDeclaration()
            if var_decls:
                if not isinstance(var_decls, list):
                    var_decls = [var_decls]
                for vd in var_decls:
                    var = self.visitVariableDeclaration(vd)
                    if var:
                        variables.append(var)
        except Exception as e:
            self.errors.append(f"解析变量段出错: {e}")
        return variables

    def visitVariableDeclaration(self, ctx):
        """variableDeclaration: identifier COLON dataType (ASSIGN expression)? SEMICOLON"""
        name = "unknown"
        try:
            if ctx.identifier():
                name = ctx.identifier().getText()
        except Exception:
            pass

        data_type = ""
        try:
            if ctx.dataType():
                data_type = ctx.dataType().getText()
        except Exception:
            pass

        initial_value = None
        try:
            if ctx.expression():
                initial_value = self.visit(ctx.expression())
        except Exception:
            pass

        return Variable(
            name=name,
            data_type=data_type,
            initial_value=initial_value,
            line=ctx.start.line if hasattr(ctx, 'start') else 0,
            column=ctx.start.column if hasattr(ctx, 'start') else 0
        )

    # ========================================================================
    # 语句列表
    # ========================================================================

    def visitStatementList(self, ctx):
        """statementList: statement*"""
        statements = []
        try:
            stmt_list = ctx.statement()
            if stmt_list:
                if not isinstance(stmt_list, list):
                    stmt_list = [stmt_list]
                for stmt_ctx in stmt_list:
                    stmt = self.visitStatement(stmt_ctx)
                    if stmt is not None:
                        statements.append(stmt)
        except Exception as e:
            self.errors.append(f"解析语句列表出错: {e}")
        return statements

    def visitStatement(self, ctx):
        """
        statement: assignmentStatement | functionCallStatement
                 | ifStatement | caseStatement
                 | forStatement | whileStatement | repeatStatement
                 | exitStatement | returnStatement | continueStatement
                 | emptyStatement
        """
        checks = [
            ('assignmentStatement', self.visitAssignmentStatement),
            ('functionCallStatement', self.visitFunctionCallStatement),
            ('ifStatement', self.visitIfStatement),
            ('caseStatement', self.visitCaseStatement),
            ('forStatement', self.visitForStatement),
            ('whileStatement', self.visitWhileStatement),
            ('repeatStatement', self.visitRepeatStatement),
            ('exitStatement', self.visitExitStatement),
            ('returnStatement', self.visitReturnStatement),
            ('continueStatement', self.visitContinueStatement),
        ]
        for attr, visitor_fn in checks:
            try:
                child = getattr(ctx, attr, None)
                if child and callable(child):
                    child_ctx = child()
                    if child_ctx:
                        return visitor_fn(child_ctx)
            except Exception:
                pass
        return None

    def visitEmptyStatement(self, ctx):
        return None

    # ========================================================================
    # 具体语句
    # ========================================================================

    def visitAssignmentStatement(self, ctx):
        """assignmentStatement: lvalue ASSIGN expression SEMICOLON"""
        target = "unknown"
        try:
            lv = ctx.lvalue()
            if lv:
                target = self._lvalue_text(lv)
        except Exception:
            pass

        value = None
        try:
            if ctx.expression():
                value = self.visit(ctx.expression())
        except Exception:
            pass

        return Assignment(
            target=target,
            value=value,
            line=ctx.start.line if hasattr(ctx, 'start') else 0,
            column=ctx.start.column if hasattr(ctx, 'start') else 0
        )

    def _lvalue_text(self, lv_ctx):
        """把 lvalue 上下文转成字符串（用于 Assignment.target）"""
        try:
            return lv_ctx.getText()
        except Exception:
            return "unknown"

    def visitFunctionCallStatement(self, ctx):
        """functionCallStatement: callExpr SEMICOLON"""
        instance_name = "unknown"
        parameters = []
        try:
            call = ctx.callExpr()
            if call:
                if call.identifier():
                    instance_name = call.identifier().getText()
                if call.parameterList():
                    parameters = self.visitParameterList(call.parameterList()) or []
        except Exception as e:
            self.errors.append(f"解析功能块调用出错: {e}")

        return FunctionBlockCall(
            instance_name=instance_name,
            parameters=parameters,
            line=ctx.start.line if hasattr(ctx, 'start') else 0,
            column=ctx.start.column if hasattr(ctx, 'start') else 0
        )

    # ---- IF ----

    def visitIfStatement(self, ctx):
        """
        ifStatement: IF expression THEN statementList
                     (ELSIF expression THEN statementList)*
                     (ELSE statementList)?
                     END_IF SEMICOLON?
        """
        condition = None
        then_stmts = []
        elsif_clauses = []
        else_stmts = []
        try:
            expressions = ctx.expression()
            stmt_lists  = ctx.statementList()
            if not isinstance(expressions, list):
                expressions = [expressions] if expressions else []
            if not isinstance(stmt_lists, list):
                stmt_lists = [stmt_lists] if stmt_lists else []

            if expressions:
                condition = self.visit(expressions[0])
            if stmt_lists:
                then_stmts = self.visitStatementList(stmt_lists[0]) or []

            for i in range(1, len(expressions)):
                elsif_cond = self.visit(expressions[i])
                elsif_body = self.visitStatementList(stmt_lists[i]) or [] if i < len(stmt_lists) else []
                elsif_clauses.append((elsif_cond, elsif_body))

            if len(stmt_lists) > len(expressions):
                else_stmts = self.visitStatementList(stmt_lists[-1]) or []
        except Exception as e:
            self.errors.append(f"解析IF语句出错: {e}")

        return IfStatement(
            condition=condition,
            then_statements=then_stmts,
            elsif_clauses=elsif_clauses,
            else_statements=else_stmts,
            line=ctx.start.line if hasattr(ctx, 'start') else 0,
            column=ctx.start.column if hasattr(ctx, 'start') else 0
        )

    # ---- CASE ----

    def visitCaseStatement(self, ctx):
        """
        caseStatement: CASE expression OF caseClause+ (ELSE statementList)? END_CASE SEMICOLON?
        """
        expr = None
        clauses = []
        else_stmts = []
        try:
            if ctx.expression():
                expr = self.visit(ctx.expression())
            cc_list = ctx.caseClause()
            if cc_list:
                if not isinstance(cc_list, list):
                    cc_list = [cc_list]
                for cc in cc_list:
                    clause = self.visitCaseClause(cc)
                    if clause:
                        clauses.append(clause)
            # ELSE 块：当 statementList 存在时
            sl_list = ctx.statementList()
            if sl_list:
                if not isinstance(sl_list, list):
                    sl_list = [sl_list]
                # 只取最后一个 statementList（即 ELSE 块）
                else_stmts = self.visitStatementList(sl_list[-1]) or []
        except Exception as e:
            self.errors.append(f"解析CASE语句出错: {e}")

        return CaseStatement(
            expression=expr,
            clauses=clauses,
            else_statements=else_stmts,
            line=ctx.start.line if hasattr(ctx, 'start') else 0,
            column=ctx.start.column if hasattr(ctx, 'start') else 0
        )

    def visitCaseClause(self, ctx):
        """caseClause: caseLabelList COLON statementList"""
        labels = []
        stmts = []
        try:
            if ctx.caseLabelList():
                lbl_list = ctx.caseLabelList()
                case_labels = lbl_list.caseLabel()
                if not isinstance(case_labels, list):
                    case_labels = [case_labels] if case_labels else []
                for cl in case_labels:
                    if cl.expression():
                        labels.append(self.visit(cl.expression()))
            if ctx.statementList():
                stmts = self.visitStatementList(ctx.statementList()) or []
        except Exception as e:
            self.errors.append(f"解析CASE子句出错: {e}")

        return CaseClause(
            labels=labels,
            statements=stmts,
            line=ctx.start.line if hasattr(ctx, 'start') else 0,
            column=ctx.start.column if hasattr(ctx, 'start') else 0
        )

    # ---- FOR ----

    def visitForStatement(self, ctx):
        """FOR identifier ASSIGN expression TO expression (BY expression)? DO statementList END_FOR"""
        var_name = "unknown"
        start = None
        end = None
        step = None
        body = []
        try:
            if ctx.identifier():
                var_name = ctx.identifier().getText()
            exprs = ctx.expression()
            if not isinstance(exprs, list):
                exprs = [exprs] if exprs else []
            if len(exprs) > 0:
                start = self.visit(exprs[0])
            if len(exprs) > 1:
                end = self.visit(exprs[1])
            if len(exprs) > 2:
                step = self.visit(exprs[2])
            if ctx.statementList():
                body = self.visitStatementList(ctx.statementList()) or []
        except Exception as e:
            self.errors.append(f"解析FOR语句出错: {e}")

        return ForStatement(
            variable=var_name,
            start=start,
            end=end,
            step=step,
            body=body,
            line=ctx.start.line if hasattr(ctx, 'start') else 0,
            column=ctx.start.column if hasattr(ctx, 'start') else 0
        )

    # ---- WHILE ----

    def visitWhileStatement(self, ctx):
        """WHILE expression DO statementList END_WHILE SEMICOLON?"""
        condition = None
        body = []
        try:
            if ctx.expression():
                condition = self.visit(ctx.expression())
            if ctx.statementList():
                body = self.visitStatementList(ctx.statementList()) or []
        except Exception as e:
            self.errors.append(f"解析WHILE语句出错: {e}")

        return WhileStatement(
            condition=condition,
            body=body,
            line=ctx.start.line if hasattr(ctx, 'start') else 0,
            column=ctx.start.column if hasattr(ctx, 'start') else 0
        )

    # ---- REPEAT...UNTIL ----

    def visitRepeatStatement(self, ctx):
        """REPEAT statementList UNTIL expression SEMICOLON? END_REPEAT SEMICOLON?"""
        body = []
        condition = None
        try:
            if ctx.statementList():
                body = self.visitStatementList(ctx.statementList()) or []
            if ctx.expression():
                condition = self.visit(ctx.expression())
        except Exception as e:
            self.errors.append(f"解析REPEAT语句出错: {e}")

        return RepeatStatement(
            body=body,
            condition=condition,
            line=ctx.start.line if hasattr(ctx, 'start') else 0,
            column=ctx.start.column if hasattr(ctx, 'start') else 0
        )

    # ---- 跳转语句 ----

    def visitExitStatement(self, ctx):
        return ExitStatement(
            line=ctx.start.line if hasattr(ctx, 'start') else 0,
            column=ctx.start.column if hasattr(ctx, 'start') else 0
        )

    def visitReturnStatement(self, ctx):
        return ReturnStatement(
            line=ctx.start.line if hasattr(ctx, 'start') else 0,
            column=ctx.start.column if hasattr(ctx, 'start') else 0
        )

    def visitContinueStatement(self, ctx):
        return ContinueStatement(
            line=ctx.start.line if hasattr(ctx, 'start') else 0,
            column=ctx.start.column if hasattr(ctx, 'start') else 0
        )

    # ========================================================================
    # 参数列表
    # ========================================================================

    def visitParameterList(self, ctx):
        """parameterList: parameter (COMMA parameter)*"""
        parameters = []
        try:
            params = ctx.parameter()
            if params:
                if not isinstance(params, list):
                    params = [params]
                for p in params:
                    param = self.visitParameter(p)
                    if param:
                        parameters.append(param)
        except Exception as e:
            self.errors.append(f"解析参数列表出错: {e}")
        return parameters

    def visitParameter(self, ctx):
        """
        parameter: identifier ASSIGN expression        // input :=
                 | identifier OUTPUT_ASSIGN lvalue     // output =>
                 | expression                          // positional
        """
        line   = ctx.start.line   if hasattr(ctx, 'start') else 0
        column = ctx.start.column if hasattr(ctx, 'start') else 0

        try:
            # output 参数：ident => lvalue
            if hasattr(ctx, 'OUTPUT_ASSIGN') and ctx.OUTPUT_ASSIGN():
                name = ctx.identifier().getText() if ctx.identifier() else "unknown"
                lv_text = ctx.lvalue().getText() if ctx.lvalue() else "unknown"
                return Parameter(name=name, value=lv_text, is_output=True, line=line, column=column)

            # input 参数：ident := expr
            if hasattr(ctx, 'ASSIGN') and ctx.ASSIGN():
                name = ctx.identifier().getText() if ctx.identifier() else "unknown"
                value = self.visit(ctx.expression()) if ctx.expression() else None
                return Parameter(name=name, value=value, is_output=False, line=line, column=column)

            # positional 参数：expr
            if ctx.expression():
                value = self.visit(ctx.expression())
                return Parameter(name="", value=value, is_output=False, line=line, column=column)

            # 兜底：只有 identifier（旧语法兼容）
            if ctx.identifier():
                name = ctx.identifier().getText()
                if ctx.expression():
                    value = self.visit(ctx.expression())
                else:
                    value = Identifier(name=name, line=line, column=column)
                return Parameter(name=name, value=value, is_output=False, line=line, column=column)

        except Exception as e:
            self.errors.append(f"解析参数出错: {e}")

        return Parameter(name="unknown", value=None, line=line, column=column)

    # ========================================================================
    # 表达式（ANTLR 标签分派）
    # ========================================================================

    def visitOrExpr(self, ctx):
        left  = self.visit(ctx.expression(0)) if ctx.expression(0) else None
        right = self.visit(ctx.expression(1)) if ctx.expression(1) else None
        return BinaryOp(operator=BinaryOperator.OR, left=left, right=right,
                        line=ctx.start.line if hasattr(ctx, 'start') else 0,
                        column=ctx.start.column if hasattr(ctx, 'start') else 0)

    def visitXorExpr(self, ctx):
        left  = self.visit(ctx.expression(0)) if ctx.expression(0) else None
        right = self.visit(ctx.expression(1)) if ctx.expression(1) else None
        return BinaryOp(operator=BinaryOperator.XOR, left=left, right=right,
                        line=ctx.start.line if hasattr(ctx, 'start') else 0,
                        column=ctx.start.column if hasattr(ctx, 'start') else 0)

    def visitAndExpr(self, ctx):
        left  = self.visit(ctx.expression(0)) if ctx.expression(0) else None
        right = self.visit(ctx.expression(1)) if ctx.expression(1) else None
        return BinaryOp(operator=BinaryOperator.AND, left=left, right=right,
                        line=ctx.start.line if hasattr(ctx, 'start') else 0,
                        column=ctx.start.column if hasattr(ctx, 'start') else 0)

    def visitNotExpr(self, ctx):
        operand = self.visit(ctx.expression()) if ctx.expression() else None
        return UnaryOp(operator=UnaryOperator.NOT, operand=operand,
                       line=ctx.start.line if hasattr(ctx, 'start') else 0,
                       column=ctx.start.column if hasattr(ctx, 'start') else 0)

    def visitCompareExpr(self, ctx):
        left  = self.visit(ctx.expression(0)) if ctx.expression(0) else None
        right = self.visit(ctx.expression(1)) if ctx.expression(1) else None
        op_text = ctx.op.text if hasattr(ctx, 'op') and ctx.op else "="
        _map = {'=': BinaryOperator.EQ, '<>': BinaryOperator.NEQ,
                '<': BinaryOperator.LT, '>': BinaryOperator.GT,
                '<=': BinaryOperator.LE, '>=': BinaryOperator.GE}
        op = _map.get(op_text, BinaryOperator.EQ)
        return BinaryOp(operator=op, left=left, right=right,
                        line=ctx.start.line if hasattr(ctx, 'start') else 0,
                        column=ctx.start.column if hasattr(ctx, 'start') else 0)

    def visitAddSubExpr(self, ctx):
        left  = self.visit(ctx.expression(0)) if ctx.expression(0) else None
        right = self.visit(ctx.expression(1)) if ctx.expression(1) else None
        op_text = ctx.op.text if hasattr(ctx, 'op') and ctx.op else "+"
        op = BinaryOperator.ADD if op_text == '+' else BinaryOperator.SUB
        return BinaryOp(operator=op, left=left, right=right,
                        line=ctx.start.line if hasattr(ctx, 'start') else 0,
                        column=ctx.start.column if hasattr(ctx, 'start') else 0)

    def visitMulDivExpr(self, ctx):
        left  = self.visit(ctx.expression(0)) if ctx.expression(0) else None
        right = self.visit(ctx.expression(1)) if ctx.expression(1) else None
        op_text = ctx.op.text if hasattr(ctx, 'op') and ctx.op else "*"
        _map = {'*': BinaryOperator.MUL, '/': BinaryOperator.DIV, 'MOD': BinaryOperator.MOD}
        op = _map.get(op_text, BinaryOperator.MUL)
        return BinaryOp(operator=op, left=left, right=right,
                        line=ctx.start.line if hasattr(ctx, 'start') else 0,
                        column=ctx.start.column if hasattr(ctx, 'start') else 0)

    def visitUnaryMinus(self, ctx):
        operand = self.visit(ctx.expression()) if ctx.expression() else None
        return UnaryOp(operator=UnaryOperator.NEG, operand=operand,
                       line=ctx.start.line if hasattr(ctx, 'start') else 0,
                       column=ctx.start.column if hasattr(ctx, 'start') else 0)

    def visitUnaryPlus(self, ctx):
        operand = self.visit(ctx.expression()) if ctx.expression() else None
        return UnaryOp(operator=UnaryOperator.POS, operand=operand,
                       line=ctx.start.line if hasattr(ctx, 'start') else 0,
                       column=ctx.start.column if hasattr(ctx, 'start') else 0)

    def visitParenExpr(self, ctx):
        if ctx.expression():
            return self.visit(ctx.expression())
        return None

    def visitFuncCallExpr(self, ctx):
        """表达式中的函数调用  callExpr"""
        name = "unknown"
        parameters = []
        try:
            call = ctx.callExpr()
            if call:
                if call.identifier():
                    name = call.identifier().getText()
                if call.parameterList():
                    parameters = self.visitParameterList(call.parameterList()) or []
        except Exception as e:
            self.errors.append(f"解析函数调用表达式出错: {e}")
        return FuncCallExpr(
            name=name,
            parameters=parameters,
            line=ctx.start.line if hasattr(ctx, 'start') else 0,
            column=ctx.start.column if hasattr(ctx, 'start') else 0
        )

    def visitAtomExprRule(self, ctx):
        """
        AtomExprRule: atomExpr
        atomExpr: atomBase (memberOrIndex)*
        atomBase: literal | identifier
        """
        try:
            atom = ctx.atomExpr()
            if atom is None:
                return None
            return self._visitAtomExpr(atom)
        except Exception as e:
            self.errors.append(f"解析AtomExprRule出错: {e}")
        return None

    def _visitAtomExpr(self, ctx):
        """atomExpr: atomBase (memberOrIndex)*"""
        base = None
        try:
            ab = ctx.atomBase()
            if ab:
                if ab.literal():
                    base = self.visitLiteral(ab.literal())
                elif ab.identifier():
                    name = ab.identifier().getText()
                    base = Identifier(
                        name=name,
                        line=ab.start.line if hasattr(ab, 'start') else 0,
                        column=ab.start.column if hasattr(ab, 'start') else 0
                    )

            # 链式 memberOrIndex
            mi_list = ctx.memberOrIndex()
            if mi_list:
                if not isinstance(mi_list, list):
                    mi_list = [mi_list]
                for mi in mi_list:
                    if mi.DOT() and mi.identifier():
                        member_name = mi.identifier().getText()
                        base = MemberAccess(
                            base=base,
                            member=member_name,
                            line=mi.start.line if hasattr(mi, 'start') else 0,
                            column=mi.start.column if hasattr(mi, 'start') else 0
                        )
                    elif mi.LBRACKET() and mi.expression():
                        idx = self.visit(mi.expression())
                        base = ArrayIndex(
                            base=base,
                            index=idx,
                            line=mi.start.line if hasattr(mi, 'start') else 0,
                            column=mi.start.column if hasattr(mi, 'start') else 0
                        )
        except Exception as e:
            self.errors.append(f"解析AtomExpr出错: {e}")
        return base

    # ---- 兼容：旧标签 IdentifierExpr / LiteralExpr ----

    def visitIdentifierExpr(self, ctx):
        name = "unknown"
        try:
            if ctx.identifier():
                name = ctx.identifier().getText()
        except Exception:
            pass
        return Identifier(name=name,
                          line=ctx.start.line if hasattr(ctx, 'start') else 0,
                          column=ctx.start.column if hasattr(ctx, 'start') else 0)

    def visitLiteralExpr(self, ctx):
        if ctx.literal():
            return self.visitLiteral(ctx.literal())
        return None

    # ========================================================================
    # 字面量
    # ========================================================================

    def visitLiteral(self, ctx):
        """literal: INTEGER_LITERAL | HEX_LITERAL | REAL_LITERAL | BOOL_LITERAL | STRING_LITERAL | TIME_LITERAL"""
        line   = ctx.start.line   if hasattr(ctx, 'start') else 0
        column = ctx.start.column if hasattr(ctx, 'start') else 0
        text   = ctx.getText()

        # TIME_LITERAL  T#1s  T#500ms
        if hasattr(ctx, 'TIME_LITERAL') and ctx.TIME_LITERAL():
            return Literal(value=text, data_type=DataType.TIME, line=line, column=column)

        # HEX_LITERAL  16#FF
        if hasattr(ctx, 'HEX_LITERAL') and ctx.HEX_LITERAL():
            try:
                hex_str = text.split('#', 1)[-1].replace('_', '')
                return Literal(value=int(hex_str, 16), data_type=DataType.INT, line=line, column=column)
            except Exception:
                pass

        # INTEGER_LITERAL
        if hasattr(ctx, 'INTEGER_LITERAL') and ctx.INTEGER_LITERAL():
            try:
                return Literal(value=int(text), data_type=DataType.INT, line=line, column=column)
            except Exception:
                pass

        # REAL_LITERAL
        if hasattr(ctx, 'REAL_LITERAL') and ctx.REAL_LITERAL():
            try:
                return Literal(value=float(text), data_type=DataType.REAL, line=line, column=column)
            except Exception:
                pass

        # BOOL_LITERAL
        if hasattr(ctx, 'BOOL_LITERAL') and ctx.BOOL_LITERAL():
            return Literal(value=(text == 'TRUE'), data_type=DataType.BOOL, line=line, column=column)

        # STRING_LITERAL
        if hasattr(ctx, 'STRING_LITERAL') and ctx.STRING_LITERAL():
            value = text[1:-1] if len(text) >= 2 else text
            return Literal(value=value, data_type=DataType.STRING, line=line, column=column)

        # 兜底：自动推断
        try:
            if '.' in text:
                return Literal(value=float(text), data_type=DataType.REAL, line=line, column=column)
            return Literal(value=int(text), data_type=DataType.INT, line=line, column=column)
        except Exception:
            return Literal(value=text, data_type=DataType.UNKNOWN, line=line, column=column)


# ============================================================================
# 便捷入口
# ============================================================================

def build_ast(parse_tree):
    """从 ANTLR 解析树构建 AST"""
    if parse_tree is None:
        return Program(name="Empty", variables=[], statements=[])

    builder = ASTBuilder()
    try:
        ast = builder.visitProgram(parse_tree)
        if not isinstance(ast, Program):
            return Program(name="Unknown", variables=[], statements=[])
        if builder.errors:
            print("AST build warnings:")
            for err in builder.errors:
                print(f"  {err}")
        return ast
    except Exception as e:
        print(f"Error building AST: {e}")
        import traceback
        traceback.print_exc()
        return Program(name="Error", variables=[], statements=[])
