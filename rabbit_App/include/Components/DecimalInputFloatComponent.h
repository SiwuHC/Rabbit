#pragma once
#ifndef DECIMAL_INPUT_FLOAT_COMPONENT_H
#define DECIMAL_INPUT_FLOAT_COMPONENT_H

#include <QLabel>
#include <QLineEdit>

#include "Components/AbstractComponent.h"
#include "Components/ComponentMacro.h"

namespace rabbit_App::component {

COMPONENT_CLASS_DECLARATION(DecimalInputFloat)

/// @brief DecimalInputFloatRawComponent class
/// This class implements the DecimalInputFloat component.
/// 32-bit float input component that displays decimal value and sends
/// the value to FPGA via 32 output ports.
class DecimalInputFloatRawComponent : public AbstractRawComponent {
  Q_OBJECT

public:
  DecimalInputFloatRawComponent(QWidget *parent = nullptr);
  virtual ~DecimalInputFloatRawComponent();

  void reset() override;

  void processReadData(QQueue<uint64_t> &read_queue) override;
  uint64_t getWriteData() const override;

protected:
  void paintEvent(QPaintEvent *event) override;
  void initPorts() override;
  void initConnections();

private slots:
  void onValueChanged();
  void updateDisplay();

private:
  QLabel *dec_label_;
  QLineEdit *value_edit_;

  float current_value_ = 0.0f;
};

} // namespace rabbit_App::component

#endif // DECIMAL_INPUT_FLOAT_COMPONENT_H
