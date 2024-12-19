%filenames = "scanner"

 /*
  * Please don't modify the lines above.
  */

 /* You can add lex definitions here. */
 /* TODO: Put your lab2 code here */

%x comment
%x string

%%

 /*
  * Below is examples, which you can wipe out
  * and write regular expressions and actions of your own.
  *
  * All the tokens:
  *   Parser::ID
  *   Parser::STRING
  *   Parser::INT
  *   Parser::COMMA
  *   Parser::COLON
  *   Parser::SEMICOLON
  *   Parser::LPAREN
  *   Parser::RPAREN
  *   Parser::LBRACK
  *   Parser::RBRACK
  *   Parser::LBRACE
  *   Parser::RBRACE
  *   Parser::DOT
  *   Parser::PLUS
  *   Parser::MINUS
  *   Parser::TIMES
  *   Parser::DIVIDE
  *   Parser::EQ
  *   Parser::NEQ
  *   Parser::LT
  *   Parser::LE
  *   Parser::GT
  *   Parser::GE
  *   Parser::AND
  *   Parser::OR
  *   Parser::ASSIGN
  *   Parser::ARRAY
  *   Parser::IF
  *   Parser::THEN
  *   Parser::ELSE
  *   Parser::WHILE
  *   Parser::FOR
  *   Parser::TO
  *   Parser::DO
  *   Parser::LET
  *   Parser::IN
  *   Parser::END
  *   Parser::OF
  *   Parser::BREAK
  *   Parser::NIL
  *   Parser::FUNCTION
  *   Parser::VAR
  *   Parser::TYPE
  */

 /* reserved words */
"while" {adjust(); return Parser::WHILE;}
"for" {adjust(); return Parser::FOR;}
"to" {adjust(); return Parser::TO;}
"break" {adjust(); return Parser::BREAK;}
"let" {adjust(); return Parser::LET;}
"in" {adjust(); return Parser::IN;}
"end" {adjust(); return Parser::END;}
"function" {adjust(); return Parser::FUNCTION;}
"var" {adjust(); return Parser::VAR;}
"type" {adjust(); return Parser::TYPE;}
"array" {adjust(); return Parser::ARRAY;}
"if" {adjust(); return Parser::IF;}
"then" {adjust(); return Parser::THEN;}
"else" {adjust(); return Parser::ELSE;}
"do" {adjust(); return Parser::DO;}
"of" {adjust(); return Parser::OF;}
"nil" {adjust(); return Parser::NIL;}

 /* TODO: Put your lab2 code here */
 /* */
 /*
  * skip white space chars.
  * space, tabs and LF
  */
[ \t]+ {adjust();}
\n {adjust(); errormsg_->Newline();}


\/\* { adjust(); comment_level_ = 1; begin(StartCondition_::comment); }
<comment>\/\* {adjust(); comment_level_ +=1;}
<comment>\*\/ { adjust(); 
                comment_level_ -=1;
                if(comment_level_==0){
                   begin(StartCondition_::INITIAL);
                }
              }
<comment>\n {adjust(); errormsg_->Newline();}
<comment>. { 
  // std::cout << "Matched: " << matched() << std::endl;
  adjust();} // Ignore all other symbols in COMMENT.

  /* Operators */
\:\= {adjust(); return Parser::ASSIGN;}
\: {adjust(); return Parser::COLON;}
  /* Arithmetic */
\+ {adjust(); return Parser::PLUS;}
\- {adjust(); return Parser::MINUS;}
\* {adjust(); return Parser::TIMES;}
\/ {adjust(); return Parser::DIVIDE;}
  /* Comparison */
\<\> {adjust(); return Parser::NEQ;}
\>\= {adjust(); return Parser::GE;}
\<\= {adjust(); return Parser::LE;}
\= {adjust(); return Parser::EQ;}
\> {adjust(); return Parser::GT;}
\< {adjust(); return Parser::LT;}
\| {adjust(); return Parser::OR;}
\& {adjust(); return Parser::AND;}



[a-zA-Z][a-zA-Z0-9_]* {adjust(); return Parser::ID;}
[0-9]+ {adjust(); return Parser::INT;}
\, {adjust(); return Parser::COMMA;}
\; {adjust(); return Parser::SEMICOLON;}
\( {adjust(); return Parser::LPAREN;}
\) {adjust(); return Parser::RPAREN;}
\[ {adjust(); return Parser::LBRACK;}
\] {adjust(); return Parser::RBRACK;}
\{ {adjust(); return Parser::LBRACE;}
\} {adjust(); return Parser::RBRACE;}
\. {adjust(); return Parser::DOT;}
  /* A Sub Language For String */

\" { adjust(); begin(StartCondition_::string); string_buf_="";}
<string>\\n { adjustStr(); string_buf_.append("\n");} 
<string>\\t { adjustStr(); string_buf_.append("\t");} 
<string>\\[0-9]{3} {
  adjustStr();
  std::string matched_str = matched(); // \111
  char ascii_char = (char)atoi(matched_str.data()+1);
  // std::cout << "Matched" << ascii_char;
  string_buf_.append(1, ascii_char);
}
<string>\\\" { adjustStr(); string_buf_.append("\""); }
<string>\\\\ { adjustStr(); string_buf_.append("\\"); }
<string>\\[ \t\n\f]+\\ {adjustStr();}
<string>\\\^[A-Z] {
  adjustStr();
  std::string matched_str = matched(); // \^C
  char ascii_char = *(matched_str.data()+2);
  string_buf_.append(1, ascii_char - '@');
}
<string>[^"] { 
            adjustStr(); // DO NOT CHANGE TOKEN POS!
            string_buf_.append(matched()); // Put Symbol in buffer.
          }  // Deal With Empty String.
<string>\" { 
  // std::cout << "Buffer: " << string_buf_ << std::endl;
  adjustStr(); setMatched(string_buf_);
  begin(StartCondition_::INITIAL);
  return Parser::STRING; 
}
 /* illegal input */
. {adjust(); errormsg_->Error(errormsg_->tok_pos_, "illegal token");}
