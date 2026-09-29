import Base

# Test protocol effect: preserve filenames as separate strings.
def relational_list_dir(path: String) -> IO(Result<&1, &1, U32 & String, List<&2, String>>):
  import "./relational_fs.c"
  import "./relational_fs.js"
