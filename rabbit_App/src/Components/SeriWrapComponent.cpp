#include "Components/SeriWrapComponent.h"

#include "Components/ComponentSettingsDialog.h"

#include <algorithm>

#include <QAbstractItemView>
#include <QFileDialog>
#include <QFont>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QPainter>
#include <QSplitter>
#include <QVBoxLayout>

using namespace rabbit_App::component;
using rabbit_App::ports::PortType;

namespace {

int boundFrameBit(const rabbit_App::ports::Port &p, bool is_output) {
  if (p.pin_index < 0) return -1;
  // Output pins are numbered from 1 inside Rabbit (index 0 is reserved for the
  // clock), so the frame bit is one lower than pin_index -- same convention as
  // the existing Stream/DataCapture components.
  return is_output ? p.pin_index - 1 : p.pin_index;
}

QString hex(uint64_t v) { return QString("0x%1").arg(v, 0, 16); }

bool parseValue(const QString &text, uint64_t *out) {
  const QString t = text.trimmed();
  if (t.isEmpty()) return false;
  bool ok = false;
  const uint64_t v = t.startsWith("0x", Qt::CaseInsensitive) ? t.mid(2).toULongLong(&ok, 16)
                                                            : t.toULongLong(&ok, 10);
  if (ok && out) *out = v;
  return ok;
}

uint64_t widthMask(int bits) {
  if (bits <= 0) return 0;
  if (bits >= 64) return ~0ULL;
  return (1ULL << bits) - 1ULL;
}

void setupTable(QTableWidget *t, int cols, const QStringList &headers, 
                const std::vector<QHeaderView::ResizeMode> &modes) {
  t->setColumnCount(cols);
  t->setHorizontalHeaderLabels(headers);
  t->verticalHeader()->setVisible(false);
  for (int i = 0; i < cols; ++i) {
    t->horizontalHeader()->setSectionResizeMode(
        i, i < static_cast<int>(modes.size()) ? modes[i] : QHeaderView::Stretch);
  }
  t->setEditTriggers(QAbstractItemView::NoEditTriggers);
  t->setSelectionMode(QAbstractItemView::NoSelection);
  t->setStyleSheet("font-size:11px;");
}

}  // namespace

// ---------------------------------------------------------------------------
// construction
// ---------------------------------------------------------------------------
SeriWrapRawComponent::SeriWrapRawComponent(QWidget *parent)
    : AbstractRawComponent(parent) {
  initPorts();

  auto *root = new QVBoxLayout(this);
  root->setContentsMargins(4, 4, 4, 4);
  root->setSpacing(3);

  status_label_ = new QLabel("SeriWrap: load a manifest", this);
  status_label_->setObjectName("status_label");
  status_label_->setStyleSheet("font-size:11px; font-weight:bold;");
  root->addWidget(status_label_);

  auto *split = new QSplitter(Qt::Horizontal, this);
  split->setObjectName("splitter");
  split->addWidget(buildConfigPane());
  split->addWidget(buildInputPane());
  split->addWidget(buildOutputPane());
  split->setStretchFactor(0, 0);
  split->setStretchFactor(1, 1);
  split->setStretchFactor(2, 1);
  root->addWidget(split, 1);

  rebuildFromBindings();
  rebuildPortTables();
  refreshModeButtons();
  refreshStreamSlot();
  refreshWordPreview();
  refreshLabels();
}

SeriWrapRawComponent::~SeriWrapRawComponent() {}

QWidget *SeriWrapRawComponent::buildConfigPane() {
  auto *box = new QGroupBox("配置", this);
  box->setObjectName("cfg_box");
  box->setMinimumWidth(190);
  auto *v = new QVBoxLayout(box);
  v->setSpacing(3);

  ready_dot_ = new QLabel("READY  ?", box);
  ready_dot_->setObjectName("ready_dot");
  ready_dot_->setStyleSheet("font-size:11px;");
  valid_dot_ = new QLabel("VALID  ?", box);
  valid_dot_->setObjectName("valid_dot");
  valid_dot_->setStyleSheet("font-size:11px;");
  v->addWidget(ready_dot_);
  v->addWidget(valid_dot_);

  auto *form = new QFormLayout();
  form->setContentsMargins(0, 2, 0, 2);
  hold_spin_ = new QSpinBox(box);
  hold_spin_->setObjectName("hold_spin");
  hold_spin_->setRange(1, 200);
  hold_spin_->setValue(hold_frames_);
  form->addRow("Hold Frames", hold_spin_);

  view_btn_ = new QToolButton(box);
  view_btn_->setObjectName("view_btn");
  view_btn_->setCheckable(true);
  view_btn_->setToolButtonStyle(Qt::ToolButtonTextOnly);
  form->addRow("输入视图", view_btn_);

  gran_btn_ = new QToolButton(box);
  gran_btn_->setObjectName("gran_btn");
  gran_btn_->setCheckable(true);
  gran_btn_->setToolButtonStyle(Qt::ToolButtonTextOnly);
  form->addRow("流式粒度", gran_btn_);
  v->addLayout(form);

  pin_label_ = new QLabel("引脚: -", box);
  pin_label_->setObjectName("pin_label");
  pin_label_->setWordWrap(true);
  pin_label_->setStyleSheet("font-size:10px; color:#444;");
  v->addWidget(pin_label_);

  manifest_btn_ = new QPushButton("Manifest...", box);
  manifest_btn_->setObjectName("manifest_btn");
  v->addWidget(manifest_btn_);

  send_btn_ = new QPushButton("Send frame", box);
  send_btn_->setObjectName("send_btn");
  v->addWidget(send_btn_);

  auto *hint = new QLabel(
      "引脚绑定用标题栏的齿轮按钮打开设置。整帧没填满时 Send 不会发帧。", box);
  hint->setWordWrap(true);
  hint->setStyleSheet("font-size:10px; color:#666;");
  v->addWidget(hint);
  v->addStretch(1);

  connect(hold_spin_, QOverload<int>::of(&QSpinBox::valueChanged), this,
          &SeriWrapRawComponent::onHoldChanged);
  connect(view_btn_, &QToolButton::clicked, this, &SeriWrapRawComponent::onViewClicked);
  connect(gran_btn_, &QToolButton::clicked, this, &SeriWrapRawComponent::onGranularityClicked);
  connect(manifest_btn_, &QPushButton::clicked, this,
          &SeriWrapRawComponent::onLoadManifestClicked);
  connect(send_btn_, &QPushButton::clicked, this, &SeriWrapRawComponent::onSendClicked);
  return box;
}

QWidget *SeriWrapRawComponent::buildInputPane() {
  auto *box = new QGroupBox("输入 (内核端口)", this);
  box->setObjectName("in_box");
  auto *v = new QVBoxLayout(box);
  v->setSpacing(3);

  input_stack_ = new QStackedWidget(box);
  input_stack_->setObjectName("input_stack");

  // ---- page 0: one row per kernel port --------------------------------
  auto *page_ports = new QWidget(input_stack_);
  auto *pv = new QVBoxLayout(page_ports);
  pv->setContentsMargins(0, 0, 0, 0);
  in_table_ = new QTableWidget(0, 3, page_ports);
  in_table_->setObjectName("in_table");
  setupTable(in_table_, 3, {"端口", "值 (dec 或 0x..)", "字"},
             {QHeaderView::ResizeToContents, QHeaderView::Stretch,
              QHeaderView::ResizeToContents});
  in_table_->setMinimumHeight(120);
  pv->addWidget(in_table_);
  input_stack_->addWidget(page_ports);

  // ---- page 1: StreamInput-style streaming entry ----------------------
  auto *page_stream = new QWidget(input_stack_);
  auto *sv = new QVBoxLayout(page_stream);
  sv->setContentsMargins(0, 0, 0, 0);
  slot_label_ = new QLabel("下一个: -", page_stream);
  slot_label_->setObjectName("slot_label");
  slot_label_->setWordWrap(true);
  slot_label_->setStyleSheet("font-size:11px;");
  sv->addWidget(slot_label_);
  stream_edit_ = new QLineEdit(page_stream);
  stream_edit_->setObjectName("stream_edit");
  stream_edit_->setPlaceholderText("输入一个值后回车（dec 或 0x..，也可粘贴 v0, v1, ...）");
  sv->addWidget(stream_edit_);
  hist_list_ = new QListWidget(page_stream);
  hist_list_->setObjectName("hist_list");
  hist_list_->setStyleSheet("font-size:10px;");
  sv->addWidget(hist_list_, 1);
  auto *row = new QHBoxLayout();
  undo_btn_ = new QPushButton("回退", page_stream);
  undo_btn_->setObjectName("undo_btn");
  clear_btn_ = new QPushButton("清空", page_stream);
  clear_btn_->setObjectName("clear_btn");
  autosend_box_ = new QCheckBox("填满自动发送", page_stream);
  autosend_box_->setObjectName("autosend_box");
  row->addWidget(undo_btn_);
  row->addWidget(clear_btn_);
  row->addWidget(autosend_box_);
  sv->addLayout(row);
  input_stack_->addWidget(page_stream);

  v->addWidget(input_stack_, 3);

  // ---- shared: bulk paste + the serial word stream --------------------
  auto *bulk_row = new QHBoxLayout();
  value_edit_ = new QLineEdit(box);
  value_edit_->setObjectName("bulk_edit");
  value_edit_->setPlaceholderText("批量填入: v0, v1, ...");
  value_edit_->setStyleSheet("font-size:11px;");
  bulk_btn_ = new QPushButton("Fill rows", box);
  bulk_btn_->setObjectName("bulk_btn");
  bulk_row->addWidget(value_edit_, 1);
  bulk_row->addWidget(bulk_btn_);
  v->addLayout(bulk_row);

  auto *word_hint = new QLabel("字流预览（线上串行顺序，每行 16 字）", box);
  word_hint->setStyleSheet("font-size:10px; color:#666;");
  v->addWidget(word_hint);
  word_preview_ = new QPlainTextEdit(box);
  word_preview_->setObjectName("word_preview");
  word_preview_->setReadOnly(true);
  word_preview_->setFixedHeight(84);
  word_preview_->setStyleSheet("font-family: monospace; font-size:10px;");
  v->addWidget(word_preview_);

  connect(stream_edit_, &QLineEdit::returnPressed, this, &SeriWrapRawComponent::onStreamReturn);
  connect(undo_btn_, &QPushButton::clicked, this, &SeriWrapRawComponent::onUndoClicked);
  connect(clear_btn_, &QPushButton::clicked, this, &SeriWrapRawComponent::onClearClicked);
  connect(bulk_btn_, &QPushButton::clicked, this, [this] {
    applyBulkValues();
    refreshWordPreview();
    refreshStreamSlot();
  });
  return box;
}

QWidget *SeriWrapRawComponent::buildOutputPane() {
  auto *box = new QGroupBox("输出 (内核端口)", this);
  box->setObjectName("out_box");
  auto *v = new QVBoxLayout(box);
  v->setSpacing(3);

  out_table_ = new QTableWidget(0, 2, box);
  out_table_->setObjectName("out_table");
  setupTable(out_table_, 2, {"端口", "上一帧"},
             {QHeaderView::ResizeToContents, QHeaderView::Stretch});
  out_table_->setMinimumHeight(60);
  v->addWidget(out_table_, 1);

  out_label_ = new QLabel("out: -", box);
  out_label_->setObjectName("out_label");
  out_label_->setWordWrap(true);
  out_label_->setStyleSheet("font-size:10px; color:#666;");
  v->addWidget(out_label_);

  raw_label_ = new QLabel("原始字: -", box);
  raw_label_->setObjectName("raw_label");
  raw_label_->setWordWrap(true);
  raw_label_->setStyleSheet("font-family: monospace; font-size:10px;");
  v->addWidget(raw_label_);

  log_list_ = new QListWidget(box);
  log_list_->setObjectName("log_list");
  log_list_->setStyleSheet("font-size:10px;");
  v->addWidget(log_list_, 2);
  return box;
}

void SeriWrapRawComponent::initPorts() {
  // host -> FPGA
  for (int i = 0; i < kSeriWrapMaxWidth; ++i) {
    appendPort(input_ports_, QString("DATA[%1]").arg(i), PortType::Input);
  }
  appendPort(input_ports_, "CLK", PortType::Input);
  appendPort(input_ports_, "STROBE", PortType::Input);
  // FPGA -> host
  for (int i = 0; i < kSeriWrapMaxWidth; ++i) {
    appendPort(output_ports_, QString("DOUT[%1]").arg(i), PortType::Output);
  }
  appendPort(output_ports_, "CLK_OUT", PortType::Output);
  appendPort(output_ports_, "DATA_VALID", PortType::Output);
  appendPort(output_ports_, "READY", PortType::Output);
}

void SeriWrapRawComponent::rebuildFromBindings() const {
  pins_ = seriwrap::PinMap();
  pins_.data_in.assign(kSeriWrapMaxWidth, -1);
  pins_.data_out.assign(kSeriWrapMaxWidth, -1);

  for (int i = 0; i < kSeriWrapMaxWidth && i < input_ports_.size(); ++i) {
    pins_.data_in[i] = boundFrameBit(input_ports_[i], false);
  }
  pins_.clk_in = boundFrameBit(input_ports_[kSeriWrapMaxWidth], false);
  pins_.strobe_in = boundFrameBit(input_ports_[kSeriWrapMaxWidth + 1], false);

  for (int i = 0; i < kSeriWrapMaxWidth && i < output_ports_.size(); ++i) {
    pins_.data_out[i] = boundFrameBit(output_ports_[i], true);
  }
  pins_.clk_out = boundFrameBit(output_ports_[kSeriWrapMaxWidth], true);
  pins_.data_valid = boundFrameBit(output_ports_[kSeriWrapMaxWidth + 1], true);
  pins_.ready = boundFrameBit(output_ports_[kSeriWrapMaxWidth + 2], true);

  if (cfg_.input_words <= 0 && cfg_.output_words <= 0) {
    // No manifest loaded yet: fall back to the simplest possible link so the
    // component is still usable for a one-word kernel.
    int bound = 0;
    for (int i = 0; i < kSeriWrapMaxWidth; ++i) {
      if (pins_.data_in[i] >= 0) bound = i + 1;
    }
    if (bound <= 0) bound = 8;
    cfg_.word_width = bound;
    cfg_.input_words = 1;
    cfg_.output_words = 1;
    cfg_.sync_mode = false;
    cfg_.has_ready = boundFrameBit(output_ports_[kSeriWrapMaxWidth + 2], true) >= 0;
    cfg_.n_in_ports = 1;
    cfg_.n_out_ports = 1;
    cfg_.in_port_names = {"a0"};
    cfg_.out_port_names = {"y0"};
    cfg_.in_packing = {{{0, bound - 1, 0, bound - 1, 0}}};
    cfg_.out_packing = {{{0, bound - 1, 0, bound - 1, 0}}};
  }

  proto_ = std::make_unique<seriwrap::SeriWrapProtocol>(cfg_, pins_);
  proto_->set_hold_frames(hold_frames_);

  int bound_in = 0, bound_out = 0;
  for (int i = 0; i < kSeriWrapMaxWidth; ++i) {
    if (pins_.data_in[i] >= 0) ++bound_in;
    if (pins_.data_out[i] >= 0) ++bound_out;
  }
  if (pin_label_) {
    pin_label_->setText(QString("引脚: DATA %1 / DOUT %2 位已绑定%3")
                            .arg(bound_in)
                            .arg(bound_out)
                            .arg(cfg_.sync_mode ? "，sync" : "，async"));
  }
}

// ---------------------------------------------------------------------------
// manifest
// ---------------------------------------------------------------------------
bool SeriWrapRawComponent::loadManifestFile(const QString &path, QString *error) {
  QFile f(path);
  if (!f.open(QIODevice::ReadOnly)) {
    if (error) *error = "Cannot open " + path;
    appendLog("ERROR: cannot open " + path);
    return false;
  }
  QJsonParseError err{};
  const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &err);
  if (err.error != QJsonParseError::NoError || !doc.isObject()) {
    if (error) *error = "Invalid manifest: " + err.errorString();
    appendLog("ERROR: invalid manifest");
    return false;
  }
  const QJsonObject root = doc.object();
  const QJsonObject link = root.value("link").toObject();
  const QJsonObject frame = root.value("frame").toObject();
  const QJsonObject ports_obj = root.value("ports").toObject();

  seriwrap::LinkConfig c;
  c.word_width = link.value("width").toInt(32);
  c.input_words = frame.value("input_words").toInt(0);
  c.output_words = frame.value("output_words").toInt(0);
  c.sync_mode = link.value("sync_mode").toBool(false);
  c.has_ready = frame.value("handshake").toObject().value("ready").toBool(true);

  const QJsonArray ins = ports_obj.value("inputs").toArray();
  const QJsonArray outs = ports_obj.value("outputs").toArray();
  c.n_in_ports = ins.size();
  c.n_out_ports = outs.size();
  in_port_widths_.clear();
  for (const auto &v : ins) {
    const QJsonObject o = v.toObject();
    c.in_port_names.push_back(o.value("name").toString().toStdString());
    in_port_widths_.push_back(o.value("width").toInt(32));
  }
  for (const auto &v : outs) c.out_port_names.push_back(v.toObject().value("name").toString().toStdString());

  auto to_packing = [&c](const QJsonArray &arr) {
    std::vector<std::vector<seriwrap::Field>> out;
    for (const auto &wv : arr) {
      const QJsonObject wo = wv.toObject();
      std::vector<seriwrap::Field> fields;
      for (const auto &fv : wo.value("fields").toArray()) {
        const QJsonObject fo = fv.toObject();
        const QJsonArray pb = fo.value("port_bits").toArray();
        const QJsonArray wb = fo.value("word_bits").toArray();
        seriwrap::Field fld;
        fld.port_index = -1;
        const QString pname = fo.value("port").toString();
        for (int i = 0; i < static_cast<int>(c.in_port_names.size()); ++i) {
          if (QString::fromStdString(c.in_port_names[i]) == pname) fld.port_index = i;
        }
        for (int i = 0; i < static_cast<int>(c.out_port_names.size()); ++i) {
          if (fld.port_index < 0 && QString::fromStdString(c.out_port_names[i]) == pname) {
            fld.port_index = i;
          }
        }
        if (pb.size() == 2) { fld.port_hi = pb[0].toInt(); fld.port_lo = pb[1].toInt(); }
        if (wb.size() == 2) { fld.word_hi = wb[0].toInt(); fld.word_lo = wb[1].toInt(); }
        fields.push_back(fld);
      }
      out.push_back(fields);
    }
    return out;
  };
  c.in_packing = to_packing(root.value("packing").toObject().value("input").toArray());
  c.out_packing = to_packing(root.value("packing").toObject().value("output").toArray());

  cfg_ = c;
  manifest_words_in_ = c.input_words;
  manifest_words_out_ = c.output_words;
  manifest_sync_ = c.sync_mode;
  rebuildFromBindings();
  rebuildPortTables();
  stream_slot_ = 0;
  hist_list_->clear();
  stream_undo_.clear();
  stream_undo_slot_.clear();
  refreshModeButtons();
  refreshStreamSlot();
  refreshWordPreview();
  appendLog(QString("manifest: %1 in-words, %2 out-words, %3-bit words, %4")
                .arg(c.input_words)
                .arg(c.output_words)
                .arg(c.word_width)
                .arg(c.sync_mode ? "sync" : "async"));
  refreshLabels();
  return true;
}

// ---------------------------------------------------------------------------
// tables / preview / streaming editor
// ---------------------------------------------------------------------------
void SeriWrapRawComponent::rebuildPortTables() {
  if (!in_table_ || !out_table_) return;
  const int nin = std::max<int>(1, cfg_.n_in_ports);
  const int nout = std::max<int>(1, cfg_.n_out_ports);

  // Keep whatever the user already typed: the tables are rebuilt on every
  // manifest load, and losing hand-typed values there would be rude.
  std::vector<QString> keep;
  for (auto *e : in_edits_) keep.push_back(e ? e->text() : QString());

  updating_rows_ = true;
  in_table_->clearContents();
  in_table_->setRowCount(nin);
  in_edits_.assign(nin, nullptr);
  for (int i = 0; i < nin; ++i) {
    const QString name = i < static_cast<int>(cfg_.in_port_names.size())
                             ? QString::fromStdString(cfg_.in_port_names[i])
                             : QString("in[%1]").arg(i);
    const int width = i < static_cast<int>(in_port_widths_.size()) ? in_port_widths_[i] : 0;
    auto *label = new QTableWidgetItem(
        width > 0 ? QString("%1 [%2]").arg(name).arg(width) : name);
    label->setFlags(Qt::ItemIsEnabled);
    in_table_->setItem(i, 0, label);

    auto *e = new QLineEdit(in_table_);
    e->setObjectName(QString("in_value_%1").arg(i));
    e->setPlaceholderText("0x0");
    e->setText(i < static_cast<int>(keep.size()) && !keep[i].isEmpty() ? keep[i]
                                                                      : QString("0x0"));
    e->setStyleSheet("font-size:11px;");
    connect(e, &QLineEdit::textChanged, this, [this, i] {
      if (updating_rows_) return;
      // Editing a port row by hand means that port is set: keep the "frame is
      // filled" bookkeeping (and therefore Send) in step with the table.
      markSlotsFilledForPortRow(i);
      refreshWordPreview();
      refreshStreamSlot();
    });
    in_table_->setCellWidget(i, 1, e);
    in_edits_[i] = e;

    const int lo = portWordLo(i);
    const int hi = portWordHi(i);
    auto *words = new QTableWidgetItem(
        lo < 0 ? QString("-") : (lo == hi ? QString("w%1").arg(lo)
                                          : QString("w%1..w%2").arg(lo).arg(hi)));
    words->setFlags(Qt::ItemIsEnabled);
    in_table_->setItem(i, 2, words);
  }
  updating_rows_ = false;

  out_table_->clearContents();
  out_table_->setRowCount(nout);
  out_values_.assign(nout, nullptr);
  for (int i = 0; i < nout; ++i) {
    const QString name = i < static_cast<int>(cfg_.out_port_names.size())
                             ? QString::fromStdString(cfg_.out_port_names[i])
                             : QString("out[%1]").arg(i);
    const int width = i < static_cast<int>(cfg_.n_out_ports) ? 0 : 0;
    Q_UNUSED(width);
    auto *label = new QTableWidgetItem(name);
    label->setFlags(Qt::ItemIsEnabled);
    out_table_->setItem(i, 0, label);
    auto *val = new QTableWidgetItem("-");
    val->setFlags(Qt::ItemIsEnabled);
    out_table_->setItem(i, 1, val);
    out_values_[i] = val;
  }

  stream_filled_.assign(static_cast<size_t>(streamSlotCount()), false);
}

int SeriWrapRawComponent::portWordLo(int port) const {
  for (size_t w = 0; w < cfg_.in_packing.size(); ++w) {
    for (const seriwrap::Field &f : cfg_.in_packing[w]) {
      if (f.port_index == port) return static_cast<int>(w);
    }
  }
  return -1;
}

int SeriWrapRawComponent::portWordHi(int port) const {
  for (size_t w = cfg_.in_packing.size(); w-- > 0;) {
    for (const seriwrap::Field &f : cfg_.in_packing[w]) {
      if (f.port_index == port) return static_cast<int>(w);
    }
  }
  return -1;
}

uint64_t SeriWrapRawComponent::dataBitsOfFrameWord(uint64_t frameWord) const {
  uint64_t v = 0;
  for (int k = 0; k < cfg_.word_width && k < static_cast<int>(pins_.data_in.size()); ++k) {
    const int b = pins_.data_in[k];
    if (b >= 0 && ((frameWord >> b) & 1ULL)) v |= (1ULL << k);
  }
  return v;
}

void SeriWrapRawComponent::refreshWordPreview() {
  if (!word_preview_) return;
  const std::vector<uint64_t> vals = inputValues();
  std::vector<uint64_t> words;
  if (proto_) words = proto_->previewInputWords(vals);
  QString text;
  const int per_line = 16;
  const int width = std::max(1, cfg_.word_width);
  const int digits = (width + 3) / 4;
  for (size_t i = 0; i < words.size(); ++i) {
    if (i && i % per_line == 0) text += "\n";
    text += QString("%1 ").arg(dataBitsOfFrameWord(words[i]), digits, 16, QChar('0'));
  }
  word_preview_->setPlainText(text);
}

int SeriWrapRawComponent::streamSlotCount() const {
  if (gran_ == Granularity::Port) return std::max(1, cfg_.n_in_ports);
  return std::max(1, cfg_.input_words);
}

int SeriWrapRawComponent::streamSlot() const { return stream_slot_; }

bool SeriWrapRawComponent::streamFull() const {
  const int n = streamSlotCount();
  if (static_cast<int>(stream_filled_.size()) != n) return false;
  for (bool b : stream_filled_) {
    if (!b) return false;
  }
  return true;
}

int SeriWrapRawComponent::filledSlots() const {
  int n = 0;
  for (bool b : stream_filled_) {
    if (b) ++n;
  }
  return n;
}

QString SeriWrapRawComponent::streamSlotName() const {
  if (gran_ == Granularity::Port) {
    const int p = stream_slot_;
    return (p >= 0 && p < static_cast<int>(cfg_.in_port_names.size()))
               ? QString::fromStdString(cfg_.in_port_names[p])
               : QString("port[%1]").arg(p);
  }
  if (stream_slot_ >= 0 && stream_slot_ < static_cast<int>(cfg_.in_packing.size()) &&
      !cfg_.in_packing[stream_slot_].empty()) {
    const seriwrap::Field &f = cfg_.in_packing[stream_slot_][0];
    const QString pname = (f.port_index >= 0 &&
                           f.port_index < static_cast<int>(cfg_.in_port_names.size()))
                              ? QString::fromStdString(cfg_.in_port_names[f.port_index])
                              : QString("port[%1]").arg(f.port_index);
    return QString("w%1 = %2[%3:%4]").arg(stream_slot_).arg(pname).arg(f.port_hi).arg(f.port_lo);
  }
  return QString("w%1").arg(stream_slot_);
}

void SeriWrapRawComponent::refreshStreamSlot() {
  if (!slot_label_) return;
  const int n = streamSlotCount();
  if (stream_slot_ >= n) stream_slot_ = 0;
  QString s = QString("下一个: %1    %2/%3 已填")
                  .arg(streamSlotName())
                  .arg(filledSlots())
                  .arg(n);
  if (cfg_.n_in_ports > 0 && cfg_.n_in_ports == cfg_.input_words) {
    s += "    [1 端口 = 1 字，端口/字视图等价]";
  }
  slot_label_->setText(s);
}

void SeriWrapRawComponent::refreshModeButtons() {
  if (view_btn_) {
    view_btn_->setChecked(view_ == InputView::Streaming);
    view_btn_->setText(view_ == InputView::Streaming ? "流式" : "端口表");
  }
  if (gran_btn_) {
    gran_btn_->setChecked(gran_ == Granularity::Word);
    gran_btn_->setText(gran_ == Granularity::Word ? "字" : "端口");
  }
  if (input_stack_) {
    input_stack_->setCurrentIndex(view_ == InputView::Streaming ? 1 : 0);
  }
}

void SeriWrapRawComponent::setGranularity(Granularity g) {
  if (gran_ == g) return;
  gran_ = g;
  stream_slot_ = 0;
  stream_filled_.assign(static_cast<size_t>(streamSlotCount()), false);
  refreshModeButtons();
  refreshStreamSlot();
}

void SeriWrapRawComponent::setInputView(InputView v) {
  view_ = v;
  refreshModeButtons();
}

void SeriWrapRawComponent::writeRowValue(int port, uint64_t value) {
  if (port < 0 || port >= static_cast<int>(in_edits_.size()) || !in_edits_[port]) return;
  updating_rows_ = true;
  in_edits_[port]->setText(hex(value));
  updating_rows_ = false;
}

std::vector<uint64_t> SeriWrapRawComponent::inputValues() const {
  std::vector<uint64_t> vals;
  const size_t n = in_edits_.empty() ? 1 : in_edits_.size();
  vals.reserve(n);
  for (size_t i = 0; i < n; ++i) {
    QLineEdit *e = (i < in_edits_.size()) ? in_edits_[i] : nullptr;
    uint64_t v = 0;
    if (e) parseValue(e->text(), &v);
    vals.push_back(v);
  }
  return vals;
}

std::vector<uint64_t> SeriWrapRawComponent::previewWords() const {
  if (!proto_) return {};
  return proto_->previewInputWords(inputValues());
}

bool SeriWrapRawComponent::commitStreamValue(const QString &text, QString *error) {
  uint64_t v = 0;
  if (!parseValue(text, &v)) {
    if (error) *error = QString("cannot parse \"%1\"").arg(text.trimmed());
    return false;
  }
  std::vector<uint64_t> vals = inputValues();
  const int slot = stream_slot_;

  if (gran_ == Granularity::Word) {
    if (slot < 0 || slot >= static_cast<int>(cfg_.in_packing.size())) {
      if (error) *error = "no word slot (load a manifest first)";
      return false;
    }
    const int wwidth = cfg_.word_width;
    if (wwidth < 64 && (v >> wwidth) != 0) {
      if (error) {
        *error = QString("%1 does not fit in the %2-bit word w%3").arg(hex(v)).arg(wwidth).arg(slot);
      }
      return false;
    }
    for (const seriwrap::Field &f : cfg_.in_packing[slot]) {
      if (f.port_index < 0 || f.port_index >= static_cast<int>(vals.size())) continue;
      const int nbits = f.word_hi - f.word_lo + 1;
      const uint64_t field = (v >> f.word_lo) & widthMask(nbits);
      const int pbits = f.port_hi - f.port_lo + 1;
      const uint64_t mask = widthMask(pbits) << f.port_lo;
      vals[f.port_index] = (vals[f.port_index] & ~mask) | ((field & widthMask(pbits)) << f.port_lo);
    }
  } else {
    const int port = slot;
    if (port < 0 || port >= static_cast<int>(vals.size())) {
      if (error) *error = "no port slot (load a manifest first)";
      return false;
    }
    const int width = port < static_cast<int>(in_port_widths_.size()) ? in_port_widths_[port] : 32;
    if (width < 64 && (v >> width) != 0) {
      if (error) *error = QString("%1 does not fit in the %2-bit port").arg(hex(v)).arg(width);
      return false;
    }
    vals[port] = v;
  }

  stream_undo_.push_back(inputValues());          // snapshot for undo
  stream_undo_slot_.push_back(slot);
  for (size_t i = 0; i < vals.size(); ++i) writeRowValue(static_cast<int>(i), vals[i]);
  if (slot >= 0 && slot < static_cast<int>(stream_filled_.size())) stream_filled_[slot] = true;

  const QString what = streamSlotName();
  hist_list_->insertItem(0, QString("%1 <- %2").arg(what).arg(hex(v)));
  while (hist_list_->count() > 200) delete hist_list_->takeItem(hist_list_->count() - 1);

  stream_slot_ = (slot + 1) % streamSlotCount();
  refreshStreamSlot();
  refreshWordPreview();

  if (autosend_box_ && autosend_box_->isChecked() && streamFull()) {
    collectInputs();
    frame_armed_ = true;
    appendLog("streaming: frame complete, queued (auto-send)");
  }
  return true;
}

void SeriWrapRawComponent::undoStreamValue() {
  if (stream_undo_.empty()) return;
  const std::vector<uint64_t> prev = stream_undo_.back();
  const int slot = stream_undo_slot_.back();
  stream_undo_.pop_back();
  stream_undo_slot_.pop_back();
  for (size_t i = 0; i < prev.size(); ++i) writeRowValue(static_cast<int>(i), prev[i]);
  if (slot >= 0 && slot < static_cast<int>(stream_filled_.size())) stream_filled_[slot] = false;
  stream_slot_ = slot;
  if (hist_list_->count() > 0) delete hist_list_->takeItem(0);
  refreshStreamSlot();
  refreshWordPreview();
}

void SeriWrapRawComponent::applyBulkValues() {
  if (!value_edit_) return;
  const QString text = value_edit_->text().trimmed();
  if (text.isEmpty()) return;
  std::vector<uint64_t> vals;
  for (const QString &tok : text.split(',', Qt::SkipEmptyParts)) {
    uint64_t v = 0;
    vals.push_back(parseValue(tok, &v) ? v : 0ULL);
  }
  for (size_t i = 0; i < in_edits_.size(); ++i) {
    if (i < vals.size()) writeRowValue(static_cast<int>(i), vals[i]);
  }
  markSlotsFilledForPorts(static_cast<int>(std::min(vals.size(), in_edits_.size())));
  value_edit_->clear();
}

void SeriWrapRawComponent::markSlotsFilledForPorts(int count) {
  if (count <= 0) return;
  if (gran_ == Granularity::Port) {
    for (int p = 0; p < count && p < static_cast<int>(stream_filled_.size()); ++p) {
      stream_filled_[p] = true;
    }
    return;
  }
  // word granularity: a port fills every word whose packing mentions it
  for (int p = 0; p < count; ++p) {
    const int lo = portWordLo(p);
    const int hi = portWordHi(p);
    for (int w = lo; w >= 0 && w <= hi && w < static_cast<int>(stream_filled_.size()); ++w) {
      stream_filled_[w] = true;
    }
  }
}

void SeriWrapRawComponent::markSlotsFilledForPortRow(int port) {
  if (port < 0) return;
  if (gran_ == Granularity::Port) {
    if (port < static_cast<int>(stream_filled_.size())) stream_filled_[port] = true;
    return;
  }
  const int lo = portWordLo(port);
  const int hi = portWordHi(port);
  for (int w = lo; w >= 0 && w <= hi && w < static_cast<int>(stream_filled_.size()); ++w) {
    stream_filled_[w] = true;
  }
}

void SeriWrapRawComponent::collectInputs() {
  pending_inputs_ = inputValues();
  if (pending_inputs_.empty()) pending_inputs_.assign(1, 0ULL);
}

// ---------------------------------------------------------------------------
// widgets -> protocol
// ---------------------------------------------------------------------------
void SeriWrapRawComponent::onSendClicked() {
  applyBulkValues();
  collectInputs();
  // In the streaming editor a half-typed frame is easy to send by accident and
  // the kernel would then run on stale words; in the port table every port has
  // a visible value, so Send is always meaningful there.
  if (view_ == InputView::Streaming && !streamFull()) {
    appendLog(QString("Send refused: the streaming frame is only %1/%2 filled")
                  .arg(filledSlots())
                  .arg(streamSlotCount()));
    refreshStreamSlot();
    return;
  }
  frame_armed_ = true;
}

void SeriWrapRawComponent::onLoadManifestClicked() {
  const QString path = QFileDialog::getOpenFileName(
      this, "Select SeriWrap stream manifest", QString(), "Manifest (*.json)");
  if (path.isEmpty()) return;
  QString error;
  if (!loadManifestFile(path, &error)) {
    QMessageBox::warning(this, "SeriWrap manifest", error);
  }
}

void SeriWrapRawComponent::onViewClicked() {
  setInputView(view_ == InputView::Streaming ? InputView::Ports : InputView::Streaming);
}

void SeriWrapRawComponent::onGranularityClicked() {
  setGranularity(gran_ == Granularity::Word ? Granularity::Port : Granularity::Word);
}

void SeriWrapRawComponent::onStreamReturn() {
  if (!stream_edit_) return;
  const QString text = stream_edit_->text().trimmed();
  if (text.isEmpty()) return;
  QString error;
  int applied = 0;
  for (const QString &tok : text.split(',', Qt::SkipEmptyParts)) {
    if (!commitStreamValue(tok, &error)) {
      stream_edit_->setStyleSheet("border: 1px solid red;");
      appendLog("streaming: " + error);
      refreshStreamSlot();
      return;
    }
    ++applied;
  }
  Q_UNUSED(applied);
  stream_edit_->setStyleSheet("");
  stream_edit_->clear();
}

void SeriWrapRawComponent::onUndoClicked() { undoStreamValue(); }

void SeriWrapRawComponent::onClearClicked() {
  stream_undo_.clear();
  stream_undo_slot_.clear();
  stream_slot_ = 0;
  const int n = streamSlotCount();
  stream_filled_.assign(static_cast<size_t>(n), false);
  for (size_t i = 0; i < in_edits_.size(); ++i) writeRowValue(static_cast<int>(i), 0);
  if (hist_list_) hist_list_->clear();
  refreshStreamSlot();
  refreshWordPreview();
}

void SeriWrapRawComponent::onHoldChanged(int value) { setNumberSetting("hold_frames", value); }

// ---------------------------------------------------------------------------
// protocol side
// ---------------------------------------------------------------------------
uint64_t SeriWrapRawComponent::getWriteData() const {
  if (!proto_) return 0;
  // A finished frame stays in State::Done until the next one starts, so "may I
  // start a frame" is Idle *or* Done -- checking busy() only would refuse every
  // frame after the first one (Send frame worked once, then went silent).
  const auto st = proto_->state();
  const bool can_start = st == seriwrap::SeriWrapProtocol::State::Idle ||
                         st == seriwrap::SeriWrapProtocol::State::Done;
  if (can_start) {
    if (frame_armed_) {
      // Take the values from the editor as they are *now*, so Send always sends
      // what the tables/stream show.
      pending_inputs_ = inputValues();
      proto_->startFrame(pending_inputs_);
      frame_armed_ = false;
      frame_reported_ = false;
      ++frames_sent_;
    } else {
      return 0;   // quiet line
    }
  }
  return proto_->nextWriteWord();
}

void SeriWrapRawComponent::processReadData(QQueue<uint64_t> &read_queue) {
  if (!proto_) {
    read_queue.clear();
    return;
  }
  while (!read_queue.isEmpty()) {
    proto_->processRead(read_queue.dequeue());
  }
  const bool done = proto_->outputsReady() &&
                    proto_->state() == seriwrap::SeriWrapProtocol::State::Done;
  if (done && !frame_reported_) {
    frame_reported_ = true;   // once per frame, not once per host access
    ++frames_done_;
    QString line = QString("RX frame %1: ").arg(frames_done_);
    for (int i = 0; i < cfg_.n_out_ports; ++i) {
      if (i) line += ", ";
      line += hex(proto_->outputPort(i));
    }
    line += QString("   [%1 words:").arg(proto_->receivedWords().size());
    for (size_t i = 0; i < proto_->receivedWords().size() && i < 16; ++i) {
      line += QString(" %1").arg(proto_->receivedWords()[i], 2, 16, QChar('0'));
    }
    line += "]";
    appendLog(line);
  }
  refreshLabels();
}

void SeriWrapRawComponent::reset() {
  if (proto_) proto_->reset();
  frame_armed_ = false;
  frames_sent_ = 0;
  frames_done_ = 0;
  frame_reported_ = false;
  if (log_list_) log_list_->clear();
  if (hist_list_) hist_list_->clear();
  stream_undo_.clear();
  stream_undo_slot_.clear();
  stream_slot_ = 0;
  stream_filled_.assign(static_cast<size_t>(streamSlotCount()), false);
  for (size_t i = 0; i < in_edits_.size(); ++i) writeRowValue(static_cast<int>(i), 0);
  if (word_preview_) {
    refreshWordPreview();
  }
  refreshStreamSlot();
  refreshLabels();
}

void SeriWrapRawComponent::refreshLabels() {
  if (!status_label_ || !proto_) return;
  const QString mode = cfg_.sync_mode ? "sync" : "async";
  status_label_->setText(QString("SeriWrap [%1] sent=%2 done=%3  %4")
                             .arg(mode)
                             .arg(frames_sent_)
                             .arg(frames_done_)
                             .arg(QString::fromStdString(proto_->statusText())));
  if (ready_dot_) {
    ready_dot_->setText(QString("READY  %1").arg(proto_->readySeen() ? "●" : "○"));
    ready_dot_->setStyleSheet(proto_->readySeen() ? "font-size:11px; color:#0a0;"
                                                  : "font-size:11px; color:#888;");
  }
  if (valid_dot_) {
    const bool valid = proto_->outputWordsReceived() > 0;
    valid_dot_->setText(QString("VALID  %1").arg(valid ? "●" : "○"));
    valid_dot_->setStyleSheet(valid ? "font-size:11px; color:#08f;"
                                    : "font-size:11px; color:#888;");
  }

  QString out = "out: ";
  for (int i = 0; i < cfg_.n_out_ports; ++i) {
    if (i) out += ", ";
    out += hex(proto_->outputPort(i));
  }
  if (out_label_) out_label_->setText(out);
  if (raw_label_) {
    QString raw = QString("原始字 (%1): ").arg(proto_->receivedWords().size());
    for (size_t i = 0; i < proto_->receivedWords().size(); ++i) {
      raw += QString("%1 ").arg(proto_->receivedWords()[i], 2, 16, QChar('0'));
    }
    raw_label_->setText(raw);
  }
  // Same numbers, one row per output port, so a 16-port kernel stays readable.
  for (size_t i = 0; i < out_values_.size(); ++i) {
    if (!out_values_[i]) continue;
    out_values_[i]->setText(
        proto_ ? hex(proto_->outputPort(static_cast<int>(i))) : QString("-"));
  }
  refreshStreamSlot();
}

void SeriWrapRawComponent::appendLog(const QString &line) const {
  if (!log_list_) return;
  log_list_->insertItem(0, line);
  while (log_list_->count() > 100) delete log_list_->takeItem(log_list_->count() - 1);
}

void SeriWrapRawComponent::paintEvent(QPaintEvent *) {
  QPainter p(this);
  p.setPen(Qt::black);
  p.setBrush(Qt::white);
  p.drawRect(rect());
}

QList<NumberSettingInfo> SeriWrapRawComponent::numberSettings() const {
  return {{"hold_frames", "Hold Frames", 1, 200, hold_frames_}};
}

void SeriWrapRawComponent::setNumberSetting(const QString &key, int value) {
  if (key == "hold_frames") {
    hold_frames_ = value;
    if (proto_) proto_->set_hold_frames(hold_frames_);
    if (hold_spin_ && hold_spin_->value() != value) {
      hold_spin_->blockSignals(true);
      hold_spin_->setValue(value);
      hold_spin_->blockSignals(false);
    }
  }
}

COMPONENT_CLASS_DEFINITION(SeriWrap, 18, 8)

void SeriWrapComponent::onSettingsBtnClicked() {
  // ArrayPortMapping gives the per-port dropdowns that bind DATA[k]/CLK/... to
  // the wrapper's s_data_in[k]/s_clk_in/...; NumberSetting exposes the phase
  // hold.  A whole project can also be pre-bound by
  // SeriWrap/tools/gen_rabbit_project.py, which writes the .rbtprj for you.
  auto *dlg = new ComponentSettingsDialogWithFeatures<
      SettingsFeature::ArrayPortMapping, SettingsFeature::NumberSetting>(this, this);
  dlg->exec();
  delete dlg;
}
