import Base

# IO.die appends a diagnostic newline, even when its message is empty.
def cli_exit(code:U32) -> IO(Unit):
  import "./effects/exit.c"
  import "./effects/exit.js"
