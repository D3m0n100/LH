grammar LH;

// ============================================================================
// LH Language Grammar - CODESYS Style (IEC 61131-3)
// ============================================================================
// Version: 2.0.1 - Complete & ANTLR-validated
// Supports:
//   - PROGRAM / VAR / VAR_CONSTANT / END_VAR / END_PROGRAM
//   - All basic + extended data types (BOOL/INT/UINT/DINT/REAL/LREAL/WORD/BYTE...)
//   - ARRAY[..] OF / POINTER TO
//   - Variable initializers:  x : INT := 0;
//   - Function block calls with input (:=) and output (=>) parameters
//   - Assignments, member access (inst.Q), array indexing (arr[i])
//   - IF / ELSIF / ELSE / END_IF
//   - CASE / OF / ELSE / END_CASE
//   - FOR / TO / BY / END_FOR
//   - WHILE / DO / END_WHILE
//   - REPEAT / UNTIL / END_REPEAT
//   - EXIT / RETURN / CONTINUE
//   - Unary: - NOT +
//   - Binary: + - * / MOD AND OR XOR and all comparisons
//   - Time literals:  T#1s  T#500ms  T#1m30s
//   - Hex literals:   16#FF  16#00FF
//   - Identifiers starting with underscore: _System _DrvAI
// ============================================================================


// ============================================================================
// PARSER RULES
// ============================================================================

program
    : PROGRAM identifier varSection* statementList END_PROGRAM EOF
    ;

// ----- Variable sections -----

varSection
    : VAR          variableDeclaration+ END_VAR   # NormalVarSection
    | VAR_CONSTANT variableDeclaration+ END_VAR   # ConstVarSection
    ;

variableDeclaration
    : identifier COLON dataType (ASSIGN expression)? SEMICOLON
    ;

// ----- Data types -----

dataType
    : BOOL
    | INT
    | UINT
    | SINT
    | USINT
    | DINT
    | UDINT
    | WORD
    | DWORD
    | BYTE
    | REAL
    | LREAL
    | STRING
    | TIME
    | DT
    | DATE
    | TOD
    | POINTER TO dataType
    | ARRAY LBRACKET arrayRange RBRACKET OF dataType
    | typeIdentifier
    ;

arrayRange
    : expression DOTDOT expression
    ;

typeIdentifier
    : identifier
    ;

// ----- Statement list -----

statementList
    : statement*
    ;

statement
    : assignmentStatement
    | functionCallStatement
    | ifStatement
    | caseStatement
    | forStatement
    | whileStatement
    | repeatStatement
    | exitStatement
    | returnStatement
    | continueStatement
    | emptyStatement
    ;

emptyStatement
    : SEMICOLON
    ;

// ----- Assignment -----

assignmentStatement
    : lvalue ASSIGN expression SEMICOLON
    ;

lvalue
    : identifier (memberOrIndex)*
    ;

memberOrIndex
    : DOT identifier
    | LBRACKET expression RBRACKET
    ;

// ----- Function / Function Block call -----

functionCallStatement
    : callExpr SEMICOLON
    ;

callExpr
    : identifier LPAREN parameterList? RPAREN
    ;

parameterList
    : parameter (COMMA parameter)*
    ;

parameter
    : identifier ASSIGN expression          // input :=
    | identifier OUTPUT_ASSIGN lvalue       // output =>
    | expression                            // positional
    ;

// ----- IF statement -----

ifStatement
    : IF expression THEN statementList
      (ELSIF expression THEN statementList)*
      (ELSE statementList)?
      END_IF SEMICOLON?
    ;

// ----- CASE statement -----

caseStatement
    : CASE expression OF
      caseClause+
      (ELSE statementList)?
      END_CASE SEMICOLON?
    ;

caseClause
    : caseLabelList COLON statementList
    ;

caseLabelList
    : caseLabel (COMMA caseLabel)*
    ;

caseLabel
    : expression
    ;

// ----- FOR statement -----

forStatement
    : FOR identifier ASSIGN expression TO expression (BY expression)? DO
      statementList
      END_FOR SEMICOLON?
    ;

// ----- WHILE statement -----

whileStatement
    : WHILE expression DO
      statementList
      END_WHILE SEMICOLON?
    ;

// ----- REPEAT...UNTIL statement -----

repeatStatement
    : REPEAT statementList UNTIL expression SEMICOLON? END_REPEAT SEMICOLON?
    ;

// ----- Jump statements -----

exitStatement
    : EXIT SEMICOLON
    ;

returnStatement
    : RETURN SEMICOLON
    ;

continueStatement
    : CONTINUE SEMICOLON
    ;

// ============================================================================
// EXPRESSIONS  (precedence lowest to highest)
// ============================================================================

expression
    : expression OR  expression                                       # OrExpr
    | expression XOR expression                                       # XorExpr
    | expression AND expression                                       # AndExpr
    | NOT expression                                                  # NotExpr
    | expression op=(EQ | NEQ | LT | GT | LTE | GTE) expression     # CompareExpr
    | expression op=(PLUS | MINUS) expression                        # AddSubExpr
    | expression op=(MULT | DIV | MOD) expression                    # MulDivExpr
    | MINUS expression                                               # UnaryMinus
    | PLUS  expression                                               # UnaryPlus
    | LPAREN expression RPAREN                                       # ParenExpr
    | callExpr                                                       # FuncCallExpr
    | atomExpr                                                       # AtomExprRule
    ;

atomExpr
    : atomBase (memberOrIndex)*
    ;

atomBase
    : literal
    | identifier
    ;

// ----- Literals -----

literal
    : INTEGER_LITERAL
    | HEX_LITERAL
    | REAL_LITERAL
    | BOOL_LITERAL
    | STRING_LITERAL
    | TIME_LITERAL
    ;

// ----- Identifiers -----

identifier
    : IDENTIFIER
    | FB_IDENTIFIER
    ;


// ============================================================================
// LEXER RULES
// ============================================================================

// ----- Structural keywords -----
PROGRAM     : 'PROGRAM'      ;
END_PROGRAM : 'END_PROGRAM'  ;
VAR         : 'VAR'          ;
VAR_CONSTANT: 'VAR_CONSTANT' ;
END_VAR     : 'END_VAR'      ;

// ----- Flow-control keywords -----
IF          : 'IF'       ;
THEN        : 'THEN'     ;
ELSE        : 'ELSE'     ;
ELSIF       : 'ELSIF'    ;
END_IF      : 'END_IF'   ;
CASE        : 'CASE'     ;
OF          : 'OF'       ;
END_CASE    : 'END_CASE' ;
FOR         : 'FOR'      ;
TO          : 'TO'       ;
BY          : 'BY'       ;
DO          : 'DO'       ;
END_FOR     : 'END_FOR'  ;
WHILE       : 'WHILE'    ;
END_WHILE   : 'END_WHILE';
REPEAT      : 'REPEAT'   ;
UNTIL       : 'UNTIL'    ;
END_REPEAT  : 'END_REPEAT';
EXIT        : 'EXIT'     ;
RETURN      : 'RETURN'   ;
CONTINUE    : 'CONTINUE' ;

// ----- Data type keywords -----
BOOL        : 'BOOL'     ;
INT         : 'INT'      ;
UINT        : 'UINT'     ;
SINT        : 'SINT'     ;
USINT       : 'USINT'    ;
DINT        : 'DINT'     ;
UDINT       : 'UDINT'    ;
WORD        : 'WORD'     ;
DWORD       : 'DWORD'    ;
BYTE        : 'BYTE'     ;
REAL        : 'REAL'     ;
LREAL       : 'LREAL'    ;
STRING      : 'STRING'   ;
TIME        : 'TIME'     ;
DT          : 'DT'       ;
DATE        : 'DATE'     ;
TOD         : 'TOD'      ;
POINTER     : 'POINTER'  ;
ARRAY       : 'ARRAY'    ;

// ----- Operator keywords (must come before IDENTIFIER) -----
AND         : 'AND'  ;
OR          : 'OR'   ;
NOT         : 'NOT'  ;
XOR         : 'XOR'  ;
MOD         : 'MOD'  ;

// ----- Boolean literals (must come before IDENTIFIER) -----
BOOL_LITERAL
    : 'TRUE'
    | 'FALSE'
    ;

// ----- Operators -----
ASSIGN        : ':=' ;
OUTPUT_ASSIGN : '=>' ;
NEQ  : '<>'  ;
LTE  : '<='  ;
GTE  : '>='  ;
LT   : '<'   ;
GT   : '>'   ;
EQ   : '='   ;
PLUS    : '+' ;
MINUS   : '-' ;
MULT    : '*' ;
DIV     : '/' ;

// ----- Punctuation -----
LPAREN    : '('  ;
RPAREN    : ')'  ;
LBRACKET  : '['  ;
RBRACKET  : ']'  ;
SEMICOLON : ';'  ;
COLON     : ':'  ;
COMMA     : ','  ;
DOTDOT    : '..' ;
DOT       : '.'  ;
HASH      : '#'  ;

// ----- Time literal  T#1s  T#500ms  T#1h2m3s -----
TIME_LITERAL
    : [Tt] '#' TIME_PART+
    ;

fragment TIME_PART
    : [0-9]+ ('d' | 'h' | 'ms' | 'm' | 's' | 'us')
    ;

// ----- Hex literal  16#FF  16#00FF -----
HEX_LITERAL
    : [0-9]+ '#' [0-9a-fA-F_]+
    ;

// ----- FB identifiers starting with underscore: _System _DrvAI -----
//       Must appear AFTER all keyword tokens.
FB_IDENTIFIER
    : '_' [a-zA-Z] [a-zA-Z0-9_]*
    ;

// ----- Regular identifiers (after all keywords) -----
IDENTIFIER
    : [a-zA-Z] [a-zA-Z0-9_]*
    ;

// ----- Numeric literals -----
// NOTE: REAL_LITERAL must require at least one digit on BOTH sides of '.'
//       so that "0..9" tokenizes as  INTEGER_LITERAL DOTDOT INTEGER_LITERAL
//       and not as REAL_LITERAL("0.") REAL_LITERAL(".9").
REAL_LITERAL
    : [0-9]+ '.' [0-9]+ ( [eE] [+-]? [0-9]+ )?
    | [0-9]+               [eE] [+-]? [0-9]+
    ;

INTEGER_LITERAL
    : [0-9]+
    ;

// ----- String literals -----
STRING_LITERAL
    : '\'' ( ~['\r\n] | '\'\'' )* '\''
    | '"'  ( ~["\r\n] | '""'   )* '"'
    ;

// ----- Comments (skipped) -----
LINE_COMMENT
    : '//' ~[\r\n]* -> skip
    ;

BLOCK_COMMENT
    : '(*' .*? '*)' -> skip
    ;

// ----- Whitespace (skipped) -----
WS
    : [ \t\r\n]+ -> skip
    ;

// ============================================================================
// End of Grammar - LH v2.0.1
// ============================================================================
