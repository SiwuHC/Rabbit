#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPainter>
#include <QIntValidator>

#include "Components/ComponentSettingsDialog.h"
#include "Components/AbstractComponent.h"
#include "Components/DecimalInput32Component.h"

using namespace rabbit_App::component;

DecimalInput32RawComponent::DecimalInput32RawComponent(QWidget *parent)
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
  value_edit_->setValidator(new QIntValidator(-2147483648, 2147483647, this));
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

DecimalInput32RawComponent::~DecimalInput32RawComponent() {}

void DecimalInput32RawComponent::initConnections() {
  connect(inc_btn_, &QPushButton::clicked, this,
          &DecimalInput32RawComponent::onIncrement);
  connect(dec_btn_, &QPushButton::clicked, this,
          &DecimalInput32RawComponent::onDecrement);
  connect(value_edit_, &QLineEdit::editingFinished, this,
          &DecimalInput32RawComponent::onValueChanged);
}

void DecimalInput32RawComponent::reset() {
  current_value_ = 0;
  updateDisplay();
}

void DecimalInput32RawComponent::processReadData(
    QQueue<uint64_t> &read_queue) {
  // Input component - no read data processing
  Q_UNUSED(read_queue);
}

uint64_t DecimalInput32RawComponent::getWriteData() const {
  quint64 data = 0;
  for (auto i = 0; i < 32; ++i) {
    // DIN0 is LSB (pin_index 0), DIN31 is MSB (pin_index 31)
    // current_value_'s bit i goes to output port at index i
    data |= (((quint64)current_value_ >> i) & 0x1) << input_ports_[i].pin_index;
  }
  return data;
}

void DecimalInput32RawComponent::paintEvent(QPaintEvent *event) {
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);

  // Draw border
  painter.setPen(Qt::black);
  painter.setBrush(Qt::white);
  painter.drawRect(rect());
}

void DecimalInput32RawComponent::initPorts() {
  // 32 input ports: DIN0 (LSB) to DIN31 (MSB)
  for (auto i = 0; i < 32; ++i) {
    appendPort(input_ports_, QString("DIN%1").arg(i), ports::PortType::Input);
  }
}

void DecimalInput32RawComponent::onIncrement() {
  current_value_++;
  updateDisplay();
}

void DecimalInput32RawComponent::onDecrement() {
  current_value_--;
  updateDisplay();
}

void DecimalInput32RawComponent::onValueChanged() {
  bool ok;
  qint64 value = value_edit_->text().toLongLong(&ok);
  if (ok) {
    current_value_ = value;
    updateDisplay();
  }
}

void DecimalInput32RawComponent::updateDisplay() {
  dec_label_->setText(QString("Dec: %1").arg(current_value_));
  hex_label_->setText(QString("Hex: 0x%1").arg((quint32)current_value_, 8, 16).toUpper());
  value_edit_->setText(QString::number(current_value_));
}

COMPONENT_CLASS_DEFINITION(DecimalInput32, 3, 3)

void DecimalInput32Component::onSettingsBtnClicked() {
  auto dialog =
      new ComponentSettingsDialogWithFeatures<SettingsFeature::ArrayPortMapping>(
          this, this);
  dialog->exec();
  delete dialog;
}
