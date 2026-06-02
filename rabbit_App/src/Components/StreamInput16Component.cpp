#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPainter>
#include <QIntValidator>

#include "Components/ComponentSettingsDialog.h"
#include "Components/AbstractComponent.h"
#include "Components/StreamInput16Component.h"

using namespace rabbit_App::component;

// Number of USB frames to hold each CLK/STROBE level so the FPGA's
// 3-stage synchronizer has time to settle.
static constexpr int kClkHold = 20;
static constexpr int kDataWidth = 16;

StreamInput16RawComponent::StreamInput16RawComponent(QWidget *parent)
    : AbstractRawComponent(parent) {
  initPorts();

  auto main_layout = new QVBoxLayout(this);
  main_layout->setContentsMargins(5, 5, 5, 5);
  main_layout->setSpacing(2);

  auto input_layout = new QHBoxLayout();
  input_edit_ = new QLineEdit(this);
  input_edit_->setValidator(new QIntValidator(0, 65535, this));
  input_edit_->setPlaceholderText("Enter 16-bit value (0-65535)");
  input_edit_->setStyleSheet("font-size: 12px;");
  input_layout->addWidget(input_edit_, 3);
  main_layout->addLayout(input_layout);

  counter_label_ = new QLabel("Sent: 0 / 32", this);
  counter_label_->setStyleSheet("font-size: 11px;");
  main_layout->addWidget(counter_label_);

  sent_list_ = new QListWidget(this);
  sent_list_->setStyleSheet("font-size: 10px;");
  sent_list_->setMaximumHeight(120);
  main_layout->addWidget(sent_list_);

  setLayout(main_layout);
  initConnections();
}

StreamInput16RawComponent::~StreamInput16RawComponent() {}

void StreamInput16RawComponent::initConnections() {
  connect(input_edit_, &QLineEdit::returnPressed, this,
          &StreamInput16RawComponent::onEnterPressed);
}

void StreamInput16RawComponent::reset() {
  value_queue_.clear();
  total_sent_ = 0;
  phase_ = IDLE;
  hold_cnt_ = 0;
  current_val_ = 0;
  last_output_ = 0;
  sent_list_->clear();
  updateCounter();
}

void StreamInput16RawComponent::processReadData(QQueue<uint64_t> &read_queue) {
  Q_UNUSED(read_queue);
}

uint64_t StreamInput16RawComponent::getWriteData() const {
  const auto &ports = input_ports_;
  // ports[0..15] = DATA[0..15], ports[16] = CLK, ports[17] = STROBE

  // If we've sent enough words, stop.
  if (total_sent_ >= target_count_) {
    return 0;
  }

  // Grab the next word if we're idle.
  if (phase_ == IDLE) {
    if (value_queue_.isEmpty()) {
      return 0;
    }
    current_val_ = value_queue_.dequeue();
    phase_ = SETUP;
    hold_cnt_ = 0;
  }

  // Hold the current phase for kClkHold frames.
  hold_cnt_++;
  if (hold_cnt_ < kClkHold) {
    return last_output_;
  }
  hold_cnt_ = 0;

  uint64_t data = 0;
  int clk_bit = 0, strobe_bit = 0;

  switch (phase_) {
    case SETUP:
      // CLK=0, STROBE=1, DATA=current_val_
      clk_bit = 0;
      strobe_bit = 1;
      phase_ = RISING;
      break;

    case RISING:
      // CLK=1, STROBE=1, DATA=current_val_  (rising edge)
      clk_bit = 1;
      strobe_bit = 1;
      phase_ = FALLING;
      break;

    case FALLING:
      // CLK=0, STROBE=0, DATA=current_val_  (falling edge / cleanup)
      clk_bit = 0;
      strobe_bit = 0;
      // Word complete — record it.
      total_sent_++;
      sent_list_->insertItem(0, QString("#%1: 0x%2 (%3)")
          .arg(total_sent_ - 1)
          .arg(current_val_, 4, 16, QChar('0')).toUpper()
          .arg(current_val_));
      while (sent_list_->count() > 100)
        delete sent_list_->takeItem(sent_list_->count() - 1);
      updateCounter();
      // Check if we've hit the target.
      if (total_sent_ >= target_count_) {
        phase_ = IDLE;
        last_output_ = 0;
        return 0;
      }
      // Grab next word if available.
      if (!value_queue_.isEmpty()) {
        current_val_ = value_queue_.dequeue();
        phase_ = SETUP;
        clk_bit = 0;
        strobe_bit = 1;
      } else {
        phase_ = IDLE;
      }
      break;

    default:
      phase_ = IDLE;
      last_output_ = 0;
      return 0;
  }

  // Build output: pack DATA[15:0], CLK, STROBE onto their pins.
  for (int i = 0; i < kDataWidth; i++) {
    int bit = (current_val_ >> i) & 1;
    data |= (static_cast<uint64_t>(bit) << ports[i].pin_index);
  }
  data |= (static_cast<uint64_t>(clk_bit) << ports[16].pin_index);
  data |= (static_cast<uint64_t>(strobe_bit) << ports[17].pin_index);

  last_output_ = data;
  return data;
}

void StreamInput16RawComponent::paintEvent(QPaintEvent *event) {
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);
  painter.setPen(Qt::black);
  painter.setBrush(Qt::white);
  painter.drawRect(rect());
}

void StreamInput16RawComponent::initPorts() {
  // 18 ports: DATA[15:0] + CLK + STROBE
  for (int i = 0; i < kDataWidth; i++) {
    appendPort(input_ports_,
               QString("DATA[%1]").arg(i),
               ports::PortType::Input);
  }
  appendPort(input_ports_, "CLK", ports::PortType::Input);
  appendPort(input_ports_, "STROBE", ports::PortType::Input);
}

void StreamInput16RawComponent::onEnterPressed() {
  bool ok;
  int value = input_edit_->text().toInt(&ok);
  if (ok && value >= 0 && value <= 65535) {
    value_queue_.enqueue(static_cast<uint16_t>(value));
    input_edit_->clear();
    input_edit_->setFocus();
    updateCounter();
  }
}

void StreamInput16RawComponent::updateCounter() const {
  counter_label_->setText(QString("Sent: %1 / %2 | Queue: %3")
      .arg(total_sent_).arg(target_count_).arg(value_queue_.size()));
}

COMPONENT_CLASS_DEFINITION(StreamInput16, 4, 5)

// ---- Settings Feature overrides ----

QList<NumberSettingInfo> StreamInput16RawComponent::numberSettings() const {
  return {{"target_count", "Target Count", 1, 1024, target_count_}};
}

void StreamInput16RawComponent::setNumberSetting(const QString &key, int value) {
  if (key == "target_count") {
    target_count_ = value;
    updateCounter();
  }
}

void StreamInput16Component::onSettingsBtnClicked() {
  auto dlg = new ComponentSettingsDialogWithFeatures<
      SettingsFeature::NumberSetting>(this, this);
  dlg->exec();
  delete dlg;
}
