#pragma once
#ifndef SERIWRAP_COMPONENT_H
#define SERIWRAP_COMPONENT_H

// ============================================================================
// SeriWrapComponent -- one Rabbit component that speaks the whole SeriWrap
// serial link (input frame + output frame), instead of gluing a StreamInput
// and a StreamOutput together by hand.
//
// It declares the wrapper's own logical ports
//     DATA[0..W-1], CLK, STROBE          (host -> FPGA)
//     DOUT[0..W-1], CLK_OUT, DATA_VALID, READY   (FPGA -> host)
// and drives them through seriwrap::SeriWrapProtocol, which knows how many
// words a frame is, how they are packed and whether the word boundary is an
// edge (async) or a clock (sync).  The port bindings in the .rbtprj can be
// generated automatically from the wrapper manifest + constraint file
// (SeriWrap/tools/gen_rabbit_project.py), so the user does not have to pick
// sixty-odd pins by hand.
// ============================================================================

#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QQueue>
#include <QPushButton>
#include <QCheckBox>
#include <QTableWidget>

#include <memory>

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

protected:
  void paintEvent(QPaintEvent *event) override;
  void initPorts() override;

private slots:
  void onSendClicked();
  void onLoadManifestClicked();
  void onAutoRepeatToggled(bool on);

private:
  void rebuildFromBindings() const;
  void refreshLabels();
  /// One editable row per kernel input port, one read-only row per kernel
  /// output port.  Called after a manifest loads (that is where the port names
  /// and counts come from).
  void rebuildPortTables();
  /// Copy the comma separated "bulk" field into the per-port rows (one value
  /// per port) and clear it.
  void applyBulkValues();
  /// Read the per-port rows into pending_inputs_ (one value per input port).
  void collectInputs();
  /// const because getWriteData(), which reports what it puts on the wire, is
  /// itself const (the log widget is reached through a pointer member).
  void appendLog(const QString &line) const;

  // widget bits
  QLineEdit *value_edit_;          // bulk paste field ("v0, v1, ...")
  QTableWidget *in_table_;         // per-port input rows (port | value)
  QTableWidget *out_table_;        // per-port output rows (port | last frame)
  std::vector<QLineEdit *> in_edits_;
  std::vector<QTableWidgetItem *> out_values_;
  QPushButton *bulk_btn_;
  QPushButton *send_btn_;
  QPushButton *manifest_btn_;
  QCheckBox *auto_repeat_;
  QLabel *status_label_;
  QLabel *out_label_;
  QListWidget *log_list_;

  // protocol state (getWriteData() is const, hence the mutable members)
  mutable seriwrap::LinkConfig cfg_;
  mutable seriwrap::PinMap pins_;
  mutable std::unique_ptr<seriwrap::SeriWrapProtocol> proto_;
  mutable bool frame_armed_ = false;
  mutable bool auto_repeat_on_ = false;
  /// A finished frame is reported once.  outputWordsReceived() stays at its
  /// final value until the next frame starts, so without this the log would
  /// print the same frame on every host access (which looks like an endless
  /// stream of frames in the GUI).
  mutable bool frame_reported_ = false;
  mutable int hold_frames_ = 20;
  mutable uint32_t frames_sent_ = 0;
  mutable uint32_t frames_done_ = 0;
  mutable std::vector<uint64_t> pending_inputs_;
  /// Raw words handed to the wire for the frame in flight, so the log can show
  /// the data the kernel actually sees (packed from the manifest).
  mutable std::vector<uint64_t> tx_words_;
  mutable QString inp_names_;
  mutable QString out_names_;

  // manifest-provided description (filled by the loader)
  int manifest_words_in_ = 0;
  int manifest_words_out_ = 0;
  bool manifest_sync_ = false;
};

}  // namespace rabbit_App::component

#endif  // SERIWRAP_COMPONENT_H
