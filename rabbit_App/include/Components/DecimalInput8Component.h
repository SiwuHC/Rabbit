#pragma once
#ifndef DECIMAL_INPUT_8_COMPONENT_H
#define DECIMAL_INPUT_8_COMPONENT_H

#include <QPushButton>
#include <QLabel>
#include <QLineEdit>

#include "Components/AbstractComponent.h"
#include "Components/ComponentMacro.h"

namespace rabbit_App::component {

COMPONENT_CLASS_DECLARATION(DecimalInput8)

/// @brief DecimalInput8RawComponent class
/// This class implements the DecimalInput8 component.
/// 8-bit input component that displays decimal/hex/binary and sends
/// the value to FPGA via 8 output ports.
class DecimalInput8RawComponent : public AbstractRawComponent {
  Q_OBJECT

public:
  DecimalInput8RawComponent(QWidget *parent = nullptr);
  virtual ~DecimalInput8RawComponent();

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
  QLabel *bin_label_;
  QLineEdit *value_edit_;
  QPushButton *inc_btn_;
  QPushButton *dec_btn_;

  int current_value_ = 0;
};

} // namespace rabbit_App::component

#endif // DECIMAL_INPUT_8_COMPONENT_H
