#ifndef MySceneController_h
#define MySceneController_h

#include "MyMySensorsBase.h"
#include "Switch.h"

namespace mymysensors {

class MySceneController : public MyMySensorsBase
{
  enum State {
    WAITING_FOR_RISING,
    WAITING_FOR_SCENE,
    WAITING_FOR_FALLING
  };
  State state_;
  bool prevSwState_;
  MyDuration lastSwRiseTime_;
  Switch &sw_;
  MyMyMessage sceneMsg_;
  bool enableShortPress_;
  bool isRising_(bool swState) {
    return swState == true and prevSwState_ == false;
  }
  bool isHeldLongEnough_(bool swState) {
    return swState == true and prevSwState_ == true and lastSwRiseTime_ + MyDuration(1000) < MyDuration();
  }
  bool isFalling(bool swState) {
    return swState == false and prevSwState_ == true;
  }
  void sendScene_(uint8_t scene) {
    sceneMsg_.send(scene);
    #ifdef MY_MY_DEBUG
    Serial.print("sendScene_ ");
    Serial.print(scene);
    Serial.print(" for child id ");
    Serial.println(sceneMsg_.getMyMessage().sensor);
    #endif
  }
  void update_() override {
    bool currSwState = sw_.update();
    if (isRising_(currSwState)) {
      lastSwRiseTime_ = MyDuration();
      state_ = WAITING_FOR_SCENE;
    }
    else if (state_ == WAITING_FOR_SCENE and isHeldLongEnough_(currSwState)) {
      state_ = WAITING_FOR_FALLING;
      sendScene_(1);
    }
    else if (isFalling(currSwState)) {
      if (state_ == WAITING_FOR_SCENE) {
        if (enableShortPress_) {
          sendScene_(0);
        }
      }
      state_ = WAITING_FOR_RISING;
    }
    prevSwState_ = currSwState;
  }
public:
  MySceneController(uint8_t sensorId, Switch &sw, bool enableShortPress=true)
    : MyMySensorsBase(sensorId, S_SCENE_CONTROLLER),
      state_(WAITING_FOR_RISING),
      prevSwState_(false),
      sw_(sw),
      sceneMsg_(sensorId, V_SCENE_ON),
      enableShortPress_(enableShortPress) {}
};

} // mymysensors

#endif //MySceneController_h
