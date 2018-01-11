const mk = require('../../lib')

const options = {
  backend: '',
  geoipCountryPath: '',
  geoipAsnPath: '',
  saveRealProbeIp: false,
  saveRealProbeAsn: true,
  saveRealProbeCc: true,
  noCollector: false,
  collectorBaseUrl: '',
  maxRuntime: '',
  softwareName: '',
  softwareVersion: '',

  logLevel: mk.LOG_DEBUG,
  inputFilePath: '',
  errorFilePath: '',
  outputFilePath: ''
}
wc = mk.Whatsapp(options)
wc.on('progress', (prog, s) => {
  console.log('progress', prog, s)
})
wc.on('log', (type, s) => {
  console.log('log', type, s)
})
wc.on('entry', (e) => {
  console.log('entry', e)
})
wc.on('overall-data-usage', (down, up) => {
  console.log('data usage', down, up)
})
wc.run()
  .then(result => {
    console.log('whatsapp test finished running with result', result)
  })
  .catch(error => {
    console.log(error)
  })
