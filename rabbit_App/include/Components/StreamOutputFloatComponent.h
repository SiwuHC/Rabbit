#pragma once
#ifndef STREAM_OUTPUT_FLOAT_COMPONENT_H
#define STREAM_OUTPUT_FLOAT_COMPONENT_H

#include <QListWidget>
#include <QLabel>
#include <QPushButton>

#include "Components/AbstractComponent.h"
#include "Components/ComponentMacro.h"

namespace rabbit_App::component {

COMPONENT_CLASS_DECLARATION(StreamOutputFloat)

class StreamOutputFloatRawComponent : public AbstractRawComponent {
  Q_OBJECT

public:
  StreamOutputFloatRawComponent(QWidget *parent = nullptr);
  virtual ~StreamOutputFloatRawComponent();

  void reset() override;
  void processReadData(QQueue<uint64_t> &read_queue) override;
  uint64_t getWriteData() const override;

  QList<NumberSettingInfo> numberSettings() const override;
  void setNumberSetting(const QString &key, int value) override;

protected:
  void paintEvent(QPaintEvent *event) override;
  void initPorts() override;

private slots:
  void toggleGridMode();
  void onClear();

private:
  void updateDisplay();
  void addValueToList(float val);
  void rebuildGridDisplay();

  QLabel *status_label_;
  QListWidget *output_list_;
  QPushButton *grid_btn_;
  QPushButton *clear_btn_;

  bool grid_mode_ = false;
  int grid_rows_ = 4;
  int grid_cols_ = 4;
  int received_count_ = 0;
  int prev_clk_ = 0;
};

} // namespace rabbit_App::component
#endif
