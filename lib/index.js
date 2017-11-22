const bindings = require('bindings')('measurement-kit')
const Promise = require('any-promise')
const EventEmitter = require('events')

const SegfaultHandler = require('segfault-handler')
SegfaultHandler.registerHandler('crash.log')

// XXX this should not be hardcoded
const caBundlePath = '/usr/local/etc/openssl/cert.pem'

const MEASUREMENT_KIT_VERSION = bindings.version()
const LOG_DEBUG2 = 3
const LOG_DEBUG = 2
const LOG_INFO = 1
const LOG_WARNING = 0

const boolOption = option => option === true ? '1' : '0'

const makeNettestFactory = nettestName => options => {
  /*
   * Factory method to generate a new instance of the WebConnectivity class
   * It allows you to do:
   *
   * nt = Nettest(options)
   *
   * instead of:
   *
   * nt = new Nettest(options)
   */
  class Nettest extends EventEmitter {
    constructor(options) {
      super()
      options = options || {}
      this.name = nettestName
      this.test = new bindings[nettestName + 'Test']()

      this.setOptions(options)
      this.bindListeners()
    }

    setOptions(options) {
      this.options = options

      this.test.set_options('save_real_probe_ip', boolOption(options.includeIp || false))
      this.test.set_options('save_real_probe_asn', boolOption(options.includeAsn || true))
      this.test.set_options('save_real_probe_cc', boolOption(options.includeCountry || true))
      this.test.set_options('no_collector', boolOption(options.noCollector || false))
      if (options.softwareName && options.softwareVersion) {
        this.test.set_options('software_name', options.softwareName)
        this.test.set_options('software_version', options.softwareVersion)
      }

      if (options.geoipCountryPath) {
        this.test.set_options('geoip_country_path', options.geoipCountryPath)
      }
      if (options.geoipAsnPath) {
        this.test.set_options('geoip_asn_path', options.geoipAsnPath)
      }

      if (options.outputPath) {
        this.test.set_options('output_path', options.outputPath)
      } else {
        this.test.set_options('no_file_report', '1')
      }
      this.test.set_verbosity(this.options.logLevel || LOG_INFO)
      this.test.set_options('net/ca_bundle_path', options.caBundlePath || caBundlePath)
    }

    bindListeners() {
      const self = this
      this.test.on_progress((percent, msg) => {
        self.emit('progress', percent, msg)
      })
      this.test.on_log((level, msg) => {
        self.emit('log', level, msg)
      })
      this.test.on_entry((entry) => {
        self.emit('entry', JSON.parse(entry))
      })
      this.test.on_event((e) => {
        self.emit('event', e)
      })
      this.test.on_end(() => {
        self.emit('end')
      })
      this.test.on_begin(() => {
        self.emit('begin')
      })
    }

    addInput(input) {
      this.test.add_input(input);
    }

    run() {
      const { test } = this
      return new Promise((resolve, reject) => {
        test.start((err) => {
          if (err) {
            reject(err)
            return
          }
          resolve()
        })
      })
    }
  }

  return new Nettest(options)

}

const WebConnectivity = makeNettestFactory('WebConnectivity')
const TcpConnect = makeNettestFactory('TcpConnect')
const Ndt = makeNettestFactory('Ndt')
const Dash = makeNettestFactory('Dash')
const MultiNdt = makeNettestFactory('MultiNdt')
const MeekFrontedRequests = makeNettestFactory('MeekFrontedRequests')
const HttpInvalidRequestLine = makeNettestFactory('HttpInvalidRequestLine')
const HttpHeaderFieldManipulation = makeNettestFactory('HttpHeaderFieldManipulation')
const DnsInjectionTest = makeNettestFactory('DnsInjection')

const library = {
  WebConnectivity,
  TcpConnect,
  Ndt,
  Dash,
  MultiNdt,
  MeekFrontedRequests,
  HttpInvalidRequestLine,
  HttpHeaderFieldManipulation,
  DnsInjectionTest,
  LOG_DEBUG,
  LOG_INFO,
  LOG_WARNING,
  LOG_DEBUG2
}
module.exports = library
