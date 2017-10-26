const mk = require('bindings')('measurement-kit')
console.log(`Using MK bindings directly; MK version: ${mk.version()}`)
const test = mk.HttpInvalidRequestLineTest
test()
  .on_begin(() => console.log("beginning test"))
  .on_progress((percent, message) => {
    console.log(`${percent * 100}%: ${message}`)
  })
  .on_log((severity, message) => {
    console.log(`<${severity}> ${message}`)
  })
  .on_entry((entry) => console.log("entry:", entry))
  .on_event((evt) => console.log("event:", evt))
  .on_end(() => console.log("ending test"))
  .set_options("no_file_report", 1)
  .set_verbosity(1 /* MK_LOG_INFO */)
  .start(() => console.log("final callback called"))
