function mechanism_fs_error(code) {
  const number = Math.abs(code || 5);
  const entry = require("util").getSystemErrorMap().get(-number);
  const detail = number === 21 ? "Is a directory" : entry ? entry[1] : "I/O error";
  const message = detail.charAt(0).toUpperCase() + detail.slice(1);
  return { $: "Fail", error: io_tup(number >>> 0, message) };
}

function mechanism_fs(operation, path, other) {
  const fs = require("fs");
  const os = require("os");
  const p = require("path");
  if (io_bytes(path).includes(0) || (operation !== 6 && io_bytes(other).includes(0))) {
    return mechanism_fs_error(process.platform === "darwin" ? 92 : 84);
  }
  try {
    switch (operation) {
      case 0: return io_done(fs.readFileSync(path, "utf8"));
      case 1: return io_done(fs.mkdtempSync(path + ".tmp-"));
      case 2:
        if (fs.existsSync(other)) return mechanism_fs_error(17);
        fs.renameSync(path, other);
        return io_done("");
      case 3: fs.unlinkSync(path); return io_done("");
      case 4: fs.rmdirSync(path); return io_done("");
      case 5: return io_done(fs.existsSync(path) ? "1" : "0");
      case 6: fs.writeFileSync(path, other, {mode: 0o666}); return io_done("");
      case 7: {
        const crypto = require("crypto");
        for (let attempt = 0; attempt < 32; attempt++) {
          const name = p.join(os.tmpdir(), path + crypto.randomBytes(6).toString("hex") + other);
          try {
            const fd = fs.openSync(name, "wx", 0o600);
            try { fs.closeSync(fd); }
            catch (error) { fs.unlinkSync(name); throw error; }
            return io_done(name);
          } catch (error) {
            if (error.code !== "EEXIST") throw error;
          }
        }
        return mechanism_fs_error(17);
      }
      default: return mechanism_fs_error(22);
    }
  } catch (error) {
    return mechanism_fs_error(typeof error.errno === "number" ? -error.errno : 5);
  }
}

function mechanism_write_bytes(path, data) {
  if (io_bytes(path).includes(0)) return mechanism_fs_error(process.platform === "darwin" ? 92 : 84);
  const bytes = [];
  for (let xs = data; xs.$ === "Con"; xs = xs.tail) {
    if (xs.head > 255) return mechanism_fs_error(22);
    bytes.push(xs.head);
  }
  try {
    require("fs").writeFileSync(path, Uint8Array.from(bytes), {mode: 0o666});
    return io_done("");
  } catch (error) {
    return mechanism_fs_error(typeof error.errno === "number" ? -error.errno : 5);
  }
}
