#include <QVBoxLayout>
#include <QPainter>

#include "Components/ComponentSettingsDialog.h"
#include "Components/AbstractComponent.h"
#include "Components/DataCapture8Component.h"

using namespace rabbit_App::component;

DataCapture8RawComponent::DataCapture8RawComponent(QWidget *parent)
    : AbstractRawComponent(parent), captured_value_(0), has_captured_(false) {
  initPorts();

  auto main_layout = new QVBoxLayout(this);
  main_layout->setContentsMargins(5, 5, 5, 5);
  main_layout->setSpacing(2);

  status_label_ = new QLabel("Status: Waiting...", this);
  dec_label_ = new QLabel("Dec: --", this);
  hex_label_ = new QLabel("Hex: --", this);
  bin_label_ = new QLabel("Bin: --------", this);

  status_label_->setStyleSheet("font-size: 12px; font-weight: bold;");
  dec_label_->setStyleSheet("font-size: 12px;");
  hex_label_->setStyleSheet("font-size: 11px;");
  bin_label_->setStyleSheet("font-size: 10px;");

  main_layout->addWidget(status_label_);
  main_layout->addWidget(dec_label_);
  main_layout->addWidget(hex_label_);
  main_layout->addWidget(bin_label_);

  setLayout(main_layout);
}

DataCapture8RawComponent::~DataCapture8RawComponent() {}

void DataCapture8RawComponent::reset() {
  captured_value_ = 0;
  has_captured_ = false;
  status_label_->setText("Status: Waiting...");
  dec_label_->setText("Dec: --");
  hex_label_->setText("Hex: --");
  bin_label_->setText("Bin: --------");
}

void DataCapture8RawComponent::processReadData(QQueue<uint64_t> &read_queue) {
  if (read_queue.isEmpty()) {
    return;
  }

  // First port (index 0) is CAPTURE signal
  // Next 8 ports (index 1-8) are DOUT0-DOUT7 data bits
  auto capture_port_index = output_ports_[0].pin_index - 1; // -1 because index 0 is clock

  // Check the most recent sample for capture signal
  auto latest_value = read_queue.back();

  // Check if capture signal is high
  bool capture_high = (latest_value >> capture_port_index) & 0x1;

  if (capture_high) {
    // Capture the 8-bit data
    int data = 0;
    for (auto i = 0; i < 8; ++i) {
      auto port_index = output_ports_[i + 1].pin_index - 1;
      data |= ((latest_value >> port_index) & 0x1) << i;
    }
    captured_value_ = data;
    has_captured_ = true;
    updateDisplay();
  }
}

uint64_t DataCapture8RawComponent::getWriteData() const {
  return 0; // Output-only component
}

void DataCapture8RawComponent::paintEvent(QPaintEvent *event) {
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);

  // Draw border
  painter.setPen(Qt::black);
  painter.setBrush(Qt::white);
  painter.drawRect(rect());
}

void DataCapture8RawComponent::initPorts() {
  // First port is CAPTURE signal
  appendPort(output_ports_, "CAPTURE", ports::PortType::Output);
  // Next 8 ports are data bits: DOUT0 (LSB) to DOUT7 (MSB)
  appendPort(output_ports_, "DOUT0", ports::PortType::Output);
  appendPort(output_ports_, "DOUT1", ports::PortType::Output);
  appendPort(output_ports_, "DOUT2", ports::PortType::Output);
  appendPort(output_ports_, "DOUT3", ports::PortType::Output);
  appendPort(output_ports_, "DOUT4", ports::PortType::Output);
  appendPort(output_ports_, "DOUT5", ports::PortType::Output);
  appendPort(output_ports_, "DOUT6", ports::PortType::Output);
  appendPort(output_ports_, "DOUT7", ports::PortType::Output);
}

void DataCapture8RawComponent::updateDisplay() {
  status_label_->setText("Status: Captured");
  dec_label_->setText(QString("Dec: %1").arg(captured_value_));
  hex_label_->setText(
      QString("Hex: 0x%1").arg(captured_value_, 2, 16).toUpper());
  bin_label_->setText(
      QString("Bin: %1").arg(captured_value_, 8, 2, QChar('0')));
}

COMPONENT_CLASS_DEFINITION(DataCapture8, 3, 3)

void DataCapture8Component::onSettingsBtnClicked() {
  ComponentSettingsDialog *dialog = new ComponentSettingsDialog(this, this);
  dialog->exec();
  delete dialog;
}
