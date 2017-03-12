/*
 * This file is part of Smarkant project
 *
 * (C) 2017 Dirk Grappendorf, www.grappendorf.net
 */

const config = require('./config');
const Alexa = require('alexa-sdk');
const AWS = require('aws-sdk');

const STRINGS = {
  'en-US': {
    'smarkant_stops': 'Smarkant stops!',
    'smarkant_moves_up': 'Smarkant moves up!',
    'smarkant_moves_down': 'Smarkant moves down!',
    'smarkant_moves_to_position': 'Smarkant moves to position {position}!',
    'invalid_position': 'Position {position} is invalid!',
    'smarkant_moves_to_height': 'Smarkant moves to {height} centimeter!',
    'invalid_height': 'Heigth {height} centimeter is invalid!'
  },
  'de-DE': {
    'smarkant_stops': 'Smarkant hält an!',
    'smarkant_moves_up': 'Smarkant fährt rauf!',
    'smarkant_moves_down': 'Smarkant fährt runter!',
    'smarkant_moves_to_position': 'Smarkant fährt in Position {position}!',
    'invalid_position': 'Position {position} is ungültig!',
    'smarkant_moves_to_height': 'Smarkant fährt auf {height} Zentimeter!',
    'invalid_height': '{height} Zentimeter ist eine ungültige Höhe!'
  }
};

const getLocale = (handler) => {
  return handler.event.request.locale;
};

const i18n = (handler, key, args) => {
  const text = STRINGS[getLocale(handler)][key];
  return text.replace(/{([^{}]*)}/g, (match, param) => {
      const value = args[param];
      return typeof value === 'string' || typeof value === 'number' ? value : match;
    });
};

const positionFromIntent = (handler) => {
  return parseInt(handler.event.request.intent.slots.Position.value);
};

const positionIsValid = (position) => {
  return position >= 1 && position <= 4;
};

const heightFromIntent = (handler) => {
  return parseInt(handler.event.request.intent.slots.Height.value);
};

const heightIsValid = (position) => {
  return position >= 500 && position <= 6000;
};

const iotData = new AWS.IotData({
  endpoint: config.iotEndpoint,
  region: config.iotRegion,
  accessKeyId: config.iotAccessKeyId,
  secretAccessKey: config.iotSecretAccessKey
});

const updateSmarkantShadow = (state, callback) => {
  iotData.updateThingShadow({
    payload: JSON.stringify({
      "state": {
        "desired": state
      }
    }),
    thingName: config.iotDeviceName
  }, (_err, _data) => {
    callback();
  });
};

const handlers = {
  'StopIntent': function() {
    updateSmarkantShadow({move: 'stop'}, () => {
      this.emit(':tell', i18n(this, 'smarkant_stops'));
    });
  },
  'MoveUpIntent': function() {
    updateSmarkantShadow({move: 'position', position: 2}, () => {
      this.emit(':tell', i18n(this, 'smarkant_moves_up'));
    });
  },
  'MoveDownIntent': function() {
    updateSmarkantShadow({move: 'position', position: 1}, () => {
      this.emit(':tell', i18n(this, 'smarkant_moves_down'));
    });
  },
  'MoveToPositionIntent': function() {
    const position = positionFromIntent(this);
    if (positionIsValid(position)) {
      updateSmarkantShadow({move: 'position', position: position}, () => {
        this.emit(':tell', i18n(this, 'smarkant_moves_to_position', {position: position}));
      });
    } else {
      this.emit(':tell', i18n(this, 'invalid_position', {position: position}));
    }
  },
  'MoveToHeightIntent': function() {
    const height = heightFromIntent(this);
    if (heightIsValid(height)) {
      updateSmarkantShadow({move: 'height', height: height}, () => {
        this.emit(':tell', i18n(this, 'smarkant_moves_to_height', {height: height}));
      });
    } else {
      this.emit(':tell', i18n(this, 'invalid_height', {height: height}));
    }
  }
};

exports.handler = (event, context, _callback) => {
  const alexa = Alexa.handler(event, context);
  alexa.appId = config.appId;
  alexa.registerHandlers(handlers);
  alexa.execute();
};
