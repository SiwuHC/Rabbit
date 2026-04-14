#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPainter>
#include <QIntValidator>

#include "Components/DataCaptureSettingsDialog.h"
#include "Components/AbstractComponent.h"
#include "Components/DecimalInput16Component.h"

using namespace rabbit_App::component;

DecimalInput16RawComponent::DecimalInput16RawComponent(QWidget *parent)
    : AbstractRawComponent(parent) {
  initPorts();

  auto main_layout = new QVBoxLayout(this);
  main_layout->setContentsMargins(5, 5, 5, 5);
  main_layout->setSpacing(2);

  // Display labels
  dec_label_ = new QLabel(this);
  hex_label_ = new QLabel(this);

  dec_label_->setStyleSheet("font-size: 14px; font-weight: bold;");
  hex_label_->setStyleSheet("font-size: 12px;");

  main_layout->addWidget(dec_label_);
  main_layout->addWidget(hex_label_);

  // Value input
  value_edit_ = new QLineEdit(this);
  value_edit_->setValidator(new QIntValidator(-32768, 65535, this));
  value_edit_->setAlignment(Qt::AlignCenter);
  value_edit_->setStyleSheet("font-size: 12px;");
  main_layout->addWidget(value_edit_);

  // Buttons layout
  auto btn_layout = new QHBoxLayout();
  btn_layout->setSpacing(5);

  dec_btn_ = new QPushButton("-", this);
  inc_btn_ = new QPushButton("+", this);
  dec_btn_->setMinimumHeight(25);
  inc_btn_->setMinimumHeight(25);

  btn_layout->addWidget(dec_btn_);
  btn_layout->addWidget(inc_btn_);

  main_layout->addLayout(btn_layout);

  setLayout(main_layout);

  initConnections();
  updateDisplay();
}

DecimalInput16RawComponent::~DecimalInput16RawComponent() {}

void DecimalInput16RawComponent::initConnections() {
  connect(inc_btn_, &QPushButton::clicked, this,
          &DecimalInput16RawComponent::onIncrement);
  connect(dec_btn_, &QPushButton::clicked, this,
          &DecimalInput16RawComponent::onDecrement);
  connect(value_edit_, &QLineEdit::editingFinished, this,
          &DecimalInput16RawComponent::onValueChanged);
}

void DecimalInput16RawComponent::reset() {
  current_value_ = 0;
  updateDisplay();
}

void DecimalInput16RawComponent::processReadData(
    QQueue<uint64_t> &read_queue) {
  // Input component - no read data processing
  Q_UNUSED(read_queue);
}

uint64_t DecimalInput16RawComponent::getWriteData() const {
  uint64_t data = 0;
  for (auto i = 0; i < 16; ++i) {
    // DIN0 is LSB (pin_index 0), DIN15 is MSB (pin_index 15)
    // current_value_'s bit i goes to output port at index i
    data |= ((current_value_ >> i) & 0x1) << input_ports_[i].pin_index;
  }
  return data;
}

void DecimalInput16RawComponent::paintEvent(QPaintEvent *event) {
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);

  // Draw border
  painter.setPen(Qt::black);
  painter.setBrush(Qt::white);
  painter.drawRect(rect());
}

void DecimalInput16RawComponent::initPorts() {
  // 16 input ports: DIN0 (LSB) to DIN15 (MSB)
  appendPort(input_ports_, "DIN0", ports::PortType::Input);
  appendPort(input_ports_, "DIN1", ports::PortType::Input);
  appendPort(input_ports_, "DIN2", ports::PortType::Input);
  appendPort(input_ports_, "DIN3", ports::PortType::Input);
  appendPort(input_ports_, "DIN4", ports::PortType::Input);
  appendPort(input_ports_, "DIN5", ports::PortType::Input);
  appendPort(input_ports_, "DIN6", ports::PortType::Input);
  appendPort(input_ports_, "DIN7", ports::PortType::Input);
  appendPort(input_ports_, "DIN8", ports::PortType::Input);
  appendPort(input_ports_, "DIN9", ports::PortType::Input);
  appendPort(input_ports_, "DIN10", ports::PortType::Input);
  appendPort(input_ports_, "DIN11", ports::PortType::Input);
  appendPort(input_ports_, "DIN12", ports::PortType::Input);
  appendPort(input_ports_, "DIN13", ports::PortType::Input);
  appendPort(input_ports_, "DIN14", ports::PortType::Input);
  appendPort(input_ports_, "DIN15", ports::PortType::Input);
}

void DecimalInput16RawComponent::onIncrement() {
  current_value_ = (current_value_ + 1) & 0xFFFF; // Wrap at 65535
  updateDisplay();
}

void DecimalInput16RawComponent::onDecrement() {
  current_value_ = (current_value_ - 1) & 0xFFFF; // Wrap at 0
  updateDisplay();
}

void DecimalInput16RawComponent::onValueChanged() {
  bool ok;
  int value = value_edit_->text().toInt(&ok);
  if (ok) {
    current_value_ = value & 0xFFFF;
    updateDisplay();
  }
}

void DecimalInput16RawComponent::updateDisplay() {
  dec_label_->setText(QString("Dec: %1").arg(current_value_));
  hex_label_->setText(QString("Hex: 0x%1").arg(current_value_, 4, 16).toUpper());
  value_edit_->setText(QString::number(current_value_));
}

COMPONENT_CLASS_DEFINITION(DecimalInput16, 3, 3)

void DecimalInput16Component::onSettingsBtnClicked() {
  DataCaptureSettingsDialog *dialog = new DataCaptureSettingsDialog(this, this);
  dialog->exec();
  delete dialog;
}
