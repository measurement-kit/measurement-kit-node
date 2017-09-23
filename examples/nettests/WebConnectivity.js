import mk from 'measurement-kit'

import {
  WebConnectivity
} from 'measurement-kit/nettests'

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

  verbosity: mk.LOG_INFO,
  inputFilePath: '',
  errorFilePath: '',
  outputFilePath: ''
}

wc = WebConnectivity(options)
wc.on('progress', (prog, s) => {
  console.log('progress', prog, s)
})
wc.on('log', (type, s) => {
  console.log('log', type, s)
})
wc.on('entry', (e) => {
  console.log('entry', e)
})
wc.start((result) => {
  console.log('web_connectivity test finished running with result', result)
})
