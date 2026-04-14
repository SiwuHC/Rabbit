#pragma once
#ifndef DECIMAL_INPUT_16_COMPONENT_H
#define DECIMAL_INPUT_16_COMPONENT_H

#include <QPushButton>
#include <QLabel>
#include <QLineEdit>

#include "Components/AbstractComponent.h"
#include "Components/ComponentMacro.h"

namespace rabbit_App::component {

COMPONENT_CLASS_DECLARATION(DecimalInput16)

/// @brief DecimalInput16RawComponent class
/// This class implements the DecimalInput16 component.
/// 16-bit input component that displays decimal/hex and sends
/// the value to FPGA via 16 output ports.
class DecimalInput16RawComponent : public AbstractRawComponent {
  Q_OBJECT

public:
  DecimalInput16RawComponent(QWidget *parent = nullptr);
  virtual ~DecimalInput16RawComponent();

  void reset() override;

  void processReadData(QQueue<uint64_t> &read_queue) override;
  uint64_t getWriteData() const override;

protected:
  void paintEvent(QPaintEvent *event) override;
  void initPorts() override;
  void initConnections();

private slots:
  void onIncrement();
  void onDecrement();
  void onValueChanged();
  void updateDisplay();

private:
  QLabel *dec_label_;
  QLabel *hex_label_;
  QLineEdit *value_edit_;
  QPushButton *inc_btn_;
  QPushButton *dec_btn_;

  int current_value_ = 0;
};

} // namespace rabbit_App::component

#endif // DECIMAL_INPUT_16_COMPONENT_H
