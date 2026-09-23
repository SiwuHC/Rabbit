#include "Components/SeriWrapComponent.h"

#include "Components/ComponentSettingsDialog.h"

#include <QFileDialog>
#include <QFont>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QPainter>
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

}  // namespace

SeriWrapRawComponent::SeriWrapRawComponent(QWidget *parent)
    : AbstractRawComponent(parent) {
  initPorts();

  auto *root = new QVBoxLayout(this);
  root->setContentsMargins(4, 4, 4, 4);
  root->setSpacing(3);

  status_label_ = new QLabel("SeriWrap: load a manifest", this);
  status_label_->setStyleSheet("font-size:11px; font-weight:bold;");
  root->addWidget(status_label_);

  value_edit_ = new QLineEdit(this);
  value_edit_->setPlaceholderText("kernel input values, comma separated (dec or 0x..)");
  value_edit_->setStyleSheet("font-size:11px;");
  root->addWidget(value_edit_);

  auto *btn_row = new QHBoxLayout();
  send_btn_ = new QPushButton("Send frame", this);
  manifest_btn_ = new QPushButton("Manifest...", this);
  auto_repeat_ = new QCheckBox("repeat", this);
  btn_row->addWidget(send_btn_);
  btn_row->addWidget(manifest_btn_);
  btn_row->addWidget(auto_repeat_);
  root->addLayout(btn_row);

  out_label_ = new QLabel("out: -", this);
  out_label_->setStyleSheet("font-size:11px;");
  out_label_->setWordWrap(true);
  root->addWidget(out_label_);

  log_list_ = new QListWidget(this);
  log_list_->setStyleSheet("font-size:10px;");
  root->addWidget(log_list_);

  connect(send_btn_, &QPushButton::clicked, this, &SeriWrapRawComponent::onSendClicked);
  connect(manifest_btn_, &QPushButton::clicked, this,
          &SeriWrapRawComponent::onLoadManifestClicked);
  connect(auto_repeat_, &QCheckBox::toggled, this,
          &SeriWrapRawComponent::onAutoRepeatToggled);

  rebuildFromBindings();
}

SeriWrapRawComponent::~SeriWrapRawComponent() {}

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
    // component is still usable for a one-word kernel.  A real project should
    // load the manifest SeriWrap writes next to the wrapper
    // (<top>__stream_manifest.json) so the frame size and packing are exact.
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
    cfg_.in_packing = {{{0, bound - 1, 0, bound - 1, 0}}};
    cfg_.out_packing = {{{0, bound - 1, 0, bound - 1, 0}}};
  }

  proto_ = std::make_unique<seriwrap::SeriWrapProtocol>(cfg_, pins_);
  proto_->set_hold_frames(hold_frames_);
}

void SeriWrapRawComponent::reset() {
  if (proto_) proto_->reset();
  frame_armed_ = false;
  frames_sent_ = 0;
  frames_done_ = 0;
  if (log_list_) log_list_->clear();
  refreshLabels();
}

void SeriWrapRawComponent::onSendClicked() {
  // parse "v0, v1, ..." (decimal or 0x..) into the kernel input ports
  pending_inputs_.clear();
  const QString text = value_edit_ ? value_edit_->text() : QString();
  for (const QString &tok : text.split(',', Qt::SkipEmptyParts)) {
    bool ok = false;
    const QString t = tok.trimmed();
    const quint64 v = t.startsWith("0x", Qt::CaseInsensitive)
                          ? t.mid(2).toULongLong(&ok, 16)
                          : t.toULongLong(&ok, 10);
    pending_inputs_.push_back(ok ? v : 0ULL);
  }
  if (pending_inputs_.empty()) pending_inputs_.assign(1, 0ULL);
  frame_armed_ = true;
}

void SeriWrapRawComponent::onAutoRepeatToggled(bool on) { auto_repeat_on_ = on; }

void SeriWrapRawComponent::onLoadManifestClicked() {
  const QString path = QFileDialog::getOpenFileName(
      this, "SeriWrap manifest", QString(), "SeriWrap manifest (*manifest.json *.json)");
  if (path.isEmpty()) return;

  QFile f(path);
  if (!f.open(QIODevice::ReadOnly)) {
    QMessageBox::warning(this, "SeriWrap", "Cannot open " + path);
    return;
  }
  QJsonParseError err{};
  const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &err);
  if (err.error != QJsonParseError::NoError || !doc.isObject()) {
    QMessageBox::warning(this, "SeriWrap", "Invalid manifest: " + err.errorString());
    return;
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
  for (const auto &v : ins) c.in_port_names.push_back(v.toObject().value("name").toString().toStdString());
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
  appendLog(QString("manifest: %1 in-words, %2 out-words, %3-bit words, %4")
                .arg(c.input_words)
                .arg(c.output_words)
                .arg(c.word_width)
                .arg(c.sync_mode ? "sync" : "async"));
  refreshLabels();
}

uint64_t SeriWrapRawComponent::getWriteData() const {
  if (!proto_) return 0;
  if (!proto_->busy()) {
    if (frame_armed_ || auto_repeat_on_) {
      proto_->startFrame(pending_inputs_);
      frame_armed_ = false;
      ++frames_sent_;
    } else {
      return 0;
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
  if (proto_->outputsReady() && proto_->state() == seriwrap::SeriWrapProtocol::State::Done) {
    ++frames_done_;
    QString line = QString("frame %1: ").arg(frames_done_);
    for (int i = 0; i < cfg_.n_out_ports; ++i) {
      if (i) line += ", ";
      line += QString("0x%1").arg(proto_->outputPort(i), 0, 16);
    }
    appendLog(line);
  }
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
  QString out = "out: ";
  for (int i = 0; i < cfg_.n_out_ports; ++i) {
    if (i) out += ", ";
    out += QString("0x%1").arg(proto_->outputPort(i), 0, 16);
  }
  out_label_->setText(out);
}

void SeriWrapRawComponent::appendLog(const QString &line) {
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
  }
}

COMPONENT_CLASS_DEFINITION(SeriWrap, 5, 4)

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
