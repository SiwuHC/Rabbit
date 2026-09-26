#pragma once
#ifndef SERIWRAP_COMPONENT_H
#define SERIWRAP_COMPONENT_H

// ============================================================================
// SeriWrapComponent -- one Rabbit component that speaks the whole SeriWrap
// serial link (input frame + output frame).
//
// The tile is split into three panes (see doc/SeriWrapComponentUI.md):
//
//   [ 配置 ]  mode / sent / done / READY+VALID / Hold Frames / view+granularity
//             switches / Manifest... / Send frame
//   [ 输入 ]  per-port rows  OR  StreamInput-style streaming entry (Enter sends
//             one slot), plus the *serial* word-stream preview that both views
//             share -- the link is serial, so the frame is shown as words too
//   [ 输出 ]  one row per kernel output port, the raw words of the last frame,
//             and the frame log
//
// The kernel is addressed by its *ports* (manifest "ports"), the link by its
// *words* (manifest "packing"): the two are the same thing only when a port is
// exactly one word wide, which the header of the input pane reports.
// ============================================================================

#include <QCheckBox>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QQueue>
#include <QSpinBox>
#include <QStackedWidget>
#include <QTableWidget>
#include <QToolButton>

#include <memory>
#include <vector>

#include "Components/AbstractComponent.h"
#include "Components/ComponentMacro.h"
#include "Components/SeriWrapProtocol.h"

namespace rabbit_App::component {

COMPONENT_CLASS_DECLARATION(SeriWrap)

/// Maximum serial word width this component can expose.  Link widths of 8, 16
/// and 32 all fit inside it; unused DATA/DOUT bits are simply left unbound.
inline constexpr int kSeriWrapMaxWidth = 32;

class SeriWrapRawComponent : public AbstractRawComponent {
  Q_OBJECT

public:
  /// What one Enter in the streaming editor fills: one serial word (8 bits for
  /// the FDP3P7 link) or one whole kernel port (32 bits, spanning 4 words).
  enum class Granularity { Word, Port };
  /// Which editor the input pane shows.
  enum class InputView { Ports, Streaming };

  SeriWrapRawComponent(QWidget *parent = nullptr);
  ~SeriWrapRawComponent() override;

  void reset() override;
  void processReadData(QQueue<uint64_t> &read_queue) override;
  uint64_t getWriteData() const override;

  QList<NumberSettingInfo> numberSettings() const override;
  void setNumberSetting(const QString &key, int value) override;

  /// Parse and apply <top>__stream_manifest.json.  The "Manifest..." button is
  /// only the file dialog: it calls this and shows the returned error text, so
  /// the whole manifest -> link config -> pin map path can be driven headlessly
  /// by the component tests.  Returns false and fills @p error on failure.
  bool loadManifestFile(const QString &path, QString *error = nullptr);

  // ---- automation / test API ----------------------------------------------
  /// Kernel input port values currently in the editor (one per port).
  std::vector<uint64_t> inputValues() const;
  /// The frame as the wire will see it (no state machine involved).
  std::vector<uint64_t> previewWords() const;
  /// Streaming slots: input_words (word granularity) or n_in_ports (port).
  int streamSlotCount() const;
  int streamSlot() const;
  QString streamSlotName() const;
  bool streamFull() const;
  int filledSlots() const;
  Granularity granularity() const { return gran_; }
  void setGranularity(Granularity g);
  InputView inputView() const { return view_; }
  void setInputView(InputView v);
  /// Type one value into the streaming editor -- exactly what Enter does.
  bool commitStreamValue(const QString &text, QString *error = nullptr);
  void undoStreamValue();
  int framesSent() const { return static_cast<int>(frames_sent_); }
  int framesDone() const { return static_cast<int>(frames_done_); }

protected:
  void paintEvent(QPaintEvent *event) override;
  void initPorts() override;

private slots:
  void onSendClicked();
  void onLoadManifestClicked();
  void onViewClicked();
  void onGranularityClicked();
  void onStreamReturn();
  void onUndoClicked();
  void onClearClicked();
  void onHoldChanged(int value);

private:
  QWidget *buildConfigPane();
  QWidget *buildInputPane();
  QWidget *buildOutputPane();

  void rebuildFromBindings() const;
  void refreshLabels();
  void rebuildPortTables();
  void refreshWordPreview();
  void refreshStreamSlot();
  void refreshModeButtons();
  void collectInputs();
  void applyBulkValues();
  void writeRowValue(int port, uint64_t value);
  void markSlotsFilledForPorts(int count);
  void markSlotsFilledForPortRow(int port);
  uint64_t dataBitsOfFrameWord(uint64_t frameWord) const;
  int portWordLo(int port) const;
  int portWordHi(int port) const;
  /// const because getWriteData(), which reports what it puts on the wire, is
  /// itself const (the log widget is reached through a pointer member).
  void appendLog(const QString &line) const;

  // widgets
  QLabel *status_label_;
  QLabel *ready_dot_;
  QLabel *valid_dot_;
  QLabel *pin_label_;
  QSpinBox *hold_spin_;
  QToolButton *view_btn_;
  QToolButton *gran_btn_;
  QTableWidget *in_table_;
  QTableWidget *out_table_;
  QStackedWidget *input_stack_;
  QLineEdit *stream_edit_;
  QLabel *slot_label_;
  QListWidget *hist_list_;
  QCheckBox *autosend_box_;
  QPlainTextEdit *word_preview_;
  QLineEdit *value_edit_;          // bulk paste field ("bulk_edit")
  QPushButton *bulk_btn_;
  QPushButton *send_btn_;
  QPushButton *manifest_btn_;
  QPushButton *undo_btn_;
  QPushButton *clear_btn_;
  QLabel *out_label_;
  QLabel *raw_label_;
  QListWidget *log_list_;
  std::vector<QLineEdit *> in_edits_;
  std::vector<QTableWidgetItem *> out_values_;
  std::vector<std::vector<uint64_t>> stream_undo_;
  std::vector<int> stream_undo_slot_;
  std::vector<bool> stream_filled_;
  bool updating_rows_ = false;

  // protocol state (getWriteData() is const, hence the mutable members)
  mutable seriwrap::LinkConfig cfg_;
  mutable seriwrap::PinMap pins_;
  mutable std::unique_ptr<seriwrap::SeriWrapProtocol> proto_;
  mutable bool frame_armed_ = false;
  /// A finished frame is reported once.  outputWordsReceived() stays at its
  /// final value until the next frame starts, so without this the log would
  /// print the same frame on every host access (which looks like an endless
  /// stream of frames in the GUI).
  mutable bool frame_reported_ = false;
  mutable int hold_frames_ = 20;
  mutable uint32_t frames_sent_ = 0;
  mutable uint32_t frames_done_ = 0;
  mutable std::vector<uint64_t> pending_inputs_;

  // editor state
  Granularity gran_ = Granularity::Word;      // Enter sends one serial word
  InputView view_ = InputView::Streaming;
  int stream_slot_ = 0;
  std::vector<int> in_port_widths_;

  // manifest-provided description (filled by the loader)
  int manifest_words_in_ = 0;
  int manifest_words_out_ = 0;
  bool manifest_sync_ = false;
};

}  // namespace rabbit_App::component

#endif  // SERIWRAP_COMPONENT_H
