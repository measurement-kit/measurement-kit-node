const bindings = require('bindings')('measurement-kit')
const Promise = require('any-promise')
const EventEmitter = require('events')

const SegfaultHandler = require('segfault-handler')
SegfaultHandler.registerHandler('crash.log')

const caBundlePath = 'deps/measurement-kit/test/fixtures/saved_ca_bundle.pem'

const LOG_DEBUG2 = 3
const LOG_DEBUG = 2
const LOG_INFO = 1
const LOG_WARNING = 0

const WebConnectivity = (options) => {
  /*
   * Factory method to generate a new instance of the WebConnectivity class
   * It allows you to do:
   *
   * wc = WebConnectivity(options)
   *
   * instead of:
   *
   * wc = new WebConnectivity(options)
   */

  class WebConnectivity extends EventEmitter {
    constructor(options) {
      super()
      options = options || {}

      this.options = options
      this.test = new bindings.WebConnectivityTest()

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
        });
      })
    }
  }

  return new WebConnectivity(options)

}

const library = {
  WebConnectivity,
  LOG_DEBUG,
  LOG_INFO,
  LOG_WARNING,
  LOG_DEBUG2
}
module.exports = library
