#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPainter>
#include <QIntValidator>

#include "Components/ComponentSettingsDialog.h"
#include "Components/AbstractComponent.h"
#include "Components/DecimalInput8Component.h"

using namespace rabbit_App::component;

DecimalInput8RawComponent::DecimalInput8RawComponent(QWidget *parent)
    : AbstractRawComponent(parent) {
  initPorts();

  auto main_layout = new QVBoxLayout(this);
  main_layout->setContentsMargins(5, 5, 5, 5);
  main_layout->setSpacing(2);

  // Display labels
  dec_label_ = new QLabel(this);
  hex_label_ = new QLabel(this);
  bin_label_ = new QLabel(this);

  dec_label_->setStyleSheet("font-size: 14px; font-weight: bold;");
  hex_label_->setStyleSheet("font-size: 12px;");
  bin_label_->setStyleSheet("font-size: 11px;");

  main_layout->addWidget(dec_label_);
  main_layout->addWidget(hex_label_);
  main_layout->addWidget(bin_label_);

  // Value input
  value_edit_ = new QLineEdit(this);
  value_edit_->setValidator(new QIntValidator(0, 255, this));
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

DecimalInput8RawComponent::~DecimalInput8RawComponent() {}

void DecimalInput8RawComponent::initConnections() {
  connect(inc_btn_, &QPushButton::clicked, this,
          &DecimalInput8RawComponent::onIncrement);
  connect(dec_btn_, &QPushButton::clicked, this,
          &DecimalInput8RawComponent::onDecrement);
  connect(value_edit_, &QLineEdit::editingFinished, this,
          &DecimalInput8RawComponent::onValueChanged);
}

void DecimalInput8RawComponent::reset() {
  current_value_ = 0;
  updateDisplay();
}

void DecimalInput8RawComponent::processReadData(
    QQueue<uint64_t> &read_queue) {
  // Input component - no read data processing
  Q_UNUSED(read_queue);
}

uint64_t DecimalInput8RawComponent::getWriteData() const {
  uint64_t data = 0;
  for (auto i = 0; i < 8; ++i) {
    // DIN0 is LSB (pin_index 0), DIN7 is MSB (pin_index 7)
    // current_value_'s bit i goes to output port at index i
    data |= ((current_value_ >> i) & 0x1) << input_ports_[i].pin_index;
  }
  return data;
}

void DecimalInput8RawComponent::paintEvent(QPaintEvent *event) {
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);

  // Draw border
  painter.setPen(Qt::black);
  painter.setBrush(Qt::white);
  painter.drawRect(rect());
}

void DecimalInput8RawComponent::initPorts() {
  // 8 input ports: DIN0 (LSB) to DIN7 (MSB)
  appendPort(input_ports_, "DIN0", ports::PortType::Input);
  appendPort(input_ports_, "DIN1", ports::PortType::Input);
  appendPort(input_ports_, "DIN2", ports::PortType::Input);
  appendPort(input_ports_, "DIN3", ports::PortType::Input);
  appendPort(input_ports_, "DIN4", ports::PortType::Input);
  appendPort(input_ports_, "DIN5", ports::PortType::Input);
  appendPort(input_ports_, "DIN6", ports::PortType::Input);
  appendPort(input_ports_, "DIN7", ports::PortType::Input);
}

void DecimalInput8RawComponent::onIncrement() {
  current_value_ = (current_value_ + 1) & 0xFF; // Wrap at 255
  updateDisplay();
}

void DecimalInput8RawComponent::onDecrement() {
  current_value_ = (current_value_ - 1) & 0xFF; // Wrap at 0
  updateDisplay();
}

void DecimalInput8RawComponent::onValueChanged() {
  bool ok;
  int value = value_edit_->text().toInt(&ok);
  if (ok) {
    current_value_ = value & 0xFF;
    updateDisplay();
  }
}

void DecimalInput8RawComponent::updateDisplay() {
  dec_label_->setText(QString("Dec: %1").arg(current_value_));
  hex_label_->setText(QString("Hex: 0x%1").arg(current_value_, 2, 16).toUpper());
  bin_label_->setText(
      QString("Bin: %1").arg(current_value_, 8, 2, QChar('0')));
  value_edit_->setText(QString::number(current_value_));
}

COMPONENT_CLASS_DEFINITION(DecimalInput8, 3, 3)

void DecimalInput8Component::onSettingsBtnClicked() {
  ComponentSettingsDialog *dialog = new ComponentSettingsDialog(this, this);
  dialog->exec();
  delete dialog;
}
