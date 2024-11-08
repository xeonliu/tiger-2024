%filenames parser
%scanner tiger/lex/scanner.h
%baseclass-preinclude tiger/absyn/absyn.h

 /*
  * Please don't modify the lines above.
  */
/*  Semantic value type */
%union {
  int ival;
  std::string* sval;
  sym::Symbol *sym;
  absyn::Exp *exp;
  absyn::ExpList *explist;
  absyn::Var *var;
  absyn::DecList *declist;
  absyn::Dec *dec;
  absyn::EFieldList *efieldlist;
  absyn::EField *efield;
  absyn::NameAndTyList *tydeclist;
  absyn::NameAndTy *tydec;
  absyn::FieldList *fieldlist;
  absyn::Field *field;
  absyn::FunDecList *fundeclist;
  absyn::FunDec *fundec;
  absyn::Ty *ty;
  }

/* Termial Symbol with semantic value */
%token <sym> ID
%token <sval> STRING
%token <ival> INT

%token
  COMMA COLON SEMICOLON LPAREN RPAREN LBRACK RBRACK
  LBRACE RBRACE DOT
  PLUS MINUS TIMES DIVIDE EQ NEQ LT LE GT GE
  AND OR ASSIGN
  ARRAY IF THEN ELSE WHILE FOR TO DO LET IN END OF
  BREAK NIL
  FUNCTION VAR TYPE

 /* token priority */
 /* TODO: Put your lab3 code here */
/* 4.5.32
  %type: associating semantic values with (non)terminals 
  With this directive, symbol-list defines of one or more blank or comma delimited grammatical symbols (i.e., terminal and/or nonterminal symbols); type is either a polymorphic type-identifier or a field name defined in the %union specification. The specified nonterminal(s) are automatically associated with the indicate semantic type. The pointed arrows are part of the type specification.

  When the semantic value type of a terminal symbol is defined the lexical scanner rather than the parser's actions must assign the appropriate semantic value to d_val_ just prior to returning the token. To associate terminal symbols with semantic values, terminal symbols can also be specified in a %type directive.
*/

/* Non-termianl Symbols & Their semantic values Here*/
%type <exp> exp expseq
%type <explist> actuals nonemptyactuals sequencing sequencing_exps
%type <var> lvalue one oneormore
/*
explicit DecList(Dec *dec)
*/
%type <declist> decs decs_nonempty
/*
FunctionDec(int pos, FunDecList *functions)
VarDec(int pos, sym::Symbol *var, sym::Symbol *typ, Exp *init)
TypeDec(int pos, NameAndTyList *types) : Dec(pos), types_(types)
*/ 
%type <dec> decs_nonempty_s vardec
%type <efieldlist> rec rec_nonempty
%type <efield> rec_one
%type <tydeclist> tydec
%type <tydec> tydec_one
%type <fieldlist> tyfields tyfields_nonempty
%type <field> tyfield
%type <ty> ty
%type <fundeclist> fundec
%type <fundec> fundec_one

%start program

%%
program:  exp  {absyn_tree_ = std::make_unique<absyn::AbsynTree>($1);};


// A.3.1 lvalue
/*
lvalue -> id
       -> lvalue . id
       -> lvalue [ exp ]
*/
lvalue:  ID  {$$ = new absyn::SimpleVar(scanner_.GetTokPos(), $1);}
  |  oneormore /* absyn::Var *var */ {$$ = $1;}
  | lvalue DOT ID {
    $$ = new absyn::FieldVar(scanner_.GetTokPos(), $1, $3);
  }
  | lvalue LBRACK exp RBRACK {
    $$ = new absyn::SubscriptVar(scanner_.GetTokPos(), $1, $3);
  }
  ;

 /* TODO: Put your lab3 code here */

// A.2 Declarations

/* decs -> {dec} */
// {} means can be none.
decs: decs_nonempty {
  $$ = $1;
}
|
// empty
{$$ = new absyn::DecList();}
;

decs_nonempty: decs_nonempty_s {
  $$ = new absyn::DecList($1);
}
;
/* dec-> tydec 
      -> vardec
      -> fundec
*/
/* Type: absyn::Dec */
decs_nonempty_s : vardec {
  $$ = $1; /* Same Level */
}
|
fundec {
  // FunctionDec(int pos, FunDecList *functions)
  $$ = new absyn::FunctionDec(scanner_.GetTokPos(), $1);
}
|
tydec {
  // TypeDec(int pos, NameAndTyList *types)
  $$ = new absyn::TypeDec(scanner_.GetTokPos(), $1);
}
;

// TODO: What's the rule for absyn::NameAndTyList *tydeclist ???
tydec: tydec_one {
  $$ = new absyn::NameAndTyList($1);
}
;

/* tydec -> type type-id = ty */
tydec_one: TYPE ID EQ ty {
  $$ = new absyn::NameAndTy($2, $4);
}
;

/*
ty -> type-id
   -> { tyfields }
   -> array of type-id
*/
ty: ID { $$ = new absyn::NameTy(scanner_.GetTokPos(), $1); }
|
LBRACE tyfields RBRACE {$$ = new absyn::RecordTy(scanner_.GetTokPos(), $2);}
|
ARRAY OF ID {$$ = new absyn::ArrayTy(scanner_.GetTokPos(), $3);}
;

/*
tyfields -> \sigma
         -> id:type-id{,id:type-id}
*/

tyfields: /* empty */ {$$ = new absyn::FieldList();}
|
tyfields_nonempty {$$ = $1;}
;

tyfields_nonempty: tyfield {
  $$ = new absyn::FieldList($1);
}
|
// TODO： Left Recurse OR Right Recurse?
tyfield COMMA tyfields_nonempty {
  $$ = $3;
  $$ -> Prepend($1);
}
;

tyfield: ID COLON ID {
  $$ = new absyn::Field(scanner_.GetTokPos(), $1, $3);
}
;

/// A.2.1 VAR
vardec: VAR ID ASSIGN exp {
  // TODO: Is nullptr valid here?
  $$ = new absyn::VarDec(scanner_.GetTokPos(), $2, nullptr, $4);
}
|
VAR ID COLON ID ASSIGN exp {
  $$ = new absyn::VarDec(scanner_.GetTokPos(), $2, $4, $6);
}
;

// A.2.2 Function


// A.3.2 Expressions
// Type: Exp
exp: lvalue {
  $$ = new absyn::VarExp(scanner_.GetTokPos(), $1);}
}
|
NIL {
  $$ = new absyn::NilExp(scanner_.GetTokPos());
}
|
INT {
  $$ = new absyn::IntExp(scanner_.GetTokPos(), $1);
}
|
// StringExp
STRING {
  $$ = new absyn::StringExp(scanner_.GetTokPos(), $1);
}
|
// CallExp
ID LPAREN actuals /* Arguments */ RPAREN {
  $$ = new absyn::CallExp(scanner_.GetTokPos(), $1, $3);
}
|
// OpExp
// Arithmetic
exp PLUS exp {
  $$ = new absyn::OpExp(scanner_.GetTokPos(), absyn::Oper::PULS_OP);
}
|
exp MINUS exp {
  $$ = new absyn::OpExp(scanner_.GetTokPos(), absyn::Oper::MINUS_OP);
}
|
exp TIMES exp {
  $$ = new absyn::OpExp(scanner_.GetTokPos(), absyn::Oper::TIMES_OP);
}
|
exp DIVIDE exp {
  $$ = new absyn::OpExp(scanner_.GetTokPos(), absyn::Oper::DIVIDE_OP);
}
|
// Comparison / String Comparisons
exp EQ exp {
  $$ = new absyn::OpExp(scanner_.GetTokPos(), absyn::Oper::EQ_OP);
}
|
exp NEQ exp {
  $$ = new absyn::OpExp(scanner_.GetTokPos(), absyn::Oper::NEQ_OP);
}
|
exp LT exp {
  $$ = new absyn::OpExp(scanner_.GetTokPos(), absyn::Oper::LT_OP);
}
|
exp LE exp {
  $$ = new absyn::OpExp(scanner_.GetTokPos(), absyn::Oper::LE_OP);
}
|
exp GT exp {
  $$ = new absyn::OpExp(scanner_.GetTokPos(), absyn::Oper::GT_OP);
}
|
exp GE exp {
  $$ = new absyn::OpExp(scanner_.GetTokPos(), absyn::Oper::GE_OP);
}
|
// Boolean Operators
exp AND exp {
  $$ = new absyn::OpExp(scanner_.GetTokPos(), absyn::Oper::AND_OP);
}
|
exp OR exp {
  $$ = new absyn::OpExp(scanner_.GetTokPos(), absyn::Oper::OR_OP);
}
|
// Record Creation
// type-id {id = exp{,id=exp}} or type-id{}
ID LBRACE rec RBRACE {
  $$ = new absyn::RecordExp(scanner_.GetTokPos(), $1, $3);
}
;


/* EFiled for Record Expression */
// type-id {id = exp{,id=exp}} or type-id{}
// %type <efieldlist> rec rec_nonempty
// %type <efield> rec_one
rec: rec_nonempty {
  $$ = $1;
} 
|
/* empty */
{
  $$ = new absyn::EFieldList();
}
;

rec_nonempty: rec_one {
  $$ = new absyn::EFieldList($1);
}
|
rec_one COMMA rec_nonempty {
  $$ = $3;
  $$->Prepend($1);
}
;

rec_one: ID EQ exp {
  $$ = new absyn::EField($1, $3);
}
;


/* Arguments */
actuals: nonemptyactuals {
  $$ = $1;
}
|
/* empty */
{
  $$ = new absyn::ExpList();
}
;

nonemptyactuals: exp {
  $$ = new absyn::ExpList($1);
}
|
exp COMMA nonemptyactuals {
  $$ = $3;
  $$->Prepend($1);
}
;