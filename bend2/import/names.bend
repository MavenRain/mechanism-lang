import Base
import ./ndjson.bend as Json

type T is Data:
  Anonymous{}
  Str{prefix: String, text: String}
  Num{prefix: String, index: String}

def parse(tag: String, value: Json.T) -> Result<&2, &2, String, T>:
  match tag:
    case "str":
      do Result<&2, &2, String, T>:
        +fields : List<&2, Json.Field<Json.T>> <- Json.exact_object(value, ["pre", "str"])
        prefix : String <- Json.nat_field(fields, "pre")
        text : String <- Json.string_field(fields, "str")
        return Str{prefix, text}
    case "num":
      do Result<&2, &2, String, T>:
        +fields : List<&2, Json.Field<Json.T>> <- Json.exact_object(value, ["pre", "i"])
        prefix : String <- Json.nat_field(fields, "pre")
        index : String <- Json.nat_field(fields, "i")
        return Num{prefix, index}
    case unknown:
      Fail{"unknown name tag " ++ unknown}

def references(value: T) -> List<&2, String>:
  match value:
    case Anonymous{}:
      Nil{}
    case Str{prefix, text}:
      [prefix]
    case Num{prefix, index}:
      [prefix]
