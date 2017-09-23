import mk from 'measurement-kit'

const options = {
  // Maybe we can bundle these as part of the library?
	geoipCountryPath: '',
	geoipAsnPath: '',
	networkType: 'wifi',
  registryUrl: mk.orchestrate.TESTING_REGISTRY_URL
}
-
const client = mk.orchestrate.Client(options)
client.register({
    password: 'THE_DEVICE_PASSWORD',
    type: 'probe' // This is the default
  })
  .then(response => {
    console.log(response)
    // Simulate a network change
    client.update({
      networkType: '3g'
    })
    .then(response => {
      console.log(response)
    })
    .catch(error => {
      console.log(error)
    })
  })
  .catch(error => {
    console.log(error)
  })
