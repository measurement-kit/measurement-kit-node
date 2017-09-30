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

wc = mk.WebConnectivity(options)
wc.on('progress', (prog, s) => {
  console.log('progress', prog, s)
})
wc.on('log', (type, s) => {
  console.log('log', type, s)
})
wc.on('entry', (e) => {
  console.log('entry', e)
})
wc.addInput('https://ooni.io/')
wc.run()
  .then(result => {
    console.log('web_connectivity test finished running with result', result)
  })
  .catch(error => {
    console.log(error)
  })
