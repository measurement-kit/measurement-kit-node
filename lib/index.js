const bindings = require('bindings')('measurement-kit')
const Promise = require('any-promise')
const EventEmitter = require('events')

const SegfaultHandler = require('segfault-handler')
SegfaultHandler.registerHandler('crash.log')

// XXX this should not be hardcoded
const caBundlePath = '/usr/local/etc/openssl/cert.pem'

const LOG_DEBUG2 = 3
const LOG_DEBUG = 2
const LOG_INFO = 1
const LOG_WARNING = 0

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

      this.options = options
      this.test = new bindings[nettestName]()

      this.setOptions()
      this.bindListeners()
    }

    setOptions() {
      this.test.set_options('net/ca_bundle_path', caBundlePath)
      this.test.set_options('no_file_report', '1')
      this.test.set_verbosity(this.options.logLevel || LOG_INFO)
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
        self.emit('entry', entry)
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
      return new Promise((resolve, reject) => {
        this.test.start((err) => {
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

const WebConnectivity = makeNettestFactory('WebConnectivityTest')
const TcpConnect = makeNettestFactory('TcpConnectTest')
const Ndt = makeNettestFactory('NdtTest')
const Dash = makeNettestFactory('DashTest')
const MultiNdt = makeNettestFactory('MultiNdtTest')
const MeekFrontedRequests = makeNettestFactory('MeekFrontedRequestsTest')
const HttpInvalidRequestLine = makeNettestFactory('HttpInvalidRequestLineTest')
const HttpHeaderFieldManipulation = makeNettestFactory('HttpHeaderFieldManipulationTest')
const DnsInjectionTest = makeNettestFactory('DnsInjectionTest')

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
