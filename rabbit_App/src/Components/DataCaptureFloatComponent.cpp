#include <QVBoxLayout>
#include <QPainter>

#include "Components/DataCaptureSettingsDialog.h"
#include "Components/AbstractComponent.h"
#include "Components/DataCaptureFloatComponent.h"

using namespace rabbit_App::component;

DataCaptureFloatRawComponent::DataCaptureFloatRawComponent(QWidget *parent)
    : AbstractRawComponent(parent), captured_value_(0.0f), has_captured_(false) {
  initPorts();

  auto main_layout = new QVBoxLayout(this);
  main_layout->setContentsMargins(5, 5, 5, 5);
  main_layout->setSpacing(2);

  status_label_ = new QLabel("Status: Waiting...", this);
  dec_label_ = new QLabel("Float: --", this);

  status_label_->setStyleSheet("font-size: 12px; font-weight: bold;");
  dec_label_->setStyleSheet("font-size: 14px; font-weight: bold;");

  main_layout->addWidget(status_label_);
  main_layout->addWidget(dec_label_);

  setLayout(main_layout);
}

DataCaptureFloatRawComponent::~DataCaptureFloatRawComponent() {}

void DataCaptureFloatRawComponent::reset() {
  captured_value_ = 0.0f;
  has_captured_ = false;
  status_label_->setText("Status: Waiting...");
  dec_label_->setText("Float: --");
}

void DataCaptureFloatRawComponent::processReadData(QQueue<uint64_t> &read_queue) {
  if (read_queue.isEmpty()) {
    return;
  }

  // First port (index 0) is CAPTURE signal
  // Next 32 ports (index 1-32) are DOUT0-DOUT31 data bits (float bits)
  auto capture_port_index = output_ports_[0].pin_index - 1; // -1 because index 0 is clock

  // Check the most recent sample for capture signal
  auto latest_value = read_queue.back();

  // Check if capture signal is high
  bool capture_high = (latest_value >> capture_port_index) & 0x1;

  if (capture_high) {
    // Capture the 32-bit float data
    quint32 data = 0;
    for (auto i = 0; i < 32; ++i) {
      auto port_index = output_ports_[i + 1].pin_index - 1;
      data |= (((quint64)latest_value >> port_index) & 0x1) << i;
    }
    memcpy(&captured_value_, &data, sizeof(captured_value_));
    has_captured_ = true;
    updateDisplay();
  }
}

uint64_t DataCaptureFloatRawComponent::getWriteData() const {
  return 0; // Output-only component
}

void DataCaptureFloatRawComponent::paintEvent(QPaintEvent *event) {
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);

  // Draw border
  painter.setPen(Qt::black);
  painter.setBrush(Qt::white);
  painter.drawRect(rect());
}

void DataCaptureFloatRawComponent::initPorts() {
  // First port is CAPTURE signal
  appendPort(output_ports_, "CAPTURE", ports::PortType::Output);
  // Next 32 ports are data bits: DOUT0 (LSB) to DOUT31 (MSB)
  for (auto i = 0; i < 32; ++i) {
    appendPort(output_ports_, QString("DOUT%1").arg(i), ports::PortType::Output);
  }
}

void DataCaptureFloatRawComponent::updateDisplay() {
  status_label_->setText("Status: Captured");
  dec_label_->setText(QString("Float: %1").arg(captured_value_, 0, 'g', 8));
}

COMPONENT_CLASS_DEFINITION(DataCaptureFloat, 3, 3)

void DataCaptureFloatComponent::onSettingsBtnClicked() {
  DataCaptureSettingsDialog *dialog = new DataCaptureSettingsDialog(this, this);
  dialog->exec();
  delete dialog;
}
