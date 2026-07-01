#include "Components/ComponentAction.h"
#include "Components/Components.h"
#include "Components/ComponentsFactory.h"

using namespace rabbit_App::component;

ComponentAction *ComponentActionFactory::create(const QString &text,
                                                QObject *parent) {
  auto action = new ComponentAction(parent);
  action->setText(text);

  action->component_factory_ = [=]() {
    return ComponentsFactory::create(text);
  };

  if (text == "LED") {
    action->setIcon(LEDComponent::componentIcon());
  } else if (text == "TextLCD") {
    action->setIcon(TextLCDComponent::componentIcon());
  } else if (text == "Switch") {
    action->setIcon(SwitchComponent::componentIcon());
  } else if (text == "Button") {
    action->setIcon(ButtonComponent::componentIcon());
  } else if (text == "SegmentDisplay") {
    action->setIcon(SegmentDisplayComponent::componentIcon());
  } else if (text == "FourDigitSegmentDisplay") {
    action->setIcon(FourDigitSegmentDisplayComponent::componentIcon());
  } else if (text == "LED4x4Matrix") {
    action->setIcon(LED4x4MatrixComponent::componentIcon());
  } else if (text == "LED8x8Matrix") {
    action->setIcon(LED8x8MatrixComponent::componentIcon());
  } else if (text == "LED16x16Matrix") {
    action->setIcon(LED16x16MatrixComponent::componentIcon());
  } else if (text == "KeyPad") {
    action->setIcon(KeyPadComponent::componentIcon());
  } else if (text == "SmallKeyPad") {
    action->setIcon(SmallKeyPadComponent::componentIcon());
  } else if (text == "GraphicLCD") {
    action->setIcon(GraphicLCDComponent::componentIcon());
  } else if (text == "RotaryButton") {
    action->setIcon(RotaryButtonComponent::componentIcon());
    // add new component icon on toolbar here :
    // } else if (text == "New Component Name") {
    //   action->setIcon(NewComponentClass::componentIcon());
  } else if(text == "PS2Keyboard"){
    action->setIcon(PS2KeyboardComponent::componentIcon());
  } else if(text == "NewKeyboard"){
    action->setIcon(NewKeyboardComponent::componentIcon());
  } else if(text == "DecimalInput8"){
    action->setIcon(DecimalInput8Component::componentIcon());
  } else if(text == "DataCapture8"){
    action->setIcon(DataCapture8Component::componentIcon());
  } else if(text == "DecimalInput16"){
    action->setIcon(DecimalInput16Component::componentIcon());
  } else if(text == "DataCapture16"){
    action->setIcon(DataCapture16Component::componentIcon());
  } else if(text == "DecimalInput32"){
    action->setIcon(DecimalInput32Component::componentIcon());
  } else if(text == "DataCapture32"){
    action->setIcon(DataCapture32Component::componentIcon());
  } else if(text == "DecimalInputFloat"){
    action->setIcon(DecimalInputFloatComponent::componentIcon());
  } else if(text == "DataCaptureFloat"){
    action->setIcon(DataCaptureFloatComponent::componentIcon());
  } else if(text == "StreamInput8"){
    action->setIcon(StreamInput8Component::componentIcon());
  } else if(text == "StreamInput16"){
    action->setIcon(StreamInput16Component::componentIcon());
  } else if(text == "StreamInput32"){
    action->setIcon(StreamInput32Component::componentIcon());
  } else if(text == "StreamInputFloat"){
    action->setIcon(StreamInputFloatComponent::componentIcon());
  } else if(text == "StreamOutput8"){
    action->setIcon(StreamOutput8Component::componentIcon());
  } else if(text == "StreamOutput16"){
    action->setIcon(StreamOutput16Component::componentIcon());
  } else if(text == "StreamOutput32"){
    action->setIcon(StreamOutput32Component::componentIcon());
  } else if(text == "StreamOutputFloat"){
    action->setIcon(StreamOutputFloatComponent::componentIcon());
  } else {
    std::runtime_error("Unknown component type");
  }

  return action;
}
