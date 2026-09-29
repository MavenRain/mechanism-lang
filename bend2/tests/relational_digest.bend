import Base

# Hash exactly the UTF-8 byte prefix used by the original diagnostic oracle.
# A zero limit selects the entire message.
def relational_digest(text: String, limit: U32) -> IO(String):
  import "./relational_digest.c"
  import "./relational_digest.js"
