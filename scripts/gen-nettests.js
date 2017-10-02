const fs = require('fs')
const path = require('path')
const changeCase = require('change-case')
const Mustache = require('mustache')


const tmplCppPath = path.join(__dirname, 'tmpl', 'nettest.cpp')
const tmplHppPath = path.join(__dirname, 'tmpl', 'nettest.hpp')

const nettestNames = [
  'DashTest',
  'DnsInjectionTest',
  'HttpHeaderFieldManipulationTest',
  'HttpInvalidRequestLineTest',
  'MeekFrontedRequestsTest',
  'MultiNdtTest',
  'NdtTest',
  'TcpConnectTest',
  'WebConnectivityTest'
]

//MK_NODE_DECLARE_TEST(TelegramTest);
//MK_NODE_DECLARE_TEST(FacebookMessengerTest);
//MK_NODE_DECLARE_TEST(CaptivePortalTest);


fs.readFile(tmplCppPath, 'utf8', (err, tmplCpp) => {
  fs.readFile(tmplHppPath, 'utf8', (err, tmplHpp) => {
    nettestNames.forEach((nettest) => {
      const fileName = changeCase.snakeCase(nettest).replace('_test', '')
      const partials = {
        nettestPascal: nettest,
        nettestSnakeUpper: changeCase.constantCase(nettest),
        nettestSnakeNoSuffix: fileName,
      }

      //console.log(partials)

      const nettestHpp = Mustache.render(tmplHpp, partials)
      const nettestCpp = Mustache.render(tmplCpp, partials)
      const nettestHppPath = path.join(__dirname, '..', 'src', 'nettests', `${fileName}.hpp`)
      const nettestCppPath = path.join(__dirname, '..', 'src', 'nettests', `${fileName}.cpp`)

      fs.writeFile(nettestCppPath, nettestCpp, function(err) {
        if (err) {
          console.log(`FAILED ${nettestCppPath}`, err)
          return
        }
        console.log(`WRITTEN ${nettestCppPath}`, err)
      })
      fs.writeFile(nettestHppPath, nettestHpp, function(err) {
        if (err) {
          console.log(`FAILED ${nettestHppPath}`, err)
          return
        }
        console.log(`WRITTEN ${nettestHppPath}`, err)
      })
    })
  })
})


console.log('--- FOR src/addon.cpp')
nettestNames.forEach((nettest) => {
  const fileName = changeCase.snakeCase(nettest).replace('_test', '')
  console.log(`#include "nettests/${fileName}.hpp"`)
})

console.log('-- In InitAll()')
nettestNames.forEach((nettest) => {
  const fileName = changeCase.snakeCase(nettest).replace('_test', '')
  console.log(`${nettest}::Init(exports);`)
})
