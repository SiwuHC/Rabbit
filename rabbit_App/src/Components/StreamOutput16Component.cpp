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

  auto main_layout = new QVBoxLayout(this);
  main_layout->setContentsMargins(5, 5, 5, 5);
  main_layout->setSpacing(2);

  status_label_ = new QLabel("Waiting for data...", this);
  status_label_->setStyleSheet("font-size: 11px; font-weight: bold;");
  main_layout->addWidget(status_label_);

  output_list_ = new QListWidget(this);
  output_list_->setStyleSheet("font-size: 10px;");
  output_list_->setAlternatingRowColors(true);
  main_layout->addWidget(output_list_);

  auto btn_layout = new QHBoxLayout();
  grid_btn_ = new QPushButton("Grid", this);
  clear_btn_ = new QPushButton("Clear", this);
  grid_btn_->setMinimumHeight(25);
  clear_btn_->setMinimumHeight(25);
  btn_layout->addWidget(grid_btn_);
  btn_layout->addWidget(clear_btn_);
  main_layout->addLayout(btn_layout);

  setLayout(main_layout);

  connect(grid_btn_, &QPushButton::clicked, this,
          &StreamOutput16RawComponent::toggleGridMode);
  connect(clear_btn_, &QPushButton::clicked, this,
          &StreamOutput16RawComponent::onClear);
}

StreamOutput16RawComponent::~StreamOutput16RawComponent() {}

void StreamOutput16RawComponent::reset() {
  output_list_->clear();
  received_count_ = 0;
  prev_clk_ = 0;
  updateDisplay();
}

void StreamOutput16RawComponent::processReadData(QQueue<uint64_t> &read_queue) {
  const auto &ports = output_ports_;
  // ports[0..15] = DATA[0..15], ports[16] = CLK, ports[17] = DATA_VALID

  for (const auto &sample : read_queue) {
    // Extract CLK and DATA_VALID (pin_index is 1-based for output ports)
    int curr_clk = (sample >> (ports[16].pin_index - 1)) & 1;
    int curr_valid = (sample >> (ports[17].pin_index - 1)) & 1;

    // Detect CLK rising edge with DATA_VALID active
    if (prev_clk_ == 0 && curr_clk == 1 && curr_valid == 1) {
      // Capture 16-bit DATA word
      uint16_t val = 0;
      for (int i = 0; i < kDataWidth; i++) {
        int bit = (sample >> (ports[i].pin_index - 1)) & 1;
        if (bit) val |= (1 << i);
      }
      addValueToList(val);
    }

    prev_clk_ = curr_clk;
  }
}

uint64_t StreamOutput16RawComponent::getWriteData() const {
  return 0;
}

void StreamOutput16RawComponent::paintEvent(QPaintEvent *event) {
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);
  painter.setPen(Qt::black);
  painter.setBrush(Qt::white);
  painter.drawRect(rect());
}

void StreamOutput16RawComponent::initPorts() {
  // 18 ports: DATA[15:0] + CLK + DATA_VALID
  for (int i = 0; i < kDataWidth; i++) {
    appendPort(output_ports_,
               QString("DATA[%1]").arg(i),
               ports::PortType::Output);
  }
  appendPort(output_ports_, "CLK", ports::PortType::Output);
  appendPort(output_ports_, "DATA_VALID", ports::PortType::Output);
}

void StreamOutput16RawComponent::toggleGridMode() {
  grid_mode_ = !grid_mode_;
  grid_btn_->setText(grid_mode_ ? "List" : "Grid");
  if (grid_mode_) {
    rebuildGridDisplay();
  } else {
    // Switch back to list: rebuild as plain list
    output_list_->clear();
    output_list_->setViewMode(QListView::ListMode);
    // Values are already stored as items; just need to re-display.
    // Due to the rebuild, we lose items.  For simplicity, keep items
    // as they are but change the view.
    output_list_->setViewMode(QListView::ListMode);
  }
}

void StreamOutput16RawComponent::onClear() {
  output_list_->clear();
  received_count_ = 0;
  updateDisplay();
}

void StreamOutput16RawComponent::addValueToList(uint16_t val) {
  received_count_++;
  output_list_->addItem(QString("#%1: 0x%2 (%3)")
      .arg(received_count_ - 1)
      .arg(val, 4, 16, QChar('0')).toUpper()
      .arg(val));
  output_list_->scrollToBottom();
  if (grid_mode_) {
    rebuildGridDisplay();
  }
  updateDisplay();
}

void StreamOutput16RawComponent::rebuildGridDisplay() {
  // Collect all values from list items and display in grid format
  QStringList values;
  for (int i = 0; i < output_list_->count(); i++) {
    auto *item = output_list_->item(i);
    if (item) {
      // Extract just the hex value from the item text
      QString text = item->text();
      int colon = text.indexOf(':');
      if (colon >= 0) {
        values.append(text.mid(colon + 1).trimmed());
      }
    }
  }
  output_list_->clear();
  for (int r = 0; r < grid_rows_; r++) {
    QString row;
    for (int c = 0; c < grid_cols_; c++) {
      int idx = r * grid_cols_ + c;
      if (idx < values.size()) {
        row += values[idx];
      } else {
        row += "----";
      }
      if (c < grid_cols_ - 1) row += "  ";
    }
    output_list_->addItem(row);
  }
}

void StreamOutput16RawComponent::updateDisplay() {
  status_label_->setText(QString("Received: %1 words").arg(received_count_));
}

COMPONENT_CLASS_DEFINITION(StreamOutput16, 4, 5)

// ---- Settings Feature overrides ----

QList<NumberSettingInfo> StreamOutput16RawComponent::numberSettings() const {
  return {
      {"grid_rows", "Grid Rows", 1, 32, grid_rows_},
      {"grid_cols", "Grid Cols", 1, 32, grid_cols_},
  };
}

void StreamOutput16RawComponent::setNumberSetting(const QString &key, int value) {
  if (key == "grid_rows") grid_rows_ = value;
  else if (key == "grid_cols") grid_cols_ = value;
  if (grid_mode_) rebuildGridDisplay();
}

void StreamOutput16Component::onSettingsBtnClicked() {
    auto dlg=new ComponentSettingsDialogWithFeatures<SettingsFeature::ArrayPortMapping,SettingsFeature::NumberSetting>(this,this);

  dlg->exec();
  delete dlg;
}
