function relational_list_dir(path) {
  try {
    if (io_bytes(path).includes(0)) throw Object.assign(new Error("embedded NUL path"), {errno: -22});
    const names = require("fs").readdirSync(path);
    let rows = {$: "Nil"};
    for (let i = names.length - 1; i >= 0; --i) rows = {$: "Con", head: names[i], tail: rows};
    return io_done(rows);
  } catch (error) {
    return {$: "Fail", error: io_tup(Math.abs(error.errno || 5), String(error.message))};
  }
}
