#pragma once
#ifndef DATA_CAPTURE_FLOAT_COMPONENT_H
#define DATA_CAPTURE_FLOAT_COMPONENT_H

#include <QLabel>

#include "Components/AbstractComponent.h"
#include "Components/ComponentMacro.h"

namespace rabbit_App::component {

COMPONENT_CLASS_DECLARATION(DataCaptureFloat)

/// @brief DataCaptureFloatRawComponent class
/// This class implements the DataCaptureFloat component.
/// 32-bit float output capture component that captures data when CAPTURE signal is high.
class DataCaptureFloatRawComponent : public AbstractRawComponent {
  Q_OBJECT

public:
  DataCaptureFloatRawComponent(QWidget *parent = nullptr);
  virtual ~DataCaptureFloatRawComponent();

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

  float captured_value_;
  bool has_captured_;
};

} // namespace rabbit_App::component

#endif // DATA_CAPTURE_FLOAT_COMPONENT_H
