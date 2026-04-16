#include <QVBoxLayout>
#include <QPainter>
#include <QDoubleValidator>

#include "Components/ComponentSettingsDialog.h"
#include "Components/AbstractComponent.h"
#include "Components/DecimalInputFloatComponent.h"

using namespace rabbit_App::component;

DecimalInputFloatRawComponent::DecimalInputFloatRawComponent(QWidget *parent)
    : AbstractRawComponent(parent) {
  initPorts();

  auto main_layout = new QVBoxLayout(this);
  main_layout->setContentsMargins(5, 5, 5, 5);
  main_layout->setSpacing(2);

  // Display label
  dec_label_ = new QLabel(this);
  dec_label_->setStyleSheet("font-size: 14px; font-weight: bold;");
  main_layout->addWidget(dec_label_);

  // Value input
  value_edit_ = new QLineEdit(this);
  value_edit_->setValidator(new QDoubleValidator(this));
  value_edit_->setAlignment(Qt::AlignCenter);
  value_edit_->setStyleSheet("font-size: 12px;");
  main_layout->addWidget(value_edit_);

  setLayout(main_layout);

  initConnections();
  updateDisplay();
}

DecimalInputFloatRawComponent::~DecimalInputFloatRawComponent() {}

void DecimalInputFloatRawComponent::initConnections() {
  connect(value_edit_, &QLineEdit::editingFinished, this,
          &DecimalInputFloatRawComponent::onValueChanged);
}

void DecimalInputFloatRawComponent::reset() {
  current_value_ = 0.0f;
  updateDisplay();
}

void DecimalInputFloatRawComponent::processReadData(
    QQueue<uint64_t> &read_queue) {
  // Input component - no read data processing
  Q_UNUSED(read_queue);
}

uint64_t DecimalInputFloatRawComponent::getWriteData() const {
  quint32 float_bits;
  memcpy(&float_bits, &current_value_, sizeof(float_bits));
  quint64 data = 0;
  for (auto i = 0; i < 32; ++i) {
    // DIN0 is LSB (pin_index 0), DIN31 is MSB (pin_index 31)
    // float_bits's bit i goes to output port at index i
    data |= ((float_bits >> i) & 0x1) << input_ports_[i].pin_index;
  }
  return data;
}

void DecimalInputFloatRawComponent::paintEvent(QPaintEvent *event) {
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);

  // Draw border
  painter.setPen(Qt::black);
  painter.setBrush(Qt::white);
  painter.drawRect(rect());
}

void DecimalInputFloatRawComponent::initPorts() {
  // 32 input ports: DIN0 (LSB) to DIN31 (MSB)
  for (auto i = 0; i < 32; ++i) {
    appendPort(input_ports_, QString("DIN%1").arg(i), ports::PortType::Input);
  }
}

void DecimalInputFloatRawComponent::onValueChanged() {
  bool ok;
  float value = value_edit_->text().toFloat(&ok);
  if (ok) {
    current_value_ = value;
    updateDisplay();
  }
}

void DecimalInputFloatRawComponent::updateDisplay() {
  dec_label_->setText(QString("Float: %1").arg(current_value_, 0, 'g', 8));
  value_edit_->setText(QString::number(current_value_, 'g', 8));
}

COMPONENT_CLASS_DEFINITION(DecimalInputFloat, 3, 3)

void DecimalInputFloatComponent::onSettingsBtnClicked() {
  auto dialog =
      new ComponentSettingsDialogWithFeatures<SettingsFeature::ArrayPortMapping>(
          this, this);
  dialog->exec();
  delete dialog;
}
