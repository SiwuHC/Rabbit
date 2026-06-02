#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPainter>

#include "Components/ComponentSettingsDialog.h"
#include "Components/AbstractComponent.h"
#include "Components/StreamOutput8Component.h"

using namespace rabbit_App::component;

static constexpr int kDataWidth = 8;

StreamOutput8RawComponent::StreamOutput8RawComponent(QWidget *parent)
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
  connect(grid_btn_,&QPushButton::clicked,this,&StreamOutput8RawComponent::toggleGridMode);
  connect(clear_btn_,&QPushButton::clicked,this,&StreamOutput8RawComponent::onClear);
}
StreamOutput8RawComponent::~StreamOutput8RawComponent() {}
void StreamOutput8RawComponent::reset() { output_list_->clear(); received_count_=0; prev_clk_=0; updateDisplay(); }
void StreamOutput8RawComponent::processReadData(QQueue<uint64_t> &rq) {
  const auto &p = output_ports_;
  for (const auto &s : rq) {
    int clk=(s>>(p[kDataWidth].pin_index-1))&1;
    int vld=(s>>(p[kDataWidth+1].pin_index-1))&1;
    if (prev_clk_==0 && clk==1 && vld==1) {
      uint8_t val=0;
      for (int i=0;i<kDataWidth;i++) { int b=(s>>(p[i].pin_index-1))&1; if(b) val|=(1<<i); }
      addValueToList(val);
    }
    prev_clk_=clk;
  }
}
uint64_t StreamOutput8RawComponent::getWriteData() const { return 0; }
void StreamOutput8RawComponent::paintEvent(QPaintEvent*) { QPainter pn(this); pn.setPen(Qt::black); pn.setBrush(Qt::white); pn.drawRect(rect()); }
void StreamOutput8RawComponent::initPorts() {
  for (int i=0;i<kDataWidth;i++) appendPort(output_ports_,QString("DATA[%1]").arg(i),ports::PortType::Output);
  appendPort(output_ports_,"CLK",ports::PortType::Output);
  appendPort(output_ports_,"DATA_VALID",ports::PortType::Output);
}
void StreamOutput8RawComponent::toggleGridMode() {
  grid_mode_=!grid_mode_; grid_btn_->setText(grid_mode_?"List":"Grid");
  if (grid_mode_) rebuildGridDisplay();
}
void StreamOutput8RawComponent::onClear() { output_list_->clear(); received_count_=0; updateDisplay(); }
void StreamOutput8RawComponent::addValueToList(uint8_t val) {
  received_count_++;
  output_list_->addItem(QString("#%1: 0x%2 (%3)").arg(received_count_-1).arg(val,2,16,QChar('0')).toUpper().arg(val));
  output_list_->scrollToBottom();
  if (grid_mode_) rebuildGridDisplay(); updateDisplay();
}
void StreamOutput8RawComponent::rebuildGridDisplay() {
  QStringList vals;
  for (int i=0;i<output_list_->count();i++) {
    auto *it=output_list_->item(i); if(!it) continue;
    QString t=it->text(); int c=t.indexOf(':'); if(c>=0) vals.append(t.mid(c+1).trimmed());
  }
  output_list_->clear();
  for (int r=0;r<grid_rows_;r++) {
    QString row;
    for (int c=0;c<grid_cols_;c++) { int idx=r*grid_cols_+c; row+=(idx<vals.size())?vals[idx]:"----"; if(c<grid_cols_-1) row+="  "; }
    output_list_->addItem(row);
  }
}
void StreamOutput8RawComponent::updateDisplay() {
  status_label_->setText(QString("Received: %1 words").arg(received_count_));
}
COMPONENT_CLASS_DEFINITION(StreamOutput8,4,5)

QList<NumberSettingInfo> StreamOutput8RawComponent::numberSettings() const {
  return {{"grid_rows","Grid Rows",1,32,grid_rows_},{"grid_cols","Grid Cols",1,32,grid_cols_}};
}
void StreamOutput8RawComponent::setNumberSetting(const QString &key, int value) {
  if (key=="grid_rows") grid_rows_=value; else if (key=="grid_cols") grid_cols_=value;
  if (grid_mode_) rebuildGridDisplay();
}
void StreamOutput8Component::onSettingsBtnClicked() {
  auto dlg=new ComponentSettingsDialogWithFeatures<SettingsFeature::NumberSetting>(this,this);
  dlg->exec(); delete dlg;
}
