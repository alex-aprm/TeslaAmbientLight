#include <Arduino.h>
#include "FootLight.h"
#include "driver/ledc.h"

FootLight::FootLight() {
}

void FootLight::initHL(byte doorNum, byte ledHLPin) {
  _firstPixel = 0;
  _lastPixel = numPixelsFoot - 1;
  _transition = 1000;
  _numPixels = numPixelsFoot;
  if (doorNum == 1) {
    _ledChannel = LEDC_CHANNEL_0;
    _ledTimer = LEDC_TIMER_0;
  } else {
    _ledChannel = LEDC_CHANNEL_1;
    _ledTimer = LEDC_TIMER_1;
  }
  
  _ledHLPin = ledHLPin;

  // Configure timer for PWM
  ledc_timer_config_t ledc_timer = {
    .speed_mode = LEDC_LOW_SPEED_MODE,
    .duty_resolution = LEDC_TIMER_8_BIT,
    .timer_num = _ledTimer,
    .freq_hz = 5000,
    .clk_cfg = LEDC_AUTO_CLK
  };
  ledc_timer_config(&ledc_timer);

  // Configure the PWM channel
  ledc_channel_config_t ledc_channel = {
    .gpio_num = ledHLPin,
    .speed_mode = LEDC_LOW_SPEED_MODE,
    .channel = _ledChannel,
    .intr_type = LEDC_INTR_DISABLE,
    .timer_sel = _ledTimer,
    .duty = 0,  // Start with 0% duty
    .hpoint = 0
  };
  ledc_channel_config(&ledc_channel);
}

void FootLight::initRGB(byte doorNum, byte ledPin) {
  _firstPixel = 0;
  _lastPixel = numPixelsFoot - 1;
  _transition = 1000;
  _numPixels = numPixelsFoot;
  _strip = new Adafruit_NeoPixel(numPixels, ledPin, NEO_GRB + NEO_KHZ800);
  _strip->begin();
}

void FootLight::setColorByCarState(CarLight& carLight) {
  _stripBrightness = 0x50;
  if (carLight.footwellLightState == FOOTWELL_ON) {
    for (int i = 0; i < numPixelsFoot; i++) {
      _setTargetColor(i, 255, 180, 180);
    }
  } else {
    for (int i = 0; i < numPixelsFoot; i++) {
      _setTargetColor(i, 0, 0, 0);
    }
  }

  _fadeColor();

  if (_ledHLPin > 0) {
    ledc_set_duty(LEDC_LOW_SPEED_MODE, _ledChannel, round(_currentColor[0][0]));
    ledc_update_duty(LEDC_LOW_SPEED_MODE, _ledChannel);
  } else {
    _pushColorToStrip();
  }
}
