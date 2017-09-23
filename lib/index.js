const bindings = require('bindings')('measurement-kit')
const Promise = require('any-promise')


class WebConnectivity {
  constructor(options) {
    this.options = options
  }

  on(what) {
    // XXX
  }

  run() {
    return new Promise((resolve, reject) => {
      resolve({testResult: 'bar'})
      //reject(new Error('something bad happened'))
    })
  }
}

const library = {
  WebConnectivity
}

module.exports = library
