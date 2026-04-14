#pragma once
#ifndef DATA_CAPTURE_8_COMPONENT_H
#define DATA_CAPTURE_8_COMPONENT_H

#include <QLabel>

#include "Components/AbstractComponent.h"
#include "Components/ComponentMacro.h"

namespace rabbit_App::component {

COMPONENT_CLASS_DECLARATION(DataCapture8)

/// @brief DataCapture8RawComponent class
/// This class implements the DataCapture8 component.
/// 8-bit output capture component that captures data when CAPTURE signal is high.
class DataCapture8RawComponent : public AbstractRawComponent {
  Q_OBJECT

public:
  DataCapture8RawComponent(QWidget *parent = nullptr);
  virtual ~DataCapture8RawComponent();

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
  QLabel *bin_label_;

  int captured_value_;
  bool has_captured_;
};

} // namespace rabbit_App::component

#endif // DATA_CAPTURE_8_COMPONENT_H
