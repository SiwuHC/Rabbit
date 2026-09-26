#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPainter>

#include "Components/ComponentSettingsDialog.h"
#include "Components/AbstractComponent.h"
#include "Components/StreamInput32Component.h"

using namespace rabbit_App::component;

static constexpr int kDataWidth = 32;

StreamInput32RawComponent::StreamInput32RawComponent(QWidget *parent)
    : AbstractRawComponent(parent) {
  initPorts();
  auto ml = new QVBoxLayout(this); ml->setContentsMargins(5,5,5,5); ml->setSpacing(2);
  auto il = new QHBoxLayout();
  input_edit_ = new QLineEdit(this);
  input_edit_->setPlaceholderText("Enter 32-bit value (0-4294967295)");
  input_edit_->setStyleSheet("font-size:12px;");
  il->addWidget(input_edit_, 3); ml->addLayout(il);
  counter_label_ = new QLabel("Sent: 0 / 32", this);
  counter_label_->setStyleSheet("font-size:11px;"); ml->addWidget(counter_label_);
  sent_list_ = new QListWidget(this); sent_list_->setStyleSheet("font-size:10px;");
  sent_list_->setMaximumHeight(120); ml->addWidget(sent_list_);
  setLayout(ml); initConnections();
}

StreamInput32RawComponent::~StreamInput32RawComponent() {}
void StreamInput32RawComponent::initConnections() {
  connect(input_edit_, &QLineEdit::returnPressed, this, &StreamInput32RawComponent::onEnterPressed);
}
void StreamInput32RawComponent::reset() {
  value_queue_.clear(); total_sent_=0; phase_=IDLE; hold_cnt_=0;
  current_val_=0; last_output_=0; sent_list_->clear(); updateCounter();
}
void StreamInput32RawComponent::processReadData(QQueue<uint64_t>&) {}
uint64_t StreamInput32RawComponent::getWriteData() const {
  const auto &p = input_ports_;
  if (total_sent_ >= target_count_) return 0;
  if (phase_ == IDLE) { if (value_queue_.isEmpty()) return 0; current_val_=value_queue_.dequeue(); phase_=SETUP; hold_cnt_=0; }
  hold_cnt_++; if (hold_cnt_ < clk_hold_) return last_output_;
  hold_cnt_=0;
  uint64_t data=0; int clk=0,stb=0;
  switch (phase_) {
    case SETUP: clk=0; stb=1; phase_=RISING; break;
    case RISING: clk=1; stb=1; phase_=FALLING; break;
    case FALLING:
      clk=0; stb=0; total_sent_++;
      sent_list_->insertItem(0, QString("#%1: 0x%2 (%3)").arg(total_sent_-1).arg(current_val_,8,16,QChar('0')).toUpper().arg(current_val_));
      while (sent_list_->count()>100) delete sent_list_->takeItem(sent_list_->count()-1);
      updateCounter();
      if (total_sent_>=target_count_) { phase_=IDLE; last_output_=0; return 0; }
      if (!value_queue_.isEmpty()) { current_val_=value_queue_.dequeue(); phase_=SETUP; clk=0; stb=1; } else { phase_=IDLE; } break;
    default: phase_=IDLE; last_output_=0; return 0;
  }
  for (int i=0;i<kDataWidth;i++) data|=(static_cast<uint64_t>((current_val_>>i)&1)<<p[i].pin_index);
  data|=(static_cast<uint64_t>(clk)<<p[kDataWidth].pin_index);
  data|=(static_cast<uint64_t>(stb)<<p[kDataWidth+1].pin_index);
  last_output_=data; return data;
}
void StreamInput32RawComponent::paintEvent(QPaintEvent *) {
  // palette-derived: the panel follows the light/dark theme
  const QPalette palette;
  QPainter pn(this);
  pn.setPen(palette.color(QPalette::WindowText));
  pn.setBrush(palette.color(QPalette::Base));
  pn.drawRect(rect());
}
void StreamInput32RawComponent::initPorts() {
  for (int i=0;i<kDataWidth;i++) appendPort(input_ports_,QString("DATA[%1]").arg(i),ports::PortType::Input);
  appendPort(input_ports_,"CLK",ports::PortType::Input);
  appendPort(input_ports_,"STROBE",ports::PortType::Input);
}
void StreamInput32RawComponent::onEnterPressed() {
  bool ok; uint32_t v=input_edit_->text().toUInt(&ok);
  if (ok) { value_queue_.enqueue(v); input_edit_->clear(); input_edit_->setFocus(); updateCounter(); }
}
void StreamInput32RawComponent::updateCounter() const {
  counter_label_->setText(QString("Sent: %1 / %2 | Queue: %3").arg(total_sent_).arg(target_count_).arg(value_queue_.size()));
}
COMPONENT_CLASS_DEFINITION(StreamInput32,4,5)

QList<NumberSettingInfo> StreamInput32RawComponent::numberSettings() const {
  return {{"target_count","Target Count",1,1024,target_count_},{"clk_hold","CLK Hold",1,100,clk_hold_}};
}
void StreamInput32RawComponent::setNumberSetting(const QString &key, int value) {
  if (key=="target_count") { target_count_=value; updateCounter(); } else if (key=="clk_hold") { clk_hold_=value; }
}
void StreamInput32Component::onSettingsBtnClicked() {
  auto dlg=new ComponentSettingsDialogWithFeatures<SettingsFeature::ArrayPortMapping,SettingsFeature::NumberSetting>(this,this);
  dlg->exec(); delete dlg;
}
