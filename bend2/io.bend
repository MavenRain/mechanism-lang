import Base
import ./host_fs.bend as Host
import ./kernel/common.bend as Common

def error_text(error: U32 & String, path: String) -> String:
  (code, message) = error
  path ++ ": " ++ message

def convert(-A: Data, result: Result<&1, &1, U32 & String, A>, path: String) -> Result<&2, &2, String, A>:
  match result:
    case Done{value}:
      Done{value}
    case Fail{error}:
      Fail{error_text(error, path)}

def fs(operation: U32, +path: String, other: String) -> IO(Result<&2, &2, String, String>):
  do IO<Result<&2, &2, String, String>>:
    result : Result<&1, &1, U32 & String, String> <- Host.mechanism_fs(operation, path, other)
    IO.pure(Result<&2, &2, String, String>, convert(String, result, path))

def unit_result(result: Result<&2, &2, String, String>) -> Result<&2, &2, String, Unit>:
  match result:
    case Fail{message}:
      Fail{message}
    case Done{value}:
      Done{Unit{}}

def fs_unit(operation: U32, path: String, other: String) -> IO(Result<&2, &2, String, Unit>):
  do IO<Result<&2, &2, String, Unit>>:
    result : Result<&2, &2, String, String> <- fs(operation, path, other)
    IO.pure(Result<&2, &2, String, Unit>, unit_result(result))

def read_file(path: String) -> IO(Result<&2, &2, String, String>):
  fs(0, path, "")

def write_file(path: String, contents: String) -> IO(Result<&2, &2, String, Unit>):
  fs_unit(6, path, contents)

def remove(path: String) -> IO(Result<&2, &2, String, Unit>):
  fs_unit(3, path, "")

def remove_dir(path: String) -> IO(Result<&2, &2, String, Unit>):
  fs_unit(4, path, "")

def exists_result(result: Result<&2, &2, String, String>) -> Bool:
  match result:
    case Fail{message}:
      False{}
    case Done{value}:
      String.eq(value, "1")

def exists(path: String) -> IO(Bool):
  do IO<Bool>:
    result : Result<&2, &2, String, String> <- fs(5, path, "")
    IO.pure(Bool, exists_result(result))

def temp_file(prefix: String, suffix: String) -> IO(Result<&2, &2, String, String>):
  fs(7, prefix, suffix)

def write_bytes(+path: String, bytes: List<&2, U32>) -> IO(Result<&2, &2, String, Unit>):
  do IO<Result<&2, &2, String, Unit>>:
    result : Result<&1, &1, U32 & String, String> <- Host.mechanism_write_bytes(path, bytes)
    IO.pure(Result<&2, &2, String, Unit>, unit_result(convert(String, result, path)))

def strip_slashes(text: String) -> String:
  match text:
    case SCon{Chr{47}, rest}:
      strip_slashes(rest)
    case other:
      other

def target_name(trimmed: String, original: String) -> String:
  match trimmed:
    case SNil{}:
      match original:
        case SNil{}:
          "."
        case other:
          "/"
    case other:
      String.reverse(other)

def normalize_target(+directory: String) -> String:
  target_name(strip_slashes(String.reverse(directory)), directory)

def remove_if_exists(yes: Bool, path: String) -> IO(Result<&2, &2, String, Unit>):
  match yes:
    case False{}:
      IO.pure(Result<&2, &2, String, Unit>, Done{Unit{}})
    case True{}:
      remove(path)

law cleanup_files:
  for files: List<&2, Common.Pair<String, String>>
  for +directory: String
  IO(Result<&2, &2, String, Unit>)

@unsafe def cleanup_more(result: Result<&2, &2, String, Unit>, files: List<&2, Common.Pair<String, String>>, directory: String) -> IO(Result<&2, &2, String, Unit>):
  match result:
    case Fail{message}:
      IO.pure(Result<&2, &2, String, Unit>, Fail{message})
    case Done{unit}:
      cleanup_files(files, directory)

@unsafe def cleanup_files(files, directory):
  match files:
    case Nil{}:
      remove_dir(directory)
    case Con{Common.Pair{name, contents}, rest}:
      do IO<Result<&2, &2, String, Unit>>:
        +path : String = directory ++ "/" ++ name
        present : Bool <- exists(path)
        result : Result<&2, &2, String, Unit> <- remove_if_exists(present, path)
        cleanup_more(result, rest, directory)

def cleanup_error(result: Result<&2, &2, String, Unit>, message: String) -> Result<&2, &2, String, Unit>:
  match result:
    case Done{unit}:
      Fail{message}
    case Fail{reason}:
      Fail{message ++ "; temporary output: " ++ reason}

def finish_publish(result: Result<&2, &2, String, Unit>, files: List<&2, Common.Pair<String, String>>, temporary: String) -> IO(Result<&2, &2, String, Unit>):
  match result:
    case Done{unit}:
      IO.pure(Result<&2, &2, String, Unit>, Done{Unit{}})
    case Fail{message}:
      do IO<Result<&2, &2, String, Unit>>:
        result : Result<&2, &2, String, Unit> <- cleanup_files(files, temporary)
        IO.pure(Result<&2, &2, String, Unit>, cleanup_error(result, message))

law write_files:
  for files: List<&2, Common.Pair<String, String>>
  for +directory: String
  IO(Result<&2, &2, String, Unit>)

@unsafe def write_more(result: Result<&2, &2, String, Unit>, files: List<&2, Common.Pair<String, String>>, directory: String) -> IO(Result<&2, &2, String, Unit>):
  match result:
    case Fail{message}:
      IO.pure(Result<&2, &2, String, Unit>, Fail{message})
    case Done{unit}:
      write_files(files, directory)

@unsafe def write_files(files, directory):
  match files:
    case Nil{}:
      IO.pure(Result<&2, &2, String, Unit>, Done{Unit{}})
    case Con{Common.Pair{name, contents}, rest}:
      do IO<Result<&2, &2, String, Unit>>:
        result : Result<&2, &2, String, Unit> <- write_file(directory ++ "/" ++ name, contents)
        write_more(result, rest, directory)

def rename_error(error: U32 & String, original: String) -> Result<&2, &2, String, Unit>:
  (code, message) = error
  Fail{original ++ ": " ++ message}

def rename_result(result: Result<&1, &1, U32 & String, String>, original: String) -> Result<&2, &2, String, Unit>:
  match result:
    case Done{value}:
      Done{Unit{}}
    case Fail{error}:
      rename_error(error, original)

def publish_rename(present: Bool, temporary: String, target: String, original: String) -> IO(Result<&2, &2, String, Unit>):
  match present:
    case True{}:
      IO.pure(Result<&2, &2, String, Unit>, Fail{"output already exists: " ++ original})
    case False{}:
      do IO<Result<&2, &2, String, Unit>>:
        result : Result<&1, &1, U32 & String, String> <- Host.mechanism_fs(2, temporary, target)
        IO.pure(Result<&2, &2, String, Unit>, rename_result(result, original))

def publish_written(result: Result<&2, &2, String, Unit>, temporary: String, +target: String, original: String) -> IO(Result<&2, &2, String, Unit>):
  match result:
    case Fail{message}:
      IO.pure(Result<&2, &2, String, Unit>, Fail{message})
    case Done{unit}:
      do IO<Result<&2, &2, String, Unit>>:
        present : Bool <- exists(target)
        publish_rename(present, temporary, target, original)

def publish_staged(result: Result<&2, &2, String, String>, +files: List<&2, Common.Pair<String, String>>, target: String, original: String) -> IO(Result<&2, &2, String, Unit>):
  match result:
    case Fail{message}:
      IO.pure(Result<&2, &2, String, Unit>, Fail{message})
    case Done{+temporary}:
      do IO<Result<&2, &2, String, Unit>>:
        result : Result<&2, &2, String, Unit> <- write_files(files, temporary)
        result : Result<&2, &2, String, Unit> <- publish_written(result, temporary, target, original)
        finish_publish(result, files, temporary)

def publish_exists(present: Bool, +target: String, original: String, files: List<&2, Common.Pair<String, String>>) -> IO(Result<&2, &2, String, Unit>):
  match present:
    case True{}:
      IO.pure(Result<&2, &2, String, Unit>, Fail{"output already exists: " ++ original})
    case False{}:
      do IO<Result<&2, &2, String, Unit>>:
        result : Result<&2, &2, String, String> <- fs(1, target, "")
        publish_staged(result, files, target, original)

def publish(+directory: String, files: List<&2, Common.Pair<String, String>>) -> IO(Result<&2, &2, String, Unit>):
  do IO<Result<&2, &2, String, Unit>>:
    +target : String = normalize_target(directory)
    present : Bool <- exists(target)
    publish_exists(present, target, directory, files)
