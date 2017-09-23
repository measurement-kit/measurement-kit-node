// This is an example of how to re-write orchestrate functions using the ES6
// await, async syntax sugar

import mk from 'measurement-kit'

const options = {
  geoipCountryPath: '',
  geoipAsnPath: '',
  networkType: 'wifi',
  registryUrl: mk.orchestrate.TESTING_REGISTRY_URL
}

const client = mk.orchestrate.Client(options)
try {
  let response = await client.register({
    password: 'THE_DEVICE_PASSWORD',
    type: 'probe' // This is the default
  })
  // Simulate a network change
  response = await client.update({networkType: '3g'})
} catch (err) {
  console.log(error)
}
