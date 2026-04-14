#pragma once
#ifndef DATA_CAPTURE_16_COMPONENT_H
#define DATA_CAPTURE_16_COMPONENT_H

#include <QLabel>

#include "Components/AbstractComponent.h"
#include "Components/ComponentMacro.h"

namespace rabbit_App::component {

COMPONENT_CLASS_DECLARATION(DataCapture16)

/// @brief DataCapture16RawComponent class
/// This class implements the DataCapture16 component.
/// 16-bit output capture component that captures data when CAPTURE signal is high.
class DataCapture16RawComponent : public AbstractRawComponent {
  Q_OBJECT

public:
  DataCapture16RawComponent(QWidget *parent = nullptr);
  virtual ~DataCapture16RawComponent();

  void reset() override;

  void processReadData(QQueue<uint64_t> &read_queue) override;
  uint64_t getWriteData() const override;

protected:
  void paintEvent(QPaintEvent *event) override;
  void initPorts() override;

private:
  void updateDisplay();

private:
  QLabel *status_label_;
  QLabel *dec_label_;
  QLabel *hex_label_;

  int captured_value_;
  bool has_captured_;
};

} // namespace rabbit_App::component

#endif // DATA_CAPTURE_16_COMPONENT_H
