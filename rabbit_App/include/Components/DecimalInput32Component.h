#pragma once
#ifndef DECIMAL_INPUT_32_COMPONENT_H
#define DECIMAL_INPUT_32_COMPONENT_H

#include <QPushButton>
#include <QLabel>
#include <QLineEdit>

#include "Components/AbstractComponent.h"
#include "Components/ComponentMacro.h"

namespace rabbit_App::component {

COMPONENT_CLASS_DECLARATION(DecimalInput32)

/// @brief DecimalInput32RawComponent class
/// This class implements the DecimalInput32 component.
/// 32-bit input component that displays decimal/hex and sends
/// the value to FPGA via 32 output ports.
class DecimalInput32RawComponent : public AbstractRawComponent {
  Q_OBJECT

public:
  DecimalInput32RawComponent(QWidget *parent = nullptr);
  virtual ~DecimalInput32RawComponent();

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

  qint64 current_value_ = 0;
};

} // namespace rabbit_App::component

#endif // DECIMAL_INPUT_32_COMPONENT_H
