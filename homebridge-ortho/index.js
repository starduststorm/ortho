var Service, Characteristic;

module.exports = function(homebridge) {
  Service = homebridge.hap.Service;
  Characteristic = homebridge.hap.Characteristic;
  homebridge.registerAccessory("homebridge-ortho", "ortho", ortho);
};


ortho.prototype.getServices = function() {
  let informationService = new Service.AccessoryInformation();
  informationService
    .setCharacteristic(Characteristic.Manufacturer, "Opal Holley")
    .setCharacteristic(Characteristic.Model, "v1")
    .setCharacteristic(Characteristic.SerialNumber, "orthov1");

  let switchService = new Service.Switch("ortho");
  switchService
    .getCharacteristic(Characteristic.On)
    .on('get', this.getState.bind(this))
    .on('set', this.setState.bind(this));

  this.informationService = informationService;
  this.switchService = switchService;
  return [informationService, switchService];
};

const request = require('request');
const url = require('url');

function ortho(log, config) {
  this.log = log;
  this.getUrl = url.parse(config['getUrl']);
  this.postUrl = url.parse(config['postUrl']);
}

ortho.prototype.getState = function(callback) {
  this.log("About to getState ortho at getUrl ", this.getUrl);
  request.get({
    url: this.getUrl,
    method: 'GET',
  },
  function (error, response, body) {
    if (error) {
      if (response) {
        this.log('STATUS: ' + response.statusCode);
      }
      this.log(error.message);
      return callback(error);
    }
    return callback(null, body == "true" ? true : false);
  }.bind(this));
};

ortho.prototype.setState = function(on, callback) {
  this.log("About to switch ortho " + on + " at postUrl ", this.postUrl);
  request.post({
    url: this.postUrl,
    body: JSON.stringify({'targetState': on}),
    method: 'POST',
    headers: {'Content-type': 'application/json'}
  },
  function (error, response) {
    if (error) {
      if (response) {
        this.log('STATUS: ' + response.statusCode);
      }
      this.log(error.message);
      return callback(error);
    }
    return callback();
  }.bind(this));
};
