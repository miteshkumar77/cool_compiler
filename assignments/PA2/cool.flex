/*
 *  The scanner definition for COOL.
 */

/*
 *  Stuff enclosed in %{ %} in the first section is copied verbatim to the
 *  output, so headers and global definitions are placed here to be visible
 * to the code in the file.  Dont remove anything that was here initially
 */
%{
#include <cool-parse.h>
#include <stringtab.h>
#include <utilities.h>

/* The compiler assumes these identifiers. */
#define yylval cool_yylval
#define yylex  cool_yylex

/* Max size of string constants */
#define MAX_STR_CONST 1025
#define YY_NO_UNPUT   /* keep g++ happy */

extern FILE *fin; /* we read from this file */

/* define YY_INPUT so we read from the FILE fin:
 * This change makes it possible to use this scanner in
 * the Cool compiler.
 */
#undef YY_INPUT
#define YY_INPUT(buf,result,max_size) \
	if ( (result = fread( (char*)buf, sizeof(char), max_size, fin)) < 0) \
		YY_FATAL_ERROR( "read() in flex scanner failed");

char string_buf[MAX_STR_CONST]; /* to assemble string constants */
char *string_buf_ptr;

extern int curr_lineno;
extern int verbose_flag;

extern YYSTYPE cool_yylval;

/*
 *  Add Your own definitions here
 */

%}

/*
 * Define names for regular expressions here.
 */



LETTER          [a-zA-Z]
DIGIT           [0-9]
CLASS           [cC][lL][aA][sS][sS]
ELSE            [eE][lL][sS][eE]
FALSE           f[aA][lL][sS][eE]
FI              [fF][iI]
IF              [iI][fF]
IN              [iI][nN]
INHERITS        [iI][nN][hH][eE][rR][iI][tT][sS]
ISVOID          [iI][sS][vV][oO][iI][dD]
LET             [lL][eE][tT]
LOOP            [lL][oO][oO][pP]
POOL            [pP][oO][oO][lL]
THEN            [tT][hH][eE][nN]
WHILE           [wW][hH][iI][lL][eE]
ASSIGN          <-
CASE            [cC][aA][sS][eE]
ESAC            [eE][sS][aA][cC]
NEW             [nN][eE][wW]
OF              [oO][fF]
DARROW          =>
NOT             [nN][oO][tT]
TRUE            t[rR][uU][eE]
INT_CONST       [0-9]+
BOOL_CONST      {TRUE}|{FALSE}
KEYWORDS        {CLASS}|{ELSE}|{FALSE}|{FI}|{IF}|{IN}|{INHERITS}|{ISVOID}|{LET}|{LOOP}|{POOL}|{THEN}|{WHILE}|{CASE}|{ESAC}|{NEW}|{OF}|{NOT}|{TRUE}
TYPEID          ([A-Z]({LETTER}|{DIGIT}|_)*)
OBJECTID        ([a-z]({LETTER}|{DIGIT}|_)*)
WHITESPACE      (" "|\f|\r|\t|\v)+
LINE_END        \n
START_CMT       "(*"
END_CMD         "*)"
STR_TERM        "\""

%x              CMT
%x              STR
%x              STRERROR
%%

 /*
  *  Nested comments
  */


 /*
  *  The multiple-character operators.
  */

{STR_TERM}         {
  string_buf_ptr = string_buf;
  BEGIN(STR);
}
<STR>{STR_TERM}    {
  BEGIN(INITIAL);
  (*string_buf_ptr) = '\0';
  cool_yylval.symbol = stringtable.add_string(string_buf);
  return STR_CONST;
}

<STR>"\\"\n       {
  ++curr_lineno;
  if (string_buf_ptr - string_buf + 1 > MAX_STR_CONST) {
    BEGIN(STRERROR);
    cool_yylval.error_msg = "String constant too long";
    return (ERROR);
  }
  *(string_buf_ptr++) = '\n';
}
<STR>\n            {
  ++curr_lineno;
  cool_yylval.error_msg = "Unterminated string constant";
  string_buf_ptr = string_buf;
  return (ERROR); 
}

<STR>0                {
  cool_yylval.error_msg = "String contains null character";
  BEGIN(STRERROR);
  return (ERROR);
}
<STR>.                {
  if (string_buf_ptr - string_buf + 1 > MAX_STR_CONST) {
    BEGIN(STRERROR);
    cool_yylval.error_msg = "String constant too long";
    return (ERROR);
  }
  *(string_buf_ptr++) = yytext[0];
}

<STRERROR>{STR_TERM} {
  BEGIN(INITIAL);
}



{CLASS}     { return (CLASS); }
{ELSE}      { return (ELSE); }
{FALSE}     { cool_yylval.boolean = false; return (BOOL_CONST); }
{FI}        { return (FI); }
{IF}        { return (IF); }
{IN}        { return (IN); }
{INHERITS}  { return (INHERITS); }
{ISVOID}    { return (ISVOID); }
{LET}       { return (LET); }
{LOOP}      { return (LOOP); }
{POOL}      { return (POOL); }
{THEN}      { return (THEN); }
{WHILE}     { return (WHILE); }
{ASSIGN}    { return (ASSIGN); }
{CASE}      { return (CASE); }
{ESAC}      { return (ESAC); }
{NEW}       { return (NEW); }
{INT_CONST} { cool_yylval.symbol = inttable.add_string(yytext); return (INT_CONST); }
{OF}        { return (OF); }
{DARROW}		{ return (DARROW); }
{NOT}       { return (NOT); }
{TRUE}      { cool_yylval.boolean = true; return (BOOL_CONST); }
{TYPEID}    { cool_yylval.symbol = stringtable.add_string(yytext); return (TYPEID); }
{OBJECTID}  { cool_yylval.symbol = stringtable.add_string(yytext); return (OBJECTID); }
{LINE_END}  { ++curr_lineno; }
{WHITESPACE} { }
"+"         { return '+'; }
"/"         { return '/'; }
"-"         { return '-'; }
"*"         { return '*'; }
"="         { return '='; }
"<"         { return '<'; }
"."         { return '.'; }
"~"         { return '~'; }
","         { return ','; }
";"         { return ';'; }
":"         { return ':'; }
"("         { return '('; }
")"         { return ')'; }
"@"         { return '@'; }
"{"         { return '{'; }
"}"         { return '}'; }
.           { cool_yylval.error_msg = yytext; return (ERROR); }


 /*
  * Keywords are case-insensitive except for the values true and false,
  * which must begin with a lower-case letter.
  */


 /*
  *  String constants (C syntax)
  *  Escape sequence \c is accepted for all characters c. Except for 
  *  \n \t \b \f, the result is c.
  *
  */


%%
