#pragma once
#ifndef SERIWRAP_PROTOCOL_H
#define SERIWRAP_PROTOCOL_H

// ============================================================================
// SeriWrapProtocol -- Qt-free core that speaks the SeriWrap serial link.
//
// Kept free of Qt so it can be compiled and unit-tested on its own (see
// rabbit_App/tests/seriwrap_protocol_test.cpp): "does the component drive the
// pins correctly" is verifiable without a board, a GUI or a device.
//
// The description of the link (word width, words per frame, packing, sync or
// async) comes from <top>__stream_manifest.json, which SeriWrap emits next to
// the generated wrapper.  Nothing here is hard-coded to one mode:
//   * async  -- the word boundary is the s_clk_in rising edge, so the host
//               drives a 3-phase sequence (SETUP / RISING / FALLING), each
//               phase held for 'hold' host frames.
//   * sync   -- the wrapper writes one word per SYSTEM clock while its strobe
//               is high, so the host raises the strobe for exactly one frame.
// ============================================================================

#include <cstdint>
#include <string>
#include <vector>

namespace seriwrap {

/// One field of a serial word: which kernel port it carries, and where it sits
/// both inside that port and inside the serial word.
struct Field {
  int port_index = 0;   ///< index into the kernel port list
  int port_hi = 0;      ///< bit range inside the kernel port
  int port_lo = 0;
  int word_hi = 0;      ///< bit range inside the serial word
  int word_lo = 0;
};

/// Everything the protocol needs to know about one wrapper.
struct LinkConfig {
  int word_width = 32;
  int input_words = 0;    ///< serial words per frame, host -> wrapper
  int output_words = 0;   ///< serial words per frame, wrapper -> host
  bool sync_mode = false;
  bool has_ready = true;
  std::vector<std::vector<Field>> in_packing;   ///< [word][field]
  std::vector<std::vector<Field>> out_packing;
  int n_in_ports = 0;
  int n_out_ports = 0;
  std::vector<std::string> in_port_names;
  std::vector<std::string> out_port_names;
};

/// Which frame bit each logic port is wired to (from the Rabbit project).
struct PinMap {
  std::vector<int> data_in;    ///< size >= word_width, bit k -> frame bit (or -1)
  int clk_in = -1;
  int strobe_in = -1;
  std::vector<int> data_out;   ///< size >= word_width
  int clk_out = -1;
  int data_valid = -1;
  int ready = -1;              ///< -1 when the wrapper has no s_ready
};

class SeriWrapProtocol {
public:
  enum class State { Idle, Sending, Waiting, Done };

  SeriWrapProtocol() = default;
  SeriWrapProtocol(LinkConfig cfg, PinMap pins);

  void reset();

  /// Host frames per protocol phase (async mode).  Matches the configurable
  /// "hold" of the existing Stream components so the slow FPGA has time to see
  /// each level.
  void set_hold_frames(int n) { hold_frames_ = n < 1 ? 1 : n; }
  int holdFrames() const { return hold_frames_; }

  /// Queue the kernel input values for the next frame.  'values' is indexed by
  /// input port index and is masked to each port's width when packing.
  void startFrame(const std::vector<uint64_t> &values);
  void setInputPort(int index, uint64_t value);
  bool busy() const { return state_ != State::Idle; }
  bool frameComplete() const { return state_ == State::Done; }

  /// One host frame: returns the 64-bit word to send.  Call exactly once per
  /// host access, then hand the received word to processRead().
  uint64_t nextWriteWord();

  /// Feed the word that came back (the FPGA's output pins for this frame).
  void processRead(uint64_t word);

  // ---- results -----------------------------------------------------------
  int outputWordsReceived() const { return out_received_; }
  /// Reconstructed kernel output port value (masked to the port width).
  uint64_t outputPort(int index) const;
  bool outputsReady() const { return out_received_ >= cfg_.output_words; }
  State state() const { return state_; }
  /// Raw serial words taken off the wire for the frame in progress (as many as
  /// outputWordsReceived()).  Lets a host show what actually arrived instead of
  /// only the reconstructed port values.
  const std::vector<uint64_t> &receivedWords() const { return received_words_; }

  /// Pack @p values (one per kernel input port) into the input frame exactly as
  /// nextWriteWord() would send it, WITHOUT touching the state machine.  The GUI
  /// uses it for the word-stream preview; the component tests use it to prove
  /// the preview and the wire agree.
  std::vector<uint64_t> previewInputWords(const std::vector<uint64_t> &values) const;

  /// True when the wrapper currently accepts a new word on s_ready.
  bool readySeen() const { return ready_seen_; }
  /// Diagnostics for the GUI.
  const std::string &statusText() const { return status_; }

private:
  uint64_t packInputWord(int word_index) const;
  uint64_t packInputWordFrom(const std::vector<uint64_t> &values, int word_index) const;
  void scatterOutputWord(int word_index, uint64_t word);
  static bool bitAt(uint64_t v, int idx) { return idx >= 0 && ((v >> idx) & 1ULL); }

  LinkConfig cfg_{};
  PinMap pins_{};
  std::vector<uint64_t> in_values_;
  std::vector<uint64_t> out_values_;
  std::vector<uint64_t> out_words_;
  std::vector<uint64_t> received_words_;   ///< raw words taken off the wire
  int hold_frames_ = 20;
  int word_index_ = 0;
  int phase_ = 0;          ///< async: 0=setup 1=rising 2=falling
  int hold_cnt_ = 0;
  int out_received_ = 0;
  bool ready_seen_ = false;
  bool prev_clk_out_ = false;
  State state_ = State::Idle;
  std::string status_;
};

}  // namespace seriwrap

#endif  // SERIWRAP_PROTOCOL_H
