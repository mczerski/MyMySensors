#include "PowerManager.h"
#include <MySensorsToolkit/MySensors.h>

namespace mys_toolkit {

int convert2mV(int v)
{
  int voltage_mV = 11 * 1000 * v * 1.1 / 1024; //includes 1/11 divider
  return voltage_mV;
}

uint8_t convertnV2LevelCoincell(int v)
{
  if (v >= 3000)
    return 100;
  else if (v <= 1800)
    return 0;
  else
    return (v - 1800) / ((30 - 18));
}

uint8_t convertnV2LevelLiIon(int v)
{
  if (v >= 4200)
    return 100;
  else if (v <= 3000)
    return 0;
  else
    return (v - 3000) / ((42 - 30));
}

uint8_t convertmV2Level(int v, bool liIonBattery)
{
  if (liIonBattery)
    return convertnV2LevelLiIon(v);
  else
    return convertnV2LevelCoincell(v);
}

PowerManager::PowerManager()
{
}

void PowerManager::handleBatteryLevel_(uint8_t value)
{
  if (abs(int(value) - int(lastBatteryLevel_)) > BATTERY_LEVEL_TRESHOLD or noUpdatesBatteryLevel_ == FORCE_UPDATE_N_READS) {
    lastBatteryLevel_ = value;
    sendBatteryLevel(value);
    noUpdatesBatteryLevel_ = 0;
  } else {
    // Increase no update counter if the humidity stayed the same
    noUpdatesBatteryLevel_++;
  }
}

static PowerManager& PowerManager::getInstance()
{
  return instance_;
}

void PowerManager::setupPowerBoost(uint8_t powerBoostPin = -1,  bool initialBoostOn = false)
{
  powerBoostPin_ = powerBoostPin;
  pinMode(powerBoostPin_, OUTPUT);
  digitalWrite(powerBoostPin_, initialBoostOn);
}

void PowerManager::setBatteryPin(uint8_t batteryPin, bool liIonBattery = false)
{
  batteryPin_ = batteryPin;
  liIonBattery_ = liIonBattery;
  analogReference(INTERNAL);
}

void PowerManager::turnBoosterOn()
{
  digitalWrite(powerBoostPin_, HIGH);
}

void PowerManager::turnBoosterOff()
{
  digitalWrite(powerBoostPin_, LOW);
}

void PowerManager::reportBatteryLevel()
{
  if (batteryPin_ == uint8_t(-1))
    return;
  int voltage = convert2mV(analogRead(batteryPin_));
  #ifdef MYS_TOOLKIT_DEBUG
  Serial.print("V: ");
  Serial.println(voltage);
  #endif
  uint8_t batteryLevel = convertmV2Level(voltage, liIonBattery_);
  handleBatteryLevel_(batteryLevel);
}

void PowerManager::forceResend()
{
  noUpdatesBatteryLevel_ = FORCE_UPDATE_N_READS;
}

PowerManager PowerManager::instance_;


} //mys_toolkit
