#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPainter>

#include "Components/ComponentSettingsDialog.h"
#include "Components/AbstractComponent.h"
#include "Components/StreamOutput16Component.h"

using namespace rabbit_App::component;

static constexpr int kDataWidth = 16;

StreamOutput16RawComponent::StreamOutput16RawComponent(QWidget *parent)
    : AbstractRawComponent(parent) {
  initPorts();
  auto ml = new QVBoxLayout(this); ml->setContentsMargins(5,5,5,5); ml->setSpacing(2);
  status_label_ = new QLabel("Waiting for data...", this);
  status_label_->setStyleSheet("font-size:11px; font-weight:bold;"); ml->addWidget(status_label_);
  output_list_ = new QListWidget(this); output_list_->setStyleSheet("font-size:10px;");
  output_list_->setAlternatingRowColors(true); ml->addWidget(output_list_);
  auto bl = new QHBoxLayout();
  grid_btn_ = new QPushButton("Grid", this); clear_btn_ = new QPushButton("Clear", this);
  grid_btn_->setMinimumHeight(25); clear_btn_->setMinimumHeight(25);
  bl->addWidget(grid_btn_); bl->addWidget(clear_btn_); ml->addLayout(bl);
  setLayout(ml);
  connect(grid_btn_,&QPushButton::clicked,this,&StreamOutput16RawComponent::toggleGridMode);
  connect(clear_btn_,&QPushButton::clicked,this,&StreamOutput16RawComponent::onClear);
}
StreamOutput16RawComponent::~StreamOutput16RawComponent() {}
void StreamOutput16RawComponent::reset() { output_list_->clear(); stored_vals_.clear(); received_count_=0; prev_clk_=0; updateDisplay(); }
void StreamOutput16RawComponent::processReadData(QQueue<uint64_t> &rq) {
  const auto &p = output_ports_;
  for (const auto &s : rq) {
    int clk=(s>>(p[kDataWidth].pin_index-1))&1;
    int vld=(s>>(p[kDataWidth+1].pin_index-1))&1;
    if (prev_clk_==0 && clk==1 && vld==1) {
      uint16_t val=0;
      for (int i=0;i<kDataWidth;i++) { int b=(s>>(p[i].pin_index-1))&1; if(b) val|=(1<<i); }
      addValueToList(val);
    }
    prev_clk_=clk;
  }
}
uint64_t StreamOutput16RawComponent::getWriteData() const { return 0; }
void StreamOutput16RawComponent::paintEvent(QPaintEvent *) {
  // palette-derived: the panel follows the light/dark theme
  const QPalette palette;
  QPainter pn(this);
  pn.setPen(palette.color(QPalette::WindowText));
  pn.setBrush(palette.color(QPalette::Base));
  pn.drawRect(rect());
}
void StreamOutput16RawComponent::initPorts() {
  for (int i=0;i<kDataWidth;i++) appendPort(output_ports_,QString("DATA[%1]").arg(i),ports::PortType::Output);
  appendPort(output_ports_,"CLK",ports::PortType::Output);
  appendPort(output_ports_,"DATA_VALID",ports::PortType::Output);
}
void StreamOutput16RawComponent::toggleGridMode() {
  grid_mode_=!grid_mode_; grid_btn_->setText(grid_mode_?"List":"Grid");
  output_list_->clear();
  if (grid_mode_) {
    for (int r=0;r<grid_rows_;r++) {
      QString row;
      for (int c=0;c<grid_cols_;c++) { int idx=r*grid_cols_+c; row+=(idx<stored_vals_.size())?stored_vals_[idx]:"----"; if(c<grid_cols_-1) row+="  "; }
      output_list_->addItem(row);
    }
  } else {
    for (int i=0;i<stored_vals_.size();i++) output_list_->addItem(QString("#%1: %2").arg(i).arg(stored_vals_[i]));
  }
}
void StreamOutput16RawComponent::onClear() { output_list_->clear(); stored_vals_.clear(); received_count_=0; updateDisplay(); }
void StreamOutput16RawComponent::addValueToList(uint16_t val) {
  received_count_++;
  stored_vals_.append(QString("0x%1 (%2)").arg(val,4,16,QChar('0')).toUpper().arg(val));
  if (grid_mode_) {
    output_list_->clear();
    for (int r=0;r<grid_rows_;r++) {
      QString row;
      for (int c=0;c<grid_cols_;c++) { int idx=r*grid_cols_+c; row+=(idx<stored_vals_.size())?stored_vals_[idx]:"----"; if(c<grid_cols_-1) row+="  "; }
      output_list_->addItem(row);
    }
  } else {
    output_list_->addItem(QString("#%1: %2").arg(received_count_-1).arg(stored_vals_.last()));
    output_list_->scrollToBottom();
  }
  updateDisplay();
}
void StreamOutput16RawComponent::rebuildGridDisplay() {
  output_list_->clear();
  for (int r=0;r<grid_rows_;r++) {
    QString row;
    for (int c=0;c<grid_cols_;c++) { int idx=r*grid_cols_+c; row+=(idx<stored_vals_.size())?stored_vals_[idx]:"----"; if(c<grid_cols_-1) row+="  "; }
    output_list_->addItem(row);
  }
}
void StreamOutput16RawComponent::updateDisplay() {
  status_label_->setText(QString("Received: %1 words").arg(received_count_));
}
COMPONENT_CLASS_DEFINITION(StreamOutput16,4,5)

QList<NumberSettingInfo> StreamOutput16RawComponent::numberSettings() const {
  return {{"grid_rows","Grid Rows",1,32,grid_rows_},{"grid_cols","Grid Cols",1,32,grid_cols_}};
}
void StreamOutput16RawComponent::setNumberSetting(const QString &key, int value) {
  if (key=="grid_rows") grid_rows_=value; else if (key=="grid_cols") grid_cols_=value;
  if (grid_mode_) rebuildGridDisplay();
}
void StreamOutput16Component::onSettingsBtnClicked() {
  auto dlg=new ComponentSettingsDialogWithFeatures<SettingsFeature::ArrayPortMapping,SettingsFeature::NumberSetting>(this,this);
  dlg->exec(); delete dlg;
}
