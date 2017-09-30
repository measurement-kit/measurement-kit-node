const bindings = require('bindings')('measurement-kit')
const Promise = require('any-promise')

const SegfaultHandler = require('segfault-handler')
SegfaultHandler.registerHandler('crash.log')

const caBundlePath = 'deps/measurement-kit/test/fixtures/saved_ca_bundle.pem'

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

  class WebConnectivity {
    constructor(options) {
      options = options || {}

      const logLevel = options.logLevel || LOG_INFO

      this.options = options
      this.wct = new bindings.WebConnectivityTest()
      this.wct.set_options('net/ca_bundle_path', caBundlePath)
      this.wct.set_verbosity(logLevel)
    }

    addInput(input) {
      this.wct.add_input(input);
    }

    on(what) {
      // XXX
    }

    run() {
      return new Promise((resolve, reject) => {
        this.wct.run((err) => {
          if (err) {
            reject(err)
            return
          }
          resolve({testResult: 'bar'})
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
  LOG_WARNING
}
module.exports = library
