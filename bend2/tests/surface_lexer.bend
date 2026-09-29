import Base
import ../surface/lexer.bend as Lexer
import ../surface/token.bend as Token
import ../kernel/error.bend as Error

def bytes_text(bytes: List<&2, U32>) -> String:
  match bytes:
    case Nil{}:
      ""
    case Con{byte, rest}:
      U32.show(byte) ++ "," ++ bytes_text(rest)

def kind_text(kind: Token.KindT) -> String:
  match kind:
    case Token.Bytes{bytes}:
      "bytes[" ++ bytes_text(bytes) ++ "]"
    case other:
      Token.describe(other)

def token_text(token: Token.T) -> String:
  match token:
    case Token.Tok{kind, Token.Loc{line, col}}:
      kind_text(kind) ++ "@" ++ Nat.show(line) ++ ":" ++ Nat.show(col) ++ ";"

def tokens_text(tokens: List<&2, Token.T>) -> String:
  match tokens:
    case Nil{}:
      ""
    case Con{token, rest}:
      token_text(token) ++ tokens_text(rest)

def result_text(result: Result<&2, &2, Error.T, List<&2, Token.T>>) -> String:
  match result:
    case Done{tokens}:
      tokens_text(tokens)
    case Fail{Error.Parse{message, line, column}}:
      message ++ "@" ++ Nat.show(line) ++ ":" ++ Nat.show(column)
    case Fail{other}:
      "unexpected error"

def check(source: String, expected: String) -> Bool:
  String.eq(result_text(Lexer.lex(source)), expected)

def padding(length: Nat, acc: String) -> String:
  match length:
    case Zero{}:
      acc
    case Succ{rest}:
      padding(rest, SCon{'x', acc})

def main() -> Bool:
  check("--" ++ padding(90000n, "\nb\"é😀\""), "bytes[195,169,240,159,152,128,]@2:1;end of input@2:10;") &&
  check("def x := 123456789012345678901234567890 -- hi\n x.1.2.10", "'def'@1:1;identifier x@1:5;':='@1:7;number 123456789012345678901234567890@1:10;identifier x@2:2;'.1'@2:3;'.2'@2:5;'.'@2:7;number 10@2:8;end of input@2:10;") &&
  check("b\"é\\x00\\n\"", "bytes[195,169,0,10,]@1:1;end of input@1:12;") &&
  check("b\"\\xg0\"", "byte escape requires two hexadecimal digits@1:3") &&
  check("b\"x\n", "unescaped newline in byte literal@1:4") &&
  check("b\"abc", "unterminated byte literal@1:6") &&
  check("000.01", "number 0@1:1;'.'@1:4;number 1@1:5;end of input@1:7;") &&
  check("b\"\\x\"", "invalid byte escape \\x@1:3") &&
  check("nu mpc[P, A] T () -> =>", "'nu'@1:1;'mpc'@1:4;'['@1:7;identifier P@1:8;','@1:9;identifier A@1:11;']'@1:12;identifier T@1:14;'()'@1:16;'->'@1:19;'=>'@1:22;end of input@1:24;")
