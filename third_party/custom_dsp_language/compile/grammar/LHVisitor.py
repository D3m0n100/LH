# Generated from LH.g4 by ANTLR 4.13.2
from antlr4 import *
if "." in __name__:
    from .LHParser import LHParser
else:
    from LHParser import LHParser

# This class defines a complete generic visitor for a parse tree produced by LHParser.

class LHVisitor(ParseTreeVisitor):

    # Visit a parse tree produced by LHParser#program.
    def visitProgram(self, ctx:LHParser.ProgramContext):
        return self.visitChildren(ctx)


    # Visit a parse tree produced by LHParser#NormalVarSection.
    def visitNormalVarSection(self, ctx:LHParser.NormalVarSectionContext):
        return self.visitChildren(ctx)


    # Visit a parse tree produced by LHParser#ConstVarSection.
    def visitConstVarSection(self, ctx:LHParser.ConstVarSectionContext):
        return self.visitChildren(ctx)


    # Visit a parse tree produced by LHParser#variableDeclaration.
    def visitVariableDeclaration(self, ctx:LHParser.VariableDeclarationContext):
        return self.visitChildren(ctx)


    # Visit a parse tree produced by LHParser#dataType.
    def visitDataType(self, ctx:LHParser.DataTypeContext):
        return self.visitChildren(ctx)


    # Visit a parse tree produced by LHParser#arrayRange.
    def visitArrayRange(self, ctx:LHParser.ArrayRangeContext):
        return self.visitChildren(ctx)


    # Visit a parse tree produced by LHParser#typeIdentifier.
    def visitTypeIdentifier(self, ctx:LHParser.TypeIdentifierContext):
        return self.visitChildren(ctx)


    # Visit a parse tree produced by LHParser#statementList.
    def visitStatementList(self, ctx:LHParser.StatementListContext):
        return self.visitChildren(ctx)


    # Visit a parse tree produced by LHParser#statement.
    def visitStatement(self, ctx:LHParser.StatementContext):
        return self.visitChildren(ctx)


    # Visit a parse tree produced by LHParser#emptyStatement.
    def visitEmptyStatement(self, ctx:LHParser.EmptyStatementContext):
        return self.visitChildren(ctx)


    # Visit a parse tree produced by LHParser#assignmentStatement.
    def visitAssignmentStatement(self, ctx:LHParser.AssignmentStatementContext):
        return self.visitChildren(ctx)


    # Visit a parse tree produced by LHParser#lvalue.
    def visitLvalue(self, ctx:LHParser.LvalueContext):
        return self.visitChildren(ctx)


    # Visit a parse tree produced by LHParser#memberOrIndex.
    def visitMemberOrIndex(self, ctx:LHParser.MemberOrIndexContext):
        return self.visitChildren(ctx)


    # Visit a parse tree produced by LHParser#functionCallStatement.
    def visitFunctionCallStatement(self, ctx:LHParser.FunctionCallStatementContext):
        return self.visitChildren(ctx)


    # Visit a parse tree produced by LHParser#callExpr.
    def visitCallExpr(self, ctx:LHParser.CallExprContext):
        return self.visitChildren(ctx)


    # Visit a parse tree produced by LHParser#parameterList.
    def visitParameterList(self, ctx:LHParser.ParameterListContext):
        return self.visitChildren(ctx)


    # Visit a parse tree produced by LHParser#parameter.
    def visitParameter(self, ctx:LHParser.ParameterContext):
        return self.visitChildren(ctx)


    # Visit a parse tree produced by LHParser#ifStatement.
    def visitIfStatement(self, ctx:LHParser.IfStatementContext):
        return self.visitChildren(ctx)


    # Visit a parse tree produced by LHParser#caseStatement.
    def visitCaseStatement(self, ctx:LHParser.CaseStatementContext):
        return self.visitChildren(ctx)


    # Visit a parse tree produced by LHParser#caseClause.
    def visitCaseClause(self, ctx:LHParser.CaseClauseContext):
        return self.visitChildren(ctx)


    # Visit a parse tree produced by LHParser#caseLabelList.
    def visitCaseLabelList(self, ctx:LHParser.CaseLabelListContext):
        return self.visitChildren(ctx)


    # Visit a parse tree produced by LHParser#caseLabel.
    def visitCaseLabel(self, ctx:LHParser.CaseLabelContext):
        return self.visitChildren(ctx)


    # Visit a parse tree produced by LHParser#forStatement.
    def visitForStatement(self, ctx:LHParser.ForStatementContext):
        return self.visitChildren(ctx)


    # Visit a parse tree produced by LHParser#whileStatement.
    def visitWhileStatement(self, ctx:LHParser.WhileStatementContext):
        return self.visitChildren(ctx)


    # Visit a parse tree produced by LHParser#repeatStatement.
    def visitRepeatStatement(self, ctx:LHParser.RepeatStatementContext):
        return self.visitChildren(ctx)


    # Visit a parse tree produced by LHParser#exitStatement.
    def visitExitStatement(self, ctx:LHParser.ExitStatementContext):
        return self.visitChildren(ctx)


    # Visit a parse tree produced by LHParser#returnStatement.
    def visitReturnStatement(self, ctx:LHParser.ReturnStatementContext):
        return self.visitChildren(ctx)


    # Visit a parse tree produced by LHParser#continueStatement.
    def visitContinueStatement(self, ctx:LHParser.ContinueStatementContext):
        return self.visitChildren(ctx)


    # Visit a parse tree produced by LHParser#AndExpr.
    def visitAndExpr(self, ctx:LHParser.AndExprContext):
        return self.visitChildren(ctx)


    # Visit a parse tree produced by LHParser#AtomExprRule.
    def visitAtomExprRule(self, ctx:LHParser.AtomExprRuleContext):
        return self.visitChildren(ctx)


    # Visit a parse tree produced by LHParser#MulDivExpr.
    def visitMulDivExpr(self, ctx:LHParser.MulDivExprContext):
        return self.visitChildren(ctx)


    # Visit a parse tree produced by LHParser#XorExpr.
    def visitXorExpr(self, ctx:LHParser.XorExprContext):
        return self.visitChildren(ctx)


    # Visit a parse tree produced by LHParser#CompareExpr.
    def visitCompareExpr(self, ctx:LHParser.CompareExprContext):
        return self.visitChildren(ctx)


    # Visit a parse tree produced by LHParser#UnaryPlus.
    def visitUnaryPlus(self, ctx:LHParser.UnaryPlusContext):
        return self.visitChildren(ctx)


    # Visit a parse tree produced by LHParser#UnaryMinus.
    def visitUnaryMinus(self, ctx:LHParser.UnaryMinusContext):
        return self.visitChildren(ctx)


    # Visit a parse tree produced by LHParser#NotExpr.
    def visitNotExpr(self, ctx:LHParser.NotExprContext):
        return self.visitChildren(ctx)


    # Visit a parse tree produced by LHParser#ParenExpr.
    def visitParenExpr(self, ctx:LHParser.ParenExprContext):
        return self.visitChildren(ctx)


    # Visit a parse tree produced by LHParser#AddSubExpr.
    def visitAddSubExpr(self, ctx:LHParser.AddSubExprContext):
        return self.visitChildren(ctx)


    # Visit a parse tree produced by LHParser#OrExpr.
    def visitOrExpr(self, ctx:LHParser.OrExprContext):
        return self.visitChildren(ctx)


    # Visit a parse tree produced by LHParser#FuncCallExpr.
    def visitFuncCallExpr(self, ctx:LHParser.FuncCallExprContext):
        return self.visitChildren(ctx)


    # Visit a parse tree produced by LHParser#atomExpr.
    def visitAtomExpr(self, ctx:LHParser.AtomExprContext):
        return self.visitChildren(ctx)


    # Visit a parse tree produced by LHParser#atomBase.
    def visitAtomBase(self, ctx:LHParser.AtomBaseContext):
        return self.visitChildren(ctx)


    # Visit a parse tree produced by LHParser#literal.
    def visitLiteral(self, ctx:LHParser.LiteralContext):
        return self.visitChildren(ctx)


    # Visit a parse tree produced by LHParser#identifier.
    def visitIdentifier(self, ctx:LHParser.IdentifierContext):
        return self.visitChildren(ctx)



del LHParser