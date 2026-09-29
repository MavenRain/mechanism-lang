function relational_digest(text, limit) {
  const bytes = Buffer.from(text, "utf8");
  return require("crypto").createHash("md5").update(limit ? bytes.subarray(0, limit) : bytes).digest("hex");
}
