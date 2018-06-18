#ifndef MyMySensor_h
#define MyMySensor_h

#include "MyMySensors.h"

namespace mymysensors {

class MyValueBase {
protected:
  static const uint8_t FORCE_UPDATE_N_READS = 10;
  MyMessage msg_;
  uint8_t noUpdates_;
  uint8_t sensorId_;
  uint8_t sensorType_;
  static uint8_t valuesCount_;
  static constexpr uint8_t MAX_VALUES = 10;
  static MyValueBase* values_[MAX_VALUES];
  static bool success_;

  void present_() {
    ::present(sensorId_, sensorType_);
  }
  void forceResend_() {
    noUpdates_ = FORCE_UPDATE_N_READS;
  }

public:
  MyValueBase(uint8_t sensorId, uint8_t type, uint8_t sensorType)
    : msg_(sensorId, type), noUpdates_(0), sensorId_(sensorId), sensorType_(sensorType)
  {
    if (valuesCount_ < MAX_VALUES)
      values_[valuesCount_++] = this;
  }
  static void present() {
    for (size_t i=0; i<valuesCount_; i++)
      values_[i]->present_();
  }
  static void forceResend() {
    for (size_t i=0; i<valuesCount_; i++)
      values_[i]->forceResend_();
  }
  static void beforeUpdate() {
    success_ = true;
  }
  static void update(bool success) {
    success_ &= success;
  }
  static bool afterUpdate() {
    return success_;
  }
};

uint8_t MyValueBase::valuesCount_ = 0;
MyValueBase* MyValueBase::values_[];
bool MyValueBase::success_ = true;

template <typename ValueType>
class MyValue : public MyValueBase {
  ValueType lastValue_;
  ValueType treshold_;

  bool handleValue_(ValueType value) {
    bool success = true;

    if (abs(lastValue_ - value) > treshold_ || noUpdates_ == FORCE_UPDATE_N_READS) {
      #ifdef MY_MY_DEBUG
      Serial.print("t=");
      Serial.print(msg_.type);
      Serial.print(",c=");
      Serial.print(msg_.sensor);
      Serial.print(". last=");
      Serial.print(lastValue_);
      Serial.print(", curent=");
      Serial.print(value);
      Serial.print(", noUpdates=");
      Serial.println(noUpdates_);
      #endif
      lastValue_ = value;

      success = sendMessage_(value);
      if (success) {
        noUpdates_ = 0;
      }
      else {
        noUpdates_ = FORCE_UPDATE_N_READS;
        #ifdef MY_MY_DEBUG
        Serial.print("t=");
        Serial.print(msg_.type);
        Serial.print(",c=");
        Serial.print(msg_.sensor);
        Serial.println(". Send failed");
        #endif
      }
    } else {
      noUpdates_++;
    }
    return success;
  }
  bool sendMessage_(ValueType value) {
    setMessageValue_(msg_, value);
    return send(msg_, true) and wait(2000, C_SET, msg_.type);
  }
public:
  MyValue(uint8_t sensorId, uint8_t type, uint8_t sensorType, ValueType treshold = 0)
    : MyValueBase(sensorId, type, sensorType), lastValue_(-1), treshold_(treshold)  {}
  void update(ValueType value) {
    MyValueBase::update(handleValue_(value));
  }
};

class MyParameterBase {
protected:
  MyMessage msg_;
  uint8_t sensorId_;
  uint8_t sensorType_;
  uint8_t parameterPoition_;
  static uint8_t parametersCount_;
  static constexpr uint8_t MAX_PARAMETERS = 10;
  static MyParameterBase* parameters_[MAX_PARAMETERS];
  static uint8_t globalParameterPoition_;

  void present_() {
    ::present(sensorId_, sensorType_);
  }
  virtual void set_(const MyMessage &message) = 0;

public:
  MyParameterBase(uint8_t sensorId, uint8_t type, uint8_t sensorType)
    : msg_(sensorId+100, type), sensorId_(sensorId+100), sensorType_(sensorType)
  {
    parameterPoition_ = globalParameterPoition_;
    if (parametersCount_ < MAX_PARAMETERS)
      parameters_[parametersCount_++] = this;
  }
  static void present() {
    for (size_t i=0; i<parametersCount_; i++)
      parameters_[i]->present_();
  }
  static void receive(const MyMessage &message) {
    for (size_t i=0; i<parametersCount_; i++) {
      if (parameters_[i]->sensorId_ == message.sensor) {
        if (message.isAck()) {
          return;
        }
        if (parameters_[i]->msg_.type == message.type) {
          if (message.getCommand() == C_REQ) {
            ::send(parameters_[i]->msg_);
          }
          else if (message.getCommand() == C_SET) {
            parameters_[i]->set_(message);
          }
        }
      }
    }
  }
};

uint8_t MyParameterBase::parametersCount_ = 0;
MyParameterBase* MyParameterBase::parameters_[];
uint8_t MyParameterBase::globalParameterPoition_ = 0;

template <typename ValueType>
class MyParameter : public MyParameterBase {
  static constexpr ValueType erasedValue = static_cast<ValueType>(0xffffffff);

  void save_(ValueType value) {
    uint32_t tmp = value;
    for (uint8_t i=0; i<sizeof(ValueType); i++) {
      saveState(parameterPoition_ + i, (tmp >> i) & 0xff);
    }
  }

  ValueType load_() {
    uint32_t tmp = 0;
    for (uint8_t i=0; i<sizeof(ValueType); i++) {
      tmp |= (static_cast<uint32_t>(loadState(parameterPoition_ + i)) << i);
    }
    return ValueType(tmp);
  }

  void set_(const MyMessage &message) override {
    auto value = getMessageValue_<ValueType>(message);
    if (value == erasedValue) 
      return;
    save_(value);
    setMessageValue_(msg_, value);
  }
public:
  MyParameter(uint8_t sensorId, uint8_t type, uint8_t sensorType, ValueType init = 0)
    : MyParameterBase(sensorId, type, sensorType)
  {
    static_assert(sizeof(ValueType) <= sizeof(uint32_t), "MyParameter may be instantiated only with types not grater than 32 bits");
    globalParameterPoition_ += sizeof(ValueType);
    ValueType eeprom = load_();
    if (eeprom == erasedValue)
      save_(init);
    else
      init = eeprom;
    setMessageValue_(msg_, init);
  }
  ValueType get() {
    return getMessageValue_<ValueType>(msg_);
  }
};

class MyMySensor {
  static constexpr uint8_t MAX_SENSORS = 10;
  static uint8_t sensorsCount_;
  static MyMySensor* sensors_[MAX_SENSORS];
  static PowerManager* powerManager_;
  static uint8_t consecutiveFails_;
  static uint8_t buttonPin_;
  static uint8_t interruptPin_;
  static uint8_t interruptMode_;
  static bool alwaysBoostOn_;
  static const uint8_t N_RETRIES = 14;
  static const unsigned long UPDATE_INTERVAL = 1000;
  static unsigned long now_;
  static unsigned long lastUpdate_;

  virtual void begin_() {};
  virtual unsigned long preUpdate_() {return 0;};
  virtual unsigned long update_() {return SLEEP_TIME;};

  static unsigned long getSleepTimeout_(bool success, unsigned long sleep = 0) {
    if (!success) {
      if (consecutiveFails_ < N_RETRIES) {
        consecutiveFails_++;
      }
    }
    else {
      consecutiveFails_ = 0;
    }
    unsigned long sleepTimeout = consecutiveFails_ ? (1<<(consecutiveFails_-1))*UPDATE_INTERVAL : sleep;
    #ifdef MY_MY_DEBUG
    Serial.print("Sleep: ");
    Serial.println(sleepTimeout);
    wait(500);
    #endif
    return sleepTimeout;
  }

protected:
  // Sleep time between sensor updates (in milliseconds)
  static constexpr unsigned long SLEEP_TIME = 600000;

  void requestInterrupt(uint8_t pin, uint8_t mode) {
    if (interruptPin_ == INTERRUPT_NOT_DEFINED) {
      interruptPin_ = pin;
      interruptMode_ = mode;
    }
  }

public:
  MyMySensor()
  {
    if (sensorsCount_ < MAX_SENSORS)
      sensors_[sensorsCount_++] = this;
  }

  static void present() {
    MyValueBase::present();
    MyParameterBase::present();
  }
  static void begin(uint8_t batteryPin = -1, bool liIonBattery = false, uint8_t powerBoostPin = -1,  bool initialBoostOn = false, bool alwaysBoostOn = false, uint8_t buttonPin = INTERRUPT_NOT_DEFINED) {
    alwaysBoostOn_ = alwaysBoostOn;
    if (buttonPin != uint8_t(-1)) {
      buttonPin_ = buttonPin;
      pinMode(buttonPin_, INPUT_PULLUP);
    }

    pinMode(MY_LED, OUTPUT);
    digitalWrite(MY_LED, LOW);

    powerManager_ = &PowerManager::getInstance();
    powerManager_->setupPowerBoost(powerBoostPin, initialBoostOn or alwaysBoostOn_);
    powerManager_->setBatteryPin(batteryPin, liIonBattery);
    for (size_t i=0; i<sensorsCount_; i++)
      sensors_[i]->begin_();
  }
  static void update() {
    if (not alwaysBoostOn_) {
      powerManager_->turnBoosterOn();
      //wait for everything to setup (100ms for dc/dc converter)
      wait(100);
    }

    unsigned long maxWait = 0;
    for (size_t i=0; i<sensorsCount_; i++) {
      auto wait = sensors_[i]->preUpdate_();
      maxWait = max(maxWait, wait);
    }
    wait(maxWait);

	checkTransport();

    MyValueBase::beforeUpdate();
	unsigned long minWait = -1;
    for (size_t i=0; i<sensorsCount_; i++) {
      auto wait = sensors_[i]->update_();
      minWait = min(minWait, wait);
    }
    bool success = MyValueBase::afterUpdate();

	powerManager_->reportBatteryLevel();
	unsigned long sleepTimeout = getSleepTimeout_(success, minWait);

    digitalWrite(MY_LED, HIGH);
 
    if (not alwaysBoostOn_)
      powerManager_->turnBoosterOff();

    int wakeUpCause;
    if (buttonPin_ == INTERRUPT_NOT_DEFINED)
      wakeUpCause = smartSleep(digitalPinToInterrupt(interruptPin_), interruptMode_, sleepTimeout);
    else
      wakeUpCause = smartSleep(digitalPinToInterrupt(buttonPin_), FALLING, digitalPinToInterrupt(interruptPin_), interruptMode_, sleepTimeout);

    if (buttonPin_ != INTERRUPT_NOT_DEFINED and wakeUpCause == digitalPinToInterrupt(buttonPin_)) {
      digitalWrite(MY_LED, LOW);
      MyValueBase::forceResend();
      #ifdef MY_MY_DEBUG
      Serial.println("Wake up from button");
      #endif
    }
    else if (interruptPin_ != INTERRUPT_NOT_DEFINED and wakeUpCause == digitalPinToInterrupt(interruptPin_)) {
      digitalWrite(MY_LED, LOW);
      #ifdef MY_MY_DEBUG
      Serial.println("Wake up from sensor");
      #endif
    }
  }
  static void receive(const MyMessage &message) {
    MyParameterBase::receive(message);
  }

};

uint8_t MyMySensor::sensorsCount_ = 0;
MyMySensor * MyMySensor::sensors_[];
uint8_t MyMySensor::consecutiveFails_ = 0;
uint8_t MyMySensor::buttonPin_ = INTERRUPT_NOT_DEFINED;
uint8_t MyMySensor::interruptPin_ = INTERRUPT_NOT_DEFINED;
uint8_t MyMySensor::interruptMode_ = MODE_NOT_DEFINED;
bool MyMySensor::alwaysBoostOn_ = false;
PowerManager* MyMySensor::powerManager_ = nullptr;

} // mymysensors

#endif //MyMySensor_h
