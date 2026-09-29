import Base

# The compiler owns its file protocol in Bend. This effect supplies POSIX I/O.
def mechanism_fs(operation: U32, path: String, other: String) -> IO(Result<&1, &1, U32 & String, String>):
  import "./effects/fs.c"
  import "./effects/fs.js"

def mechanism_write_bytes(path: String, bytes: List<&2, U32>) -> IO(Result<&1, &1, U32 & String, String>):
  import "./effects/fs.c"
  import "./effects/fs.js"
