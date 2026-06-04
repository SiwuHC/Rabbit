#pragma once
#ifndef STREAM_INPUT_FLOAT_COMPONENT_H
#define STREAM_INPUT_FLOAT_COMPONENT_H

#include <QLineEdit>
#include <QListWidget>
#include <QLabel>
#include <QQueue>

#include "Components/AbstractComponent.h"
#include "Components/ComponentMacro.h"

namespace rabbit_App::component {

COMPONENT_CLASS_DECLARATION(StreamInputFloat)

class StreamInputFloatRawComponent : public AbstractRawComponent {
  Q_OBJECT

public:
  StreamInputFloatRawComponent(QWidget *parent = nullptr);
  virtual ~StreamInputFloatRawComponent();

  void reset() override;
  void processReadData(QQueue<uint64_t> &read_queue) override;
  uint64_t getWriteData() const override;

  QList<NumberSettingInfo> numberSettings() const override;
  void setNumberSetting(const QString &key, int value) override;

protected:
  void paintEvent(QPaintEvent *event) override;
  void initPorts() override;
  void initConnections();

private slots:
  void onEnterPressed();

private:
  void updateCounter() const;

  QLineEdit *input_edit_;
  QLabel *counter_label_;
  QListWidget *sent_list_;

  mutable QQueue<float> value_queue_;
  mutable int total_sent_ = 0;
  int target_count_ = 32;
  int clk_hold_ = 20;

  enum WordPhase { IDLE, SETUP, RISING, FALLING };
  mutable WordPhase phase_ = IDLE;
  mutable int hold_cnt_ = 0;
  mutable uint32_t current_val_ = 0;  // raw bits of float
  mutable uint64_t last_output_ = 0;
};

} // namespace rabbit_App::component
#endif
