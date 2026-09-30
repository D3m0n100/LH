# Generated from LH.g4 by ANTLR 4.13.2
from antlr4 import *
if "." in __name__:
    from .LHParser import LHParser
else:
    from LHParser import LHParser

# This class defines a complete listener for a parse tree produced by LHParser.
class LHListener(ParseTreeListener):

    # Enter a parse tree produced by LHParser#program.
    def enterProgram(self, ctx:LHParser.ProgramContext):
        pass

    # Exit a parse tree produced by LHParser#program.
    def exitProgram(self, ctx:LHParser.ProgramContext):
        pass


    # Enter a parse tree produced by LHParser#NormalVarSection.
    def enterNormalVarSection(self, ctx:LHParser.NormalVarSectionContext):
        pass

    # Exit a parse tree produced by LHParser#NormalVarSection.
    def exitNormalVarSection(self, ctx:LHParser.NormalVarSectionContext):
        pass


    # Enter a parse tree produced by LHParser#ConstVarSection.
    def enterConstVarSection(self, ctx:LHParser.ConstVarSectionContext):
        pass

    # Exit a parse tree produced by LHParser#ConstVarSection.
    def exitConstVarSection(self, ctx:LHParser.ConstVarSectionContext):
        pass


    # Enter a parse tree produced by LHParser#variableDeclaration.
    def enterVariableDeclaration(self, ctx:LHParser.VariableDeclarationContext):
        pass

    # Exit a parse tree produced by LHParser#variableDeclaration.
    def exitVariableDeclaration(self, ctx:LHParser.VariableDeclarationContext):
        pass


    # Enter a parse tree produced by LHParser#dataType.
    def enterDataType(self, ctx:LHParser.DataTypeContext):
        pass

    # Exit a parse tree produced by LHParser#dataType.
    def exitDataType(self, ctx:LHParser.DataTypeContext):
        pass


    # Enter a parse tree produced by LHParser#arrayRange.
    def enterArrayRange(self, ctx:LHParser.ArrayRangeContext):
        pass

    # Exit a parse tree produced by LHParser#arrayRange.
    def exitArrayRange(self, ctx:LHParser.ArrayRangeContext):
        pass


    # Enter a parse tree produced by LHParser#typeIdentifier.
    def enterTypeIdentifier(self, ctx:LHParser.TypeIdentifierContext):
        pass

    # Exit a parse tree produced by LHParser#typeIdentifier.
    def exitTypeIdentifier(self, ctx:LHParser.TypeIdentifierContext):
        pass


    # Enter a parse tree produced by LHParser#statementList.
    def enterStatementList(self, ctx:LHParser.StatementListContext):
        pass

    # Exit a parse tree produced by LHParser#statementList.
    def exitStatementList(self, ctx:LHParser.StatementListContext):
        pass


    # Enter a parse tree produced by LHParser#statement.
    def enterStatement(self, ctx:LHParser.StatementContext):
        pass

    # Exit a parse tree produced by LHParser#statement.
    def exitStatement(self, ctx:LHParser.StatementContext):
        pass


    # Enter a parse tree produced by LHParser#emptyStatement.
    def enterEmptyStatement(self, ctx:LHParser.EmptyStatementContext):
        pass

    # Exit a parse tree produced by LHParser#emptyStatement.
    def exitEmptyStatement(self, ctx:LHParser.EmptyStatementContext):
        pass


    # Enter a parse tree produced by LHParser#assignmentStatement.
    def enterAssignmentStatement(self, ctx:LHParser.AssignmentStatementContext):
        pass

    # Exit a parse tree produced by LHParser#assignmentStatement.
    def exitAssignmentStatement(self, ctx:LHParser.AssignmentStatementContext):
        pass


    # Enter a parse tree produced by LHParser#lvalue.
    def enterLvalue(self, ctx:LHParser.LvalueContext):
        pass

    # Exit a parse tree produced by LHParser#lvalue.
    def exitLvalue(self, ctx:LHParser.LvalueContext):
        pass


    # Enter a parse tree produced by LHParser#memberOrIndex.
    def enterMemberOrIndex(self, ctx:LHParser.MemberOrIndexContext):
        pass

    # Exit a parse tree produced by LHParser#memberOrIndex.
    def exitMemberOrIndex(self, ctx:LHParser.MemberOrIndexContext):
        pass


    # Enter a parse tree produced by LHParser#functionCallStatement.
    def enterFunctionCallStatement(self, ctx:LHParser.FunctionCallStatementContext):
        pass

    # Exit a parse tree produced by LHParser#functionCallStatement.
    def exitFunctionCallStatement(self, ctx:LHParser.FunctionCallStatementContext):
        pass


    # Enter a parse tree produced by LHParser#callExpr.
    def enterCallExpr(self, ctx:LHParser.CallExprContext):
        pass

    # Exit a parse tree produced by LHParser#callExpr.
    def exitCallExpr(self, ctx:LHParser.CallExprContext):
        pass


    # Enter a parse tree produced by LHParser#parameterList.
    def enterParameterList(self, ctx:LHParser.ParameterListContext):
        pass

    # Exit a parse tree produced by LHParser#parameterList.
    def exitParameterList(self, ctx:LHParser.ParameterListContext):
        pass


    # Enter a parse tree produced by LHParser#parameter.
    def enterParameter(self, ctx:LHParser.ParameterContext):
        pass

    # Exit a parse tree produced by LHParser#parameter.
    def exitParameter(self, ctx:LHParser.ParameterContext):
        pass


    # Enter a parse tree produced by LHParser#ifStatement.
    def enterIfStatement(self, ctx:LHParser.IfStatementContext):
        pass

    # Exit a parse tree produced by LHParser#ifStatement.
    def exitIfStatement(self, ctx:LHParser.IfStatementContext):
        pass


    # Enter a parse tree produced by LHParser#caseStatement.
    def enterCaseStatement(self, ctx:LHParser.CaseStatementContext):
        pass

    # Exit a parse tree produced by LHParser#caseStatement.
    def exitCaseStatement(self, ctx:LHParser.CaseStatementContext):
        pass


    # Enter a parse tree produced by LHParser#caseClause.
    def enterCaseClause(self, ctx:LHParser.CaseClauseContext):
        pass

    # Exit a parse tree produced by LHParser#caseClause.
    def exitCaseClause(self, ctx:LHParser.CaseClauseContext):
        pass


    # Enter a parse tree produced by LHParser#caseLabelList.
    def enterCaseLabelList(self, ctx:LHParser.CaseLabelListContext):
        pass

    # Exit a parse tree produced by LHParser#caseLabelList.
    def exitCaseLabelList(self, ctx:LHParser.CaseLabelListContext):
        pass


    # Enter a parse tree produced by LHParser#caseLabel.
    def enterCaseLabel(self, ctx:LHParser.CaseLabelContext):
        pass

    # Exit a parse tree produced by LHParser#caseLabel.
    def exitCaseLabel(self, ctx:LHParser.CaseLabelContext):
        pass


    # Enter a parse tree produced by LHParser#forStatement.
    def enterForStatement(self, ctx:LHParser.ForStatementContext):
        pass

    # Exit a parse tree produced by LHParser#forStatement.
    def exitForStatement(self, ctx:LHParser.ForStatementContext):
        pass


    # Enter a parse tree produced by LHParser#whileStatement.
    def enterWhileStatement(self, ctx:LHParser.WhileStatementContext):
        pass

    # Exit a parse tree produced by LHParser#whileStatement.
    def exitWhileStatement(self, ctx:LHParser.WhileStatementContext):
        pass


    # Enter a parse tree produced by LHParser#repeatStatement.
    def enterRepeatStatement(self, ctx:LHParser.RepeatStatementContext):
        pass

    # Exit a parse tree produced by LHParser#repeatStatement.
    def exitRepeatStatement(self, ctx:LHParser.RepeatStatementContext):
        pass


    # Enter a parse tree produced by LHParser#exitStatement.
    def enterExitStatement(self, ctx:LHParser.ExitStatementContext):
        pass

    # Exit a parse tree produced by LHParser#exitStatement.
    def exitExitStatement(self, ctx:LHParser.ExitStatementContext):
        pass


    # Enter a parse tree produced by LHParser#returnStatement.
    def enterReturnStatement(self, ctx:LHParser.ReturnStatementContext):
        pass

    # Exit a parse tree produced by LHParser#returnStatement.
    def exitReturnStatement(self, ctx:LHParser.ReturnStatementContext):
        pass


    # Enter a parse tree produced by LHParser#continueStatement.
    def enterContinueStatement(self, ctx:LHParser.ContinueStatementContext):
        pass

    # Exit a parse tree produced by LHParser#continueStatement.
    def exitContinueStatement(self, ctx:LHParser.ContinueStatementContext):
        pass


    # Enter a parse tree produced by LHParser#AndExpr.
    def enterAndExpr(self, ctx:LHParser.AndExprContext):
        pass

    # Exit a parse tree produced by LHParser#AndExpr.
    def exitAndExpr(self, ctx:LHParser.AndExprContext):
        pass


    # Enter a parse tree produced by LHParser#AtomExprRule.
    def enterAtomExprRule(self, ctx:LHParser.AtomExprRuleContext):
        pass

    # Exit a parse tree produced by LHParser#AtomExprRule.
    def exitAtomExprRule(self, ctx:LHParser.AtomExprRuleContext):
        pass


    # Enter a parse tree produced by LHParser#MulDivExpr.
    def enterMulDivExpr(self, ctx:LHParser.MulDivExprContext):
        pass

    # Exit a parse tree produced by LHParser#MulDivExpr.
    def exitMulDivExpr(self, ctx:LHParser.MulDivExprContext):
        pass


    # Enter a parse tree produced by LHParser#XorExpr.
    def enterXorExpr(self, ctx:LHParser.XorExprContext):
        pass

    # Exit a parse tree produced by LHParser#XorExpr.
    def exitXorExpr(self, ctx:LHParser.XorExprContext):
        pass


    # Enter a parse tree produced by LHParser#CompareExpr.
    def enterCompareExpr(self, ctx:LHParser.CompareExprContext):
        pass

    # Exit a parse tree produced by LHParser#CompareExpr.
    def exitCompareExpr(self, ctx:LHParser.CompareExprContext):
        pass


    # Enter a parse tree produced by LHParser#UnaryPlus.
    def enterUnaryPlus(self, ctx:LHParser.UnaryPlusContext):
        pass

    # Exit a parse tree produced by LHParser#UnaryPlus.
    def exitUnaryPlus(self, ctx:LHParser.UnaryPlusContext):
        pass


    # Enter a parse tree produced by LHParser#UnaryMinus.
    def enterUnaryMinus(self, ctx:LHParser.UnaryMinusContext):
        pass

    # Exit a parse tree produced by LHParser#UnaryMinus.
    def exitUnaryMinus(self, ctx:LHParser.UnaryMinusContext):
        pass


    # Enter a parse tree produced by LHParser#NotExpr.
    def enterNotExpr(self, ctx:LHParser.NotExprContext):
        pass

    # Exit a parse tree produced by LHParser#NotExpr.
    def exitNotExpr(self, ctx:LHParser.NotExprContext):
        pass


    # Enter a parse tree produced by LHParser#ParenExpr.
    def enterParenExpr(self, ctx:LHParser.ParenExprContext):
        pass

    # Exit a parse tree produced by LHParser#ParenExpr.
    def exitParenExpr(self, ctx:LHParser.ParenExprContext):
        pass


    # Enter a parse tree produced by LHParser#AddSubExpr.
    def enterAddSubExpr(self, ctx:LHParser.AddSubExprContext):
        pass

    # Exit a parse tree produced by LHParser#AddSubExpr.
    def exitAddSubExpr(self, ctx:LHParser.AddSubExprContext):
        pass


    # Enter a parse tree produced by LHParser#OrExpr.
    def enterOrExpr(self, ctx:LHParser.OrExprContext):
        pass

    # Exit a parse tree produced by LHParser#OrExpr.
    def exitOrExpr(self, ctx:LHParser.OrExprContext):
        pass


    # Enter a parse tree produced by LHParser#FuncCallExpr.
    def enterFuncCallExpr(self, ctx:LHParser.FuncCallExprContext):
        pass

    # Exit a parse tree produced by LHParser#FuncCallExpr.
    def exitFuncCallExpr(self, ctx:LHParser.FuncCallExprContext):
        pass


    # Enter a parse tree produced by LHParser#atomExpr.
    def enterAtomExpr(self, ctx:LHParser.AtomExprContext):
        pass

    # Exit a parse tree produced by LHParser#atomExpr.
    def exitAtomExpr(self, ctx:LHParser.AtomExprContext):
        pass


    # Enter a parse tree produced by LHParser#atomBase.
    def enterAtomBase(self, ctx:LHParser.AtomBaseContext):
        pass

    # Exit a parse tree produced by LHParser#atomBase.
    def exitAtomBase(self, ctx:LHParser.AtomBaseContext):
        pass


    # Enter a parse tree produced by LHParser#literal.
    def enterLiteral(self, ctx:LHParser.LiteralContext):
        pass

    # Exit a parse tree produced by LHParser#literal.
    def exitLiteral(self, ctx:LHParser.LiteralContext):
        pass


    # Enter a parse tree produced by LHParser#identifier.
    def enterIdentifier(self, ctx:LHParser.IdentifierContext):
        pass

    # Exit a parse tree produced by LHParser#identifier.
    def exitIdentifier(self, ctx:LHParser.IdentifierContext):
        pass



del LHParser