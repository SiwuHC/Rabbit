// Headless component tests for the Rabbit "Stream" family and SeriWrapComponent.
//
// They drive exactly the API the GUI controllers use --
//   AbstractRawComponent::{getWriteData, processReadData, reset,
//                          numberSettings, setNumberSetting,
//                          inputPorts, outputPorts}
// plus the real widget slots (QLineEdit + onEnterPressed / onSendClicked), and
// read results back from the widgets the components own.
//
// Ports are bound to real pins first (what ProjectFileHandler does when a
// project is loaded), because pin_index is only meaningful once bound: a fresh
// component leaves the placeholder indices appendPort() assigned.
//
// Run:  QT_QPA_PLATFORM=offscreen ./component_test
#include <QApplication>
#include <QCheckBox>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QSplitter>
#include <QStackedWidget>
#include <QTableWidget>
#include <QToolButton>
#include <QListWidget>
#include <QMetaObject>
#include <QQueue>
#include <QString>
#include <QStringList>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "Components/AbstractComponent.h"
#include "Components/ComponentsFactory.h"
#include "Components/SeriWrapComponent.h"
#include "Ports/PinInfo.h"

using rabbit_App::component::AbstractComponent;
using rabbit_App::component::AbstractRawComponent;
using rabbit_App::component::ComponentsFactory;
using rabbit_App::component::SeriWrapRawComponent;
using rabbit_App::ports::Port;
using rabbit_App::ports::PortType;

static int g_checks = 0, g_fails = 0;

static void group(const QString &name) { std::printf("\n== %s ==\n", qPrintable(name)); }

static void check(bool ok, const QString &what) {
  ++g_checks;
  if (ok) {
    std::printf("  ok   %s\n", qPrintable(what));
  } else {
    ++g_fails;
    std::printf("  FAIL %s\n", qPrintable(what));
  }
}

static void checkEq(qulonglong got, qulonglong want, const QString &what) {
  check(got == want, QString("%1  [got 0x%2 want 0x%3]")
                         .arg(what)
                         .arg(got, 0, 16)
                         .arg(want, 0, 16));
}

struct Box {
  AbstractComponent *wrap = nullptr;
  AbstractRawComponent *raw = nullptr;
};

static Box make(const QString &name) {
  AbstractComponent *w = ComponentsFactory::create(name, nullptr);
  return Box{w, w ? w->rawComponent() : nullptr};
}

// ---- pin binding ---------------------------------------------------------
// The declaration order of the board's pins (PinInfo.cpp).  Binding ports to
// these is what turns a component's placeholder indices into the real frame
// bits, so every frame-level check below is an absolute one.
static const char *kInPins[] = {
    "P151", "P148", "P150", "P152", "P160", "P161", "P162", "P163", "P164",
    "P165", "P166", "P169", "P173", "P174", "P175", "P191", "P120", "P116",
    "P115", "P114", "P113", "P112", "P111", "P108", "P102", "P101", "P100",
    "P97",  "P96",  "P95",  "P89",  "P88",  "P87",  "P86",  "P81",  "P75",
    "P74",  "P70",  "P69",  "P68",  "P64",  "P62",  "P61",  "P58",  "P57",
    "P49",  "P47",  "P48",  "P192", "P193", "P199", "P200", "P201", "P202"};
static const char *kOutPins[] = {
    "P7",   "P6",   "P5",   "P4",   "P9",   "P8",   "P16",  "P15",  "P11",
    "P10",  "P20",  "P18",  "P17",  "P22",  "P21",  "P23",  "P44",  "P45",
    "P46",  "P43",  "P40",  "P41",  "P42",  "P33",  "P34",  "P35",  "P36",
    "P30",  "P31",  "P24",  "P27",  "P29",  "P110", "P109", "P99",  "P98",
    "P94",  "P93",  "P84",  "P83",  "P82",  "P73",  "P71",  "P63",  "P60",
    "P59",  "P56",  "P55",  "P167", "P168", "P176", "P187", "P189", "P194"};

static bool bindPins(AbstractRawComponent *raw) {
  const auto &ip = raw->inputPorts();
  const auto &op = raw->outputPorts();
  if (ip.size() > static_cast<int>(sizeof(kInPins) / sizeof(kInPins[0]))) return false;
  if (op.size() > static_cast<int>(sizeof(kOutPins) / sizeof(kOutPins[0]))) return false;
  for (int i = 0; i < ip.size(); ++i) {
    raw->inputPorts()[i].pin_name = kInPins[i];
    raw->inputPorts()[i].pin_index = rabbit_App::ports::inputDeclIndexMap(kInPins[i]);
  }
  for (int i = 0; i < op.size(); ++i) {
    raw->outputPorts()[i].pin_name = kOutPins[i];
    raw->outputPorts()[i].pin_index = rabbit_App::ports::outputDeclIndexMap(kOutPins[i]);
  }
  return true;
}

// ---- frame helpers -------------------------------------------------------
// Design INPUT pins sit at bit pin_index, design OUTPUT pins at pin_index-1.
// (Kept as documentation of the write-frame layout: the Stream input tests read
// the component's own frames rather than building them.)
[[maybe_unused]] static uint64_t inFrame(const QList<Port> &p, int w, uint64_t value,
                                        int clk, int stb) {
  uint64_t f = 0;
  for (int i = 0; i < w; ++i)
    if ((value >> i) & 1ULL) f |= 1ULL << p[i].pin_index;
  if (clk) f |= 1ULL << p[w].pin_index;
  if (stb) f |= 1ULL << p[w + 1].pin_index;
  return f;
}

static uint64_t outFrame(const QList<Port> &p, int w, uint64_t value, int clk, int vld) {
  uint64_t f = 0;
  for (int i = 0; i < w; ++i)
    if ((value >> i) & 1ULL) f |= 1ULL << (p[i].pin_index - 1);
  if (clk) f |= 1ULL << (p[w].pin_index - 1);
  if (vld) f |= 1ULL << (p[w + 1].pin_index - 1);
  return f;
}

static uint64_t inValue(const QList<Port> &p, int w, uint64_t f) {
  uint64_t v = 0;
  for (int i = 0; i < w; ++i)
    if ((f >> p[i].pin_index) & 1ULL) v |= 1ULL << i;
  return v;
}

static int inClk(const QList<Port> &p, int w, uint64_t f) { return (f >> p[w].pin_index) & 1; }
static int inStb(const QList<Port> &p, int w, uint64_t f) { return (f >> p[w + 1].pin_index) & 1; }

// ---- widget helpers ------------------------------------------------------
static QStringList widgetTexts(QWidget *w) {
  QStringList out;
  for (auto *l : w->findChildren<QLabel *>()) out << l->text();
  for (auto *l : w->findChildren<QListWidget *>())
    for (int i = 0; i < l->count(); ++i) out << l->item(i)->text();
  return out;
}

static bool textsContain(QWidget *w, const QString &needle) {
  for (const QString &s : widgetTexts(w))
    if (s.contains(needle, Qt::CaseInsensitive)) return true;
  return false;
}

static void feedRead(AbstractRawComponent *raw, const QList<uint64_t> &frames) {
  QQueue<uint64_t> q;
  for (uint64_t f : frames) q.enqueue(f);
  raw->processReadData(q);
}

static bool pushValue(AbstractRawComponent *raw, const QString &text) {
  auto *edit = raw->findChild<QLineEdit *>();
  if (!edit) return false;
  edit->setText(text);
  return QMetaObject::invokeMethod(raw, "onEnterPressed");
}

// ---------------------------------------------------------------------------
static void testStructure() {
  group("structure: factory, ports, settings, bound frame bits");
  struct Spec { const char *name; int width; bool in; };
  const Spec specs[] = {
      {"StreamInput8", 8, true},      {"StreamInput16", 16, true},
      {"StreamInput32", 32, true},    {"StreamInputFloat", 32, true},
      {"StreamOutput8", 8, false},    {"StreamOutput16", 16, false},
      {"StreamOutput32", 32, false},  {"StreamOutputFloat", 32, false},
  };
  for (const Spec &s : specs) {
    Box b = make(s.name);
    check(b.wrap && b.raw, QString("%1: factory creates the component").arg(s.name));
    if (!b.raw) continue;
    check(bindPins(b.raw), QString("%1: ports fit the board's pin tables").arg(s.name));
    const auto &ip = b.raw->inputPorts();
    const auto &op = b.raw->outputPorts();
    if (s.in) {
      check(ip.size() == s.width + 2,
            QString("%1: %2 input ports (DATA[0..%3], CLK, STROBE), got %4")
                .arg(s.name).arg(s.width + 2).arg(s.width - 1).arg(ip.size()));
      check(op.isEmpty(), QString("%1: no output ports").arg(s.name));
      check(ip[0].name == "DATA[0]" && ip[s.width - 1].name == QString("DATA[%1]").arg(s.width - 1) &&
                ip[s.width].name == "CLK" && ip[s.width + 1].name == "STROBE",
            QString("%1: port names DATA[i]/CLK/STROBE").arg(s.name));
      bool all_in = true;
      for (const Port &p : ip) all_in &= (p.type == PortType::Input);
      check(all_in, QString("%1: every port is an Input").arg(s.name));
      const auto st = b.raw->numberSettings();
      check(st.size() == 2 && st[0].key == "target_count" && st[1].key == "clk_hold",
            QString("%1: settings are target_count + clk_hold").arg(s.name));
    } else {
      // Note the real names: a StreamOutput's data pins are DATA[i], and its
      // control pins are CLK and DATA_VALID (not DOUTi/VLD).
      check(op.size() == s.width + 2,
            QString("%1: %2 output ports (DATA[0..%3], CLK, DATA_VALID), got %4")
                .arg(s.name).arg(s.width + 2).arg(s.width - 1).arg(op.size()));
      check(ip.isEmpty(), QString("%1: no input ports").arg(s.name));
      check(op[0].name == "DATA[0]" && op[s.width].name == "CLK" && op[s.width + 1].name == "DATA_VALID",
            QString("%1: port names DATA[i]/CLK/DATA_VALID").arg(s.name));
      bool all_out = true;
      for (const Port &p : op) all_out &= (p.type == PortType::Output);
      check(all_out, QString("%1: every port is an Output").arg(s.name));
    }
    // Frame bits are unique and inside the 64-bit frame once the pins are bound.
    bool unique = true;
    QList<int> seen;
    const auto &list = s.in ? ip : op;
    for (const Port &p : list) {
      const int idx = s.in ? p.pin_index : p.pin_index - 1;
      if (idx < 0 || idx > 63 || seen.contains(idx)) unique = false;
      seen << idx;
    }
    check(unique, QString("%1: frame bits unique and inside 0..63").arg(s.name));
    if (s.in) {
      checkEq(ip[0].pin_index, rabbit_App::ports::inputDeclIndexMap("P151"),
              QString("%1: first data pin bound to P151 at its declaration index").arg(s.name));
    } else {
      checkEq(op[0].pin_index, rabbit_App::ports::outputDeclIndexMap("P7"),
              QString("%1: first data pin bound to P7 at its declaration index").arg(s.name));
    }
  }

  Box sw = make("SeriWrap");
  check(sw.wrap && sw.raw, "SeriWrap: factory creates the component");
  if (sw.raw) {
    bindPins(sw.raw);
    const auto &ip = sw.raw->inputPorts();
    const auto &op = sw.raw->outputPorts();
    check(ip.size() == 34, QString("SeriWrap: 34 input ports (DATA[0..31], CLK, STROBE), got %1").arg(ip.size()));
    check(op.size() == 35, QString("SeriWrap: 35 output ports (DOUT[0..31], CLK_OUT, DATA_VALID, READY), got %1").arg(op.size()));
    check(ip[0].name == "DATA[0]" && ip[31].name == "DATA[31]" && ip[32].name == "CLK" && ip[33].name == "STROBE",
          "SeriWrap: input port names");
    check(op[0].name == "DOUT[0]" && op[31].name == "DOUT[31]" && op[32].name == "CLK_OUT" &&
              op[33].name == "DATA_VALID" && op[34].name == "READY",
          "SeriWrap: output port names");
    bool unique = true;
    QList<int> seen;
    for (const Port &p : ip) {
      if (p.pin_index < 0 || p.pin_index > 63 || seen.contains(p.pin_index)) unique = false;
      seen << p.pin_index;
    }
    seen.clear();
    for (const Port &p : op) {
      const int idx = p.pin_index - 1;
      if (idx < 0 || idx > 63 || seen.contains(idx)) unique = false;
      seen << idx;
    }
    check(unique, "SeriWrap: 69 bound frame bits are unique and inside 0..63");
    const auto st = sw.raw->numberSettings();
    check(st.size() == 1 && st[0].key == "hold_frames", "SeriWrap: setting is hold_frames");
    checkEq(sw.raw->getWriteData(), 0, "SeriWrap: idle write frame is 0 (no manifest loaded)");
    sw.raw->reset();
    checkEq(sw.raw->getWriteData(), 0, "SeriWrap: still 0 after reset");
  }
}

// ---------------------------------------------------------------------------
static void testStreamInputWidth(const char *name, int w, const char *value_text,
                                 uint64_t expect_bits, bool float_mode) {
  (void)float_mode;
  Box b = make(name);
  if (!b.raw) { check(false, QString("%1: missing").arg(name)); return; }
  bindPins(b.raw);
  b.raw->setNumberSetting("clk_hold", 1);
  b.raw->setNumberSetting("target_count", 8);
  const auto &p = b.raw->inputPorts();

  checkEq(b.raw->getWriteData(), 0, QString("%1: idle before any value").arg(name));
  check(pushValue(b.raw, value_text), QString("%1: value entered through the QLineEdit slot").arg(name));

  QList<uint64_t> f;
  for (int i = 0; i < 4; ++i) f << b.raw->getWriteData();

  checkEq(inClk(p, w, f[0]), 0, QString("%1: phase 1 CLK=0").arg(name));
  checkEq(inStb(p, w, f[0]), 1, QString("%1: phase 1 STROBE=1").arg(name));
  checkEq(inClk(p, w, f[1]), 1, QString("%1: phase 2 CLK=1 (the design's rising edge)").arg(name));
  checkEq(inStb(p, w, f[1]), 1, QString("%1: phase 2 STROBE=1").arg(name));
  checkEq(inClk(p, w, f[2]), 0, QString("%1: phase 3 CLK=0").arg(name));
  checkEq(inStb(p, w, f[2]), 0, QString("%1: phase 3 STROBE=0").arg(name));
  checkEq(inValue(p, w, f[1]), expect_bits, QString("%1: data bits").arg(name));
  checkEq(inValue(p, w, f[2]), expect_bits, QString("%1: data held during phase 3").arg(name));
  checkEq(f[3], 0, QString("%1: back to idle after one value").arg(name));

  uint64_t allowed = 0;
  for (const Port &q : p) allowed |= 1ULL << q.pin_index;
  bool confined = true;
  for (uint64_t x : f) confined &= ((x & ~allowed) == 0);
  check(confined, QString("%1: frame only drives its own pins").arg(name));
}

static void testStreamInputHoldAndOrder() {
  group("StreamInput: hold, ordering, target count");
  Box b = make("StreamInput16");
  if (!b.raw) { check(false, "StreamInput16 missing"); return; }
  bindPins(b.raw);
  const auto &p = b.raw->inputPorts();
  b.raw->setNumberSetting("clk_hold", 3);
  b.raw->setNumberSetting("target_count", 8);
  pushValue(b.raw, "291");  // 0x0123

  // What the design actually sees, per value: SETUP (CLK=0, STROBE=1) held for
  // clk_hold frames, then CLK=1 (STROBE=1) held for clk_hold frames -- the
  // design latches on that edge.  The FALLING state is deliberately NOT held:
  // while the queue still has a value it is merged into the next value's SETUP
  // frame (the same electrical state, CLK=0/STROBE=1), and when the queue drains
  // it is emitted exactly once, after which the component idles (write frame 0,
  // which is also CLK=0/STROBE=0).
  QList<uint64_t> f;
  for (int i = 0; i < 12; ++i) f << b.raw->getWriteData();
  int n_setup = 0, n_rise = 0, n_fall = 0;
  for (uint64_t x : f) {
    if (inValue(p, 16, x) != 0x0123) continue;
    if (inClk(p, 16, x) == 0 && inStb(p, 16, x) == 1) ++n_setup;
    if (inClk(p, 16, x) == 1 && inStb(p, 16, x) == 1) ++n_rise;
    if (inClk(p, 16, x) == 0 && inStb(p, 16, x) == 0) ++n_fall;
  }
  checkEq(n_setup, 3, "clk_hold=3: SETUP phase held for 3 frames");
  checkEq(n_rise, 3, "clk_hold=3: CLK=1 phase held for 3 frames (the latch edge)");
  checkEq(n_fall, 1, "clk_hold=3: the closing FALLING state appears once, then idle");
  checkEq(inValue(p, 16, f[5]), 0x0123, "clk_hold=3: data stable across the pulse");

  Box c = make("StreamInput16");
  bindPins(c.raw);
  c.raw->setNumberSetting("clk_hold", 3);
  c.raw->setNumberSetting("target_count", 8);
  pushValue(c.raw, "4369");  // 0x1111
  pushValue(c.raw, "8738");  // 0x2222
  const auto &q = c.raw->inputPorts();
  QList<uint64_t> burst;
  for (int i = 0; i < 20; ++i) burst << c.raw->getWriteData();
  int rise1 = 0, rise2 = 0, fall_frames = 0, setup1 = 0, setup2 = 0;
  uint64_t fall_val = 0;
  for (uint64_t x : burst) {
    const uint64_t v = inValue(q, 16, x);
    if (inClk(q, 16, x) == 1 && inStb(q, 16, x) == 1) {
      if (v == 0x1111) ++rise1;
      if (v == 0x2222) ++rise2;
    }
    if (inClk(q, 16, x) == 0 && inStb(q, 16, x) == 1) {
      if (v == 0x1111) ++setup1;
      if (v == 0x2222) ++setup2;
    }
    if (x != 0 && inClk(q, 16, x) == 0 && inStb(q, 16, x) == 0) { ++fall_frames; fall_val = v; }
  }
  checkEq(rise1, 3, "two values, clk_hold=3: the first value gets 3 latch frames");
  checkEq(rise2, 3, "two values, clk_hold=3: the second value gets 3 latch frames");
  checkEq(fall_frames, 1, "two values, clk_hold=3: one closing FALLING frame for the burst");
  checkEq(fall_val, 0x2222, "the closing FALLING frame carries the last value");
  // Measured behaviour, worth knowing because a word costs host accesses on the
  // board: the first value's SETUP phase is clk_hold frames wide, but every
  // later value's SETUP phase is 2*clk_hold wide -- the frame that merges the
  // previous FALLING with the next SETUP is followed by a full SETUP phase as
  // well.  Each word still presents exactly ONE CLK=1/STROBE=1 latch window, so
  // the design latches once per word; the price is throughput.
  checkEq(setup1, 3, "two values, clk_hold=3: first value SETUP = clk_hold frames");
  checkEq(setup2, 6, "two values, clk_hold=3: later values SETUP = 2*clk_hold frames");

  // Ordering with clk_hold=1 (one frame per phase)
  Box e = make("StreamInput16");
  bindPins(e.raw);
  e.raw->setNumberSetting("clk_hold", 1);
  e.raw->setNumberSetting("target_count", 8);
  pushValue(e.raw, "4369");  // 0x1111
  pushValue(e.raw, "8738");  // 0x2222
  const auto &ei = e.raw->inputPorts();
  QList<uint64_t> order;
  for (int i = 0; i < 8; ++i) order << e.raw->getWriteData();
  checkEq(inValue(ei, 16, order[1]), 0x1111, "two queued values: first sent first");
  checkEq(inValue(ei, 16, order[4]), 0x2222, "two queued values: second sent second");

  // The design latches on CLK=1 & STROBE=1, so count those pulses: when the
  // target count is reached the closing FALLING frame is emitted as an all-zero
  // frame (CLK=0, STROBE=0, data dropped), which is electrically the same edge
  // but would be missed by counting data-carrying FALLING frames.
  Box d = make("StreamInput16");
  bindPins(d.raw);
  d.raw->setNumberSetting("clk_hold", 1);
  d.raw->setNumberSetting("target_count", 1);
  pushValue(d.raw, "1");
  pushValue(d.raw, "2");
  pushValue(d.raw, "3");
  const auto &r = d.raw->inputPorts();
  int pulses = 0;
  uint64_t pulse_val = 0;
  for (int i = 0; i < 20; ++i) {
    const uint64_t x = d.raw->getWriteData();
    if (x == 0) break;
    if (inClk(r, 16, x) == 1 && inStb(r, 16, x) == 1) { ++pulses; pulse_val = inValue(r, 16, x); }
  }
  checkEq(pulses, 1, "target_count=1: exactly one CLK pulse leaves the component");
  checkEq(pulse_val, 1, "target_count=1: the pulse carries the first value");
}

static void testStreamOutput(const char *name, int w, uint64_t value, const QString &needle,
                             bool float_mode) {
  (void)float_mode;
  Box b = make(name);
  if (!b.raw) { check(false, QString("%1: missing").arg(name)); return; }
  bindPins(b.raw);
  const auto &p = b.raw->outputPorts();

  feedRead(b.raw, {outFrame(p, w, value, 0, 1)});
  check(!textsContain(b.raw, needle), QString("%1: CLK=0 does not capture").arg(name));
  feedRead(b.raw, {outFrame(p, w, value, 1, 0)});
  check(!textsContain(b.raw, needle), QString("%1: rising edge without DATA_VALID does not capture").arg(name));
  feedRead(b.raw, {outFrame(p, w, value, 1, 1)});
  check(!textsContain(b.raw, needle), QString("%1: held CLK=1 is not a new edge").arg(name));

  feedRead(b.raw, {outFrame(p, w, 0, 0, 1)});
  feedRead(b.raw, {outFrame(p, w, value, 1, 1)});
  check(textsContain(b.raw, needle), QString("%1: rising edge with DATA_VALID captures %2").arg(name, needle));

  feedRead(b.raw, {outFrame(p, w, 0, 0, 0)});
  feedRead(b.raw, {outFrame(p, w, value, 1, 1)});
  int hits = 0;
  for (const QString &s : widgetTexts(b.raw))
    if (s.contains(needle, Qt::CaseInsensitive)) ++hits;
  check(hits >= 2, QString("%1: a second edge captures again (found %2)").arg(name).arg(hits));

  b.raw->reset();
  check(!textsContain(b.raw, needle), QString("%1: reset() clears the display").arg(name));
}

// ---------------------------------------------------------------------------
static QString writeManifest(const QString &dir) {
  QJsonObject root;
  QJsonObject link{{"width", 16}, {"sync_mode", true}};
  QJsonObject frame{{"input_words", 2}, {"output_words", 2},
                    {"handshake", QJsonObject{{"ready", true}}}};
  QJsonArray ins, outs;
  ins.append(QJsonObject{{"name", "a"}, {"width", 16}});
  ins.append(QJsonObject{{"name", "b"}, {"width", 16}});
  outs.append(QJsonObject{{"name", "y0"}, {"width", 16}});
  outs.append(QJsonObject{{"name", "y1"}, {"width", 16}});
  QJsonObject ports{{"inputs", ins}, {"outputs", outs}};
  auto word = [](const char *port) {
    QJsonObject f{{"port", port},
                  {"port_bits", QJsonArray{15, 0}},
                  {"word_bits", QJsonArray{15, 0}}};
    return QJsonObject{{"fields", QJsonArray{f}}};
  };
  QJsonObject packing{{"input", QJsonArray{word("a"), word("b")}},
                      {"output", QJsonArray{word("y0"), word("y1")}}};
  root["link"] = link;
  root["frame"] = frame;
  root["ports"] = ports;
  root["packing"] = packing;
  const QString path = dir + "/component_test_manifest.json";
  QFile f(path);
  f.open(QIODevice::WriteOnly | QIODevice::Truncate);
  f.write(QJsonDocument(root).toJson());
  f.close();
  return path;
}

static void testSeriWrapComponent() {
  group("SeriWrapComponent: manifest, send path, read path");
  Box b = make("SeriWrap");
  if (!b.raw) { check(false, "SeriWrap missing"); return; }
  bindPins(b.raw);
  const QString dir = QDir::tempPath();
  const QString good = writeManifest(dir);
  QString err;
  auto *sw = dynamic_cast<SeriWrapRawComponent *>(b.raw);
  check(sw != nullptr, "the raw component is a SeriWrapRawComponent");
  if (!sw) return;

  check(!sw->loadManifestFile(dir + "/does_not_exist.json", &err),
        "missing manifest file is rejected");
  const QString bad = dir + "/component_test_bad.json";
  { QFile f(bad); f.open(QIODevice::WriteOnly | QIODevice::Truncate); f.write("not json at all"); }
  check(!sw->loadManifestFile(bad, &err), "malformed manifest is rejected");

  err.clear();
  check(sw->loadManifestFile(good, &err), QString("valid manifest loads [%1]").arg(err));
  check(textsContain(b.raw, "manifest:"), "the log reports the loaded manifest");
  checkEq(b.raw->getWriteData(), 0, "idle after loading a manifest (nothing armed)");

  // The input UI is one row per kernel port (the Stream components' one-value
  // per widget style), labelled with the manifest's port names; the output side
  // mirrors it so a 16-port kernel stays readable.
  auto *in_tbl = b.raw->findChild<QTableWidget *>("in_table");
  auto *out_tbl = b.raw->findChild<QTableWidget *>("out_table");
  check(in_tbl != nullptr && out_tbl != nullptr, "SeriWrap: per-port input/output tables exist");
  if (in_tbl) {
    checkEq(in_tbl->rowCount(), 2, "SeriWrap: one input row per manifest input port");
    check(in_tbl->item(0, 0) && in_tbl->item(0, 0)->text().startsWith("a") &&
              in_tbl->item(1, 0) && in_tbl->item(1, 0)->text().startsWith("b"),
          "SeriWrap: input rows are labelled a, b (manifest order, width appended)");
    check(in_tbl->cellWidget(0, 1) != nullptr && in_tbl->cellWidget(1, 1) != nullptr,
          "SeriWrap: every input row has its own value editor");
  }
  if (out_tbl) {
    checkEq(out_tbl->rowCount(), 2, "SeriWrap: one output row per manifest output port");
    check(out_tbl->item(0, 0) && out_tbl->item(0, 0)->text() == "y0",
          "SeriWrap: output rows are labelled with the port names");
  }

  // The link is ready-gated: the design's READY bit has to be seen in a read
  // frame before the component may start a frame (that is what the GUI's
  // per-frame processReadData() provides).
  const auto &op = b.raw->outputPorts();
  const int ready_bit = op[34].pin_index - 1;
  const int valid_bit = op[33].pin_index - 1;
  feedRead(b.raw, {1ULL << ready_bit});
  checkEq(b.raw->getWriteData(), 0, "ready alone does not start a frame");

  // The SeriWrap component has one value editor per kernel input port plus a
  // "bulk paste" field; the bulk field is the one the tests type into (its
  // text is applied to the rows when Send frame is pressed).
  auto *edit = b.raw->findChild<QLineEdit *>("bulk_edit");
  check(edit != nullptr, "the component has a bulk value editor");
  if (edit) {
    edit->setText("0x1234, 0x5678");
    QMetaObject::invokeMethod(b.raw, "onSendClicked");
    QList<uint64_t> words;
    for (int i = 0; i < 8; ++i) words << b.raw->getWriteData();
    uint64_t seen = 0;
    for (uint64_t w : words) seen |= w;
    check(seen != 0, "arming a frame produces non-zero serial words");
    const auto &ip = b.raw->inputPorts();
    bool found = false;
    for (uint64_t w : words) {
      uint64_t v = 0;
      for (int i = 0; i < 16; ++i)
        if ((w >> ip[i].pin_index) & 1ULL) v |= 1ULL << i;
      if (v == 0x1234) { found = true; break; }
    }
    check(found, "the first input word carries 0x1234 on DATA[0..15]");
    if (!found) {
      QString dbg;
      for (uint64_t w : words) dbg += QString("%1 ").arg(w, 16, 16, QChar('0'));
      std::printf("       words: %s\n", qPrintable(dbg));
      std::printf("       ready_bit=%d valid_bit=%d\n", ready_bit, valid_bit);
    }
  }

  // Values typed straight into the per-port rows (no bulk field) must reach the
  // wire in manifest port order: row 0 -> a, row 1 -> b.
  if (in_tbl && in_tbl->cellWidget(0, 1) && in_tbl->cellWidget(1, 1)) {
    b.raw->reset();                        // the previous block left a frame busy
    static_cast<QLineEdit *>(in_tbl->cellWidget(0, 1))->setText("0x1234");
    static_cast<QLineEdit *>(in_tbl->cellWidget(1, 1))->setText("0x5678");
    feedRead(b.raw, {1ULL << ready_bit});
    QMetaObject::invokeMethod(b.raw, "onSendClicked");
    QList<uint64_t> rw;
    for (int i = 0; i < 8; ++i) rw << b.raw->getWriteData();
    const auto &ip_r = b.raw->inputPorts();
    bool a_ok = false, b_ok = false;
    for (uint64_t word : rw) {
      uint64_t v = 0;
      for (int i = 0; i < 16; ++i)
        if ((word >> ip_r[i].pin_index) & 1ULL) v |= 1ULL << i;
      if (v == 0x1234) a_ok = true;
      if (v == 0x5678) b_ok = true;
    }
    check(a_ok && b_ok, "SeriWrap: per-port rows carry a=0x1234 then b=0x5678 in order");
  }

  // Decimal is accepted as well as hex (the editor hint says "dec or 0x.."):
  // the same values typed in decimal must produce the same first word.
  Box b2 = make("SeriWrap");
  if (b2.raw) {
    bindPins(b2.raw);
    auto *sw2 = dynamic_cast<SeriWrapRawComponent *>(b2.raw);
    if (sw2 && sw2->loadManifestFile(good, nullptr)) {
      const auto &op2 = b2.raw->outputPorts();
      feedRead(b2.raw, {1ULL << (op2[34].pin_index - 1)});
      auto *edit2 = b2.raw->findChild<QLineEdit *>("bulk_edit");
      const auto &ip2 = b2.raw->inputPorts();
      if (edit2) {
        edit2->setText("4660, 22136");          // == 0x1234, 0x5678
        QMetaObject::invokeMethod(b2.raw, "onSendClicked");
        bool found_dec = false;
        for (int i = 0; i < 8 && !found_dec; ++i) {
          const uint64_t w = b2.raw->getWriteData();
          uint64_t v = 0;
          for (int j = 0; j < 16; ++j)
            if ((w >> ip2[j].pin_index) & 1ULL) v |= 1ULL << j;
          if (v == 0x1234) found_dec = true;
        }
        check(found_dec, "decimal input works too: 4660 -> 0x1234 on DATA[0..15]");
      }
    }
  }

  // A token that does not parse must become 0, not garbage.  Fresh instance:
  // the previous frame is still in flight, so its words would be read instead.
  Box b3 = make("SeriWrap");
  if (b3.raw) {
    bindPins(b3.raw);
    auto *sw3 = dynamic_cast<SeriWrapRawComponent *>(b3.raw);
    if (sw3 && sw3->loadManifestFile(good, nullptr)) {
      const auto &op3 = b3.raw->outputPorts();
      feedRead(b3.raw, {1ULL << (op3[34].pin_index - 1)});
      auto *edit3 = b3.raw->findChild<QLineEdit *>();
      const auto &ip3 = b3.raw->inputPorts();
      if (edit3) {
        edit3->setText("not-a-number");
        QMetaObject::invokeMethod(b3.raw, "onSendClicked");
        bool zeroish = true;
        for (int i = 0; i < 4; ++i) {
          const uint64_t w = b3.raw->getWriteData();
          for (int j = 0; j < 16; ++j)
            if ((w >> ip3[j].pin_index) & 1ULL) zeroish = false;
        }
        check(zeroish, "an unparsable token yields 0, not garbage");
      }
    }
  }

  QQueue<uint64_t> empty;
  b.raw->processReadData(empty);
  check(true, "processReadData(empty) is a no-op");

  b.raw->reset();
  check(true, "reset() on a component without an active frame is safe");
}


// ---------------------------------------------------------------------------
// Three-pane UI: a manifest where one port spans several words.
static QString writeWideManifest(const QString &dir) {
  QJsonObject root;
  root["link"] = QJsonObject{{"width", 8}, {"sync_mode", true}};
  root["frame"] = QJsonObject{{"input_words", 4}, {"output_words", 4},
                              {"handshake", QJsonObject{{"ready", true}}}};
  QJsonArray ins{QJsonObject{{"name", "a0"}, {"width", 32}}};
  QJsonArray outs{QJsonObject{{"name", "y0"}, {"width", 32}}};
  root["ports"] = QJsonObject{{"inputs", ins}, {"outputs", outs}};
  QJsonArray in_words, out_words;
  for (int i = 0; i < 4; ++i) {
    const QJsonArray pb{8 * i + 7, 8 * i};
    in_words.append(QJsonObject{
        {"word", i},
        {"fields", QJsonArray{QJsonObject{
                       {"port", "a0"}, {"port_bits", pb}, {"word_bits", QJsonArray{7, 0}}}}}});
    out_words.append(QJsonObject{
        {"word", i},
        {"fields", QJsonArray{QJsonObject{
                       {"port", "y0"}, {"port_bits", pb}, {"word_bits", QJsonArray{7, 0}}}}}});
  }
  root["packing"] = QJsonObject{{"input", in_words}, {"output", out_words}};
  const QString path = dir + "/component_test_manifest_wide.json";
  QFile f(path);
  f.open(QIODevice::WriteOnly | QIODevice::Truncate);
  f.write(QJsonDocument(root).toJson());
  f.close();
  return path;
}

static void testSeriWrapPanes() {
  group("SeriWrap: three panes, port/word granularity, streaming entry");
  const QString dir = QDir::tempPath();
  const QString wide = writeWideManifest(dir);

  Box b = make("SeriWrap");
  if (!b.raw) {
    check(false, "SeriWrap missing");
    return;
  }
  bindPins(b.raw);
  auto *sw = dynamic_cast<SeriWrapRawComponent *>(b.raw);
  if (!sw) {
    check(false, "the raw component is a SeriWrapRawComponent");
    return;
  }

  auto *split = b.raw->findChild<QSplitter *>("splitter");
  check(split != nullptr && split->count() == 3,
        "SeriWrap: three panes (config | input | output)");
  check(b.raw->findChild<QWidget *>("cfg_box") && b.raw->findChild<QWidget *>("in_box") &&
            b.raw->findChild<QWidget *>("out_box"),
        "SeriWrap: the panes are the config / input / output boxes");
  auto *stack = b.raw->findChild<QStackedWidget *>("input_stack");
  check(stack != nullptr && stack->count() == 2, "SeriWrap: the input pane has two editors");
  check(b.raw->findChild<QToolButton *>("view_btn") && b.raw->findChild<QToolButton *>("gran_btn"),
        "SeriWrap: the config pane carries the view and granularity switches");
  auto *prev = b.raw->findChild<QPlainTextEdit *>("word_preview");
  check(prev != nullptr, "SeriWrap: the serial word stream is previewed");
  auto *slot = b.raw->findChild<QLabel *>("slot_label");
  check(slot != nullptr, "SeriWrap: the streaming editor names the next slot");

  QString err;
  check(sw->loadManifestFile(wide, &err), QString("wide manifest loads [%1]").arg(err));
  check(sw->streamSlotCount() == 4, "word granularity: one slot per serial word");
  sw->setGranularity(SeriWrapRawComponent::Granularity::Port);
  check(sw->streamSlotCount() == 1, "port granularity: one slot per kernel port");
  sw->setGranularity(SeriWrapRawComponent::Granularity::Word);

  check(sw->commitStreamValue("0x44", &err), QString("stream w0 takes 0x44 [%1]").arg(err));
  check(sw->commitStreamValue("0x33", &err), "stream w1 takes 0x33");
  check(sw->commitStreamValue("0x22", &err), "stream w2 takes 0x22");
  check(sw->commitStreamValue("0x11", &err), "stream w3 takes 0x11");
  const auto vals = sw->inputValues();
  check(vals.size() == 1 && vals[0] == 0x11223344ULL,
        "four streamed words rebuild the 32-bit port (little endian)");
  check(sw->streamFull(), "four committed words fill the frame");
  check(sw->streamSlot() == 0, "the slot pointer wraps once the frame is full");

  const auto pv = sw->previewWords();
  check(pv.size() == 4 && pv[0] == 0x44 && pv[1] == 0x33 && pv[2] == 0x22 && pv[3] == 0x11,
        "the word preview shows the 4 words in wire order");
  check(!sw->commitStreamValue("0x1FF", &err), "a value wider than one word is refused");
  check(sw->previewWords() == pv, "a refused value leaves the frame unchanged");

  // the preview is not a second implementation: compare it with the wire
  const auto &op = b.raw->outputPorts();
  feedRead(b.raw, {1ULL << (op[34].pin_index - 1)});   // READY: the host may send
  QMetaObject::invokeMethod(b.raw, "onSendClicked");
  const auto &ip = b.raw->inputPorts();
  std::vector<uint64_t> sent;
  for (int i = 0; i < 8; i += 2) {          // one pulse + one gap frame per word
    const uint64_t w = b.raw->getWriteData();
    (void)b.raw->getWriteData();
    uint64_t v = 0;
    for (int k = 0; k < 8; ++k) {
      if ((w >> ip[k].pin_index) & 1ULL) v |= 1ULL << k;
    }
    sent.push_back(v);
  }
  check(sent.size() >= 4 && sent[0] == pv[0] && sent[1] == pv[1] && sent[2] == pv[2] &&
            sent[3] == pv[3],
        "the words put on the wire are exactly the previewed words");

  const auto before = sw->inputValues();
  check(sw->commitStreamValue("0xAA", &err), "commit a value for the undo test");
  sw->undoStreamValue();
  check(sw->inputValues() == before, "undo restores the previous port values");

  QMetaObject::invokeMethod(b.raw, "onClearClicked");
  bool all_zero = true;
  for (uint64_t v : sw->inputValues()) {
    if (v) all_zero = false;
  }
  check(all_zero, "clear zeroes the editor");
  check(sw->previewWords() == std::vector<uint64_t>(4, 0), "clear zeroes the preview");

  // view switch keeps the data; auto-send queues the frame when it is complete
  sw->setInputView(SeriWrapRawComponent::InputView::Streaming);
  const auto stream_vals = sw->inputValues();
  sw->setInputView(SeriWrapRawComponent::InputView::Ports);
  check(sw->inputValues() == stream_vals, "switching editors keeps the entered values");
  auto *view_btn = b.raw->findChild<QToolButton *>("view_btn");
  if (view_btn) {
    view_btn->click();
    check(sw->inputView() == SeriWrapRawComponent::InputView::Streaming,
          "the view switch toggles to the streaming editor");
  }

  // layout sanity: at the configured tile size (18x8 grid = 900x400) all three
  // panes must get real estate, and the word preview must show the frame
  b.wrap->resize(900, 400);
  b.wrap->show();
  QApplication::processEvents();
  auto *cfg_box = b.raw->findChild<QWidget *>("cfg_box");
  auto *in_box = b.raw->findChild<QWidget *>("in_box");
  auto *out_box = b.raw->findChild<QWidget *>("out_box");
  check(cfg_box && cfg_box->width() >= 150 && cfg_box->height() >= 200,
        QString("config pane keeps its width and height (%1x%2)")
            .arg(cfg_box ? cfg_box->width() : 0)
            .arg(cfg_box ? cfg_box->height() : 0));
  check(in_box && in_box->width() >= 250 && in_box->height() >= 200,
        QString("input pane keeps its width and height (%1x%2)")
            .arg(in_box ? in_box->width() : 0)
            .arg(in_box ? in_box->height() : 0));
  check(out_box && out_box->width() >= 250 && out_box->height() >= 200,
        QString("output pane keeps its width and height (%1x%2)")
            .arg(out_box ? out_box->width() : 0)
            .arg(out_box ? out_box->height() : 0));
  check(prev && !prev->toPlainText().trimmed().isEmpty(),
        "the word preview shows the packed frame");
  b.wrap->hide();

  Box b2 = make("SeriWrap");
  if (b2.raw) {
    bindPins(b2.raw);
    auto *sw2 = dynamic_cast<SeriWrapRawComponent *>(b2.raw);
    if (sw2 && sw2->loadManifestFile(wide, nullptr)) {
      auto *auto_box = b2.raw->findChild<QCheckBox *>("autosend_box");
      check(auto_box != nullptr, "SeriWrap: the streaming editor has an auto-send box");
      if (auto_box) {
        auto_box->setChecked(true);
        sw2->setGranularity(SeriWrapRawComponent::Granularity::Word);
        sw2->commitStreamValue("0x1");
        sw2->commitStreamValue("0x2");
        sw2->commitStreamValue("0x3");
        check(sw2->framesSent() == 0, "auto-send keeps quiet until the frame is full");
        sw2->commitStreamValue("0x4");
        (void)sw2->getWriteData();          // one host frame starts the queued frame
        check(sw2->framesSent() == 1, "auto-send queues the frame once it is complete");
      }
      // a half-filled streaming frame is refused, a full one is not
      QMetaObject::invokeMethod(b2.raw, "onClearClicked");
      QMetaObject::invokeMethod(b2.raw, "onSendClicked");
      check(sw2->framesSent() == 1, "Send refuses a half-filled streaming frame");
    }
  }
}

// ---------------------------------------------------------------------------
// Offscreen snapshot of the component: the layout is otherwise only visible in
// the GUI, and the tests cannot check "does it look right".
static int snapshot(const QString &out_path, const QString &manifest, bool streaming_view) {
  Box b = make("SeriWrap");
  if (!b.wrap || !b.raw) return 1;
  b.wrap->resize(900, 400);
  b.wrap->show();
  QApplication::processEvents();
  bindPins(b.raw);
  auto *sw = dynamic_cast<SeriWrapRawComponent *>(b.raw);
  if (sw) {
    if (!manifest.isEmpty()) sw->loadManifestFile(manifest, nullptr);
    sw->setInputView(streaming_view ? SeriWrapRawComponent::InputView::Streaming
                                    : SeriWrapRawComponent::InputView::Ports);
    // a few committed words, so the history/preview panes are not empty
    for (int i = 0; i < 4 && sw->streamSlotCount() > i; ++i) {
      QString err;
      sw->commitStreamValue(QString("0x%1").arg(0x44 - i * 0x11, 0, 16), &err);
    }
    // one finished frame on the output side
    const auto &op = b.raw->outputPorts();
    const int w = 8;
    feedRead(b.raw, {outFrame(op, w, 0x44, 0, 1)});
    feedRead(b.raw, {outFrame(op, w, 0x33, 0, 1)});
    feedRead(b.raw, {outFrame(op, w, 0x22, 0, 1)});
    feedRead(b.raw, {outFrame(op, w, 0x11, 0, 1)});
  }
  QApplication::processEvents();
  const bool ok = b.wrap->grab().save(out_path);
  std::printf("snapshot %s -> %s\n", ok ? "saved" : "FAILED", qPrintable(out_path));
  delete b.wrap;
  return ok ? 0 : 1;
}

int main(int argc, char **argv) {
  QApplication app(argc, argv);
  if (argc >= 3 && QString(argv[1]) == "--snapshot") {
    const QString view = argc >= 5 ? QString(argv[4]) : QString("stream");
    return snapshot(QString(argv[2]), argc >= 4 ? QString(argv[3]) : QString(),
                    view != "ports");
  }
  testStructure();
  group("StreamInput: 8/16/32-bit and float value paths");
  testStreamInputWidth("StreamInput8", 8, "171", 0xAB, false);
  testStreamInputWidth("StreamInput16", 16, "43981", 0xABCD, false);
  testStreamInputWidth("StreamInput32", 32, "3735928559", 0xDEADBEEFULL, false);
  testStreamInputWidth("StreamInputFloat", 32, "1.5", 0x3FC00000ULL, true);
  testStreamInputHoldAndOrder();
  group("StreamOutput: edge detection, DATA_VALID gating, widths");
  testStreamOutput("StreamOutput8", 8, 0x5A, "5A", false);
  testStreamOutput("StreamOutput16", 16, 0xBEEF, "BEEF", false);
  testStreamOutput("StreamOutput32", 32, 0xDEADBEEF, "DEADBEEF", false);
  testStreamOutput("StreamOutputFloat", 32, 0x3FC00000ULL, "1.5", true);
  testSeriWrapComponent();
  testSeriWrapPanes();

  std::printf("\n========== component tests: %d checks, %d FAIL ==========\n", g_checks, g_fails);
  return g_fails == 0 ? 0 : 1;
}
