#include "Components/SeriWrapProtocol.h"

#include <algorithm>

namespace seriwrap {

namespace {

uint64_t maskOf(int width) {
  if (width <= 0) return 0ULL;
  if (width >= 64) return ~0ULL;
  return (1ULL << width) - 1ULL;
}

uint64_t withBit(uint64_t w, int idx, bool value) {
  if (idx < 0) return w;                    // port not bound: leave the bit alone
  if (value) return w | (1ULL << idx);
  return w & ~(1ULL << idx);
}

}  // namespace

SeriWrapProtocol::SeriWrapProtocol(LinkConfig cfg, PinMap pins)
    : cfg_(std::move(cfg)), pins_(std::move(pins)) {
  reset();
}

void SeriWrapProtocol::reset() {
  in_values_.assign(std::max(1, cfg_.n_in_ports), 0ULL);
  out_values_.assign(std::max(1, cfg_.n_out_ports), 0ULL);
  out_words_.assign(std::max(1, cfg_.output_words), 0ULL);
  received_words_.clear();
  word_index_ = 0;
  phase_ = 0;
  hold_cnt_ = 0;
  out_received_ = 0;
  ready_seen_ = !cfg_.has_ready;             // no s_ready -> never blocks
  prev_clk_out_ = false;
  state_ = State::Idle;
  status_ = "idle";
}

void SeriWrapProtocol::setInputPort(int index, uint64_t value) {
  if (index >= 0 && index < static_cast<int>(in_values_.size())) {
    in_values_[index] = value;
  }
}

void SeriWrapProtocol::startFrame(const std::vector<uint64_t> &values) {
  in_values_.assign(std::max(1, cfg_.n_in_ports), 0ULL);
  for (size_t i = 0; i < values.size() && i < in_values_.size(); ++i) {
    in_values_[i] = values[i];
  }
  out_values_.assign(std::max(1, cfg_.n_out_ports), 0ULL);
  out_words_.assign(std::max(1, cfg_.output_words), 0ULL);
  received_words_.clear();
  word_index_ = 0;
  phase_ = 0;
  hold_cnt_ = 0;
  out_received_ = 0;
  if (cfg_.input_words <= 0) {
    // output-only wrapper: nothing to send, wait for the stream
    state_ = State::Waiting;
    status_ = "waiting for output stream";
  } else {
    state_ = State::Sending;
    status_ = "sending input frame";
  }
}

uint64_t SeriWrapProtocol::nextWriteWord() {
  switch (state_) {
    case State::Idle:
    case State::Done:
      return 0;                              // quiet line
    case State::Waiting:
      return 0;
    case State::Sending:
      break;
  }

  // Do not start driving a new WORD while the wrapper says it is busy.  The
  // wrapper drops s_ready when its input frame is complete and it moves on to
  // launch/compute/dump, so this is what makes the host wait for the frame to
  // be consumed instead of pushing words into a busy wrapper.
  const bool at_word_start = cfg_.sync_mode ? true : (hold_cnt_ == 0 && phase_ == 0);
  if (cfg_.has_ready && !ready_seen_ && at_word_start) {
    status_ = "waiting for s_ready";
    return 0;
  }

  if (cfg_.sync_mode) {
    // Sync wrapper: it writes one word per SYSTEM clock while its strobe is
    // high, so the strobe must be raised for exactly one host frame.  (Whether
    // one host frame really is one system clock is what probe_clk measures.)
    // Two host frames per word: data + STROBE high, then the same data with
    // STROBE low.  The generated sync SIPO writes on the STROBE *rising edge*,
    // so the pulse, not the number of clocks the host holds a frame for,
    // defines a word.  (A Rabbit GUI frame lasts several FPGA clocks; with a
    // level-sensitive SIPO every word would be stored that many times.)
    uint64_t word = packInputWord(word_index_);
    word = withBit(word, pins_.strobe_in, phase_ == 0);
    word = withBit(word, pins_.clk_in, false);
    if (++phase_ > 1) {
      phase_ = 0;
      ++word_index_;
      if (word_index_ >= cfg_.input_words) {
        state_ = State::Waiting;
        status_ = "input frame sent, collecting output";
      }
    }
    return word;
  }

  // Async wrapper: word boundary is the s_clk_in RISING edge, so drive
  //   0: data + strobe high, clock low   (data settles)
  //   1: clock high                      (FPGA samples on this edge)
  //   2: everything low                  (idle, and the word is done)
  // each phase held for hold_frames_ host frames.
  uint64_t word = 0;
  if (phase_ == 0 || phase_ == 1) {
    word = packInputWord(word_index_);
    word = withBit(word, pins_.strobe_in, true);
    word = withBit(word, pins_.clk_in, phase_ == 1);
  } else {
    word = withBit(word, pins_.strobe_in, false);
    word = withBit(word, pins_.clk_in, false);
  }

  if (++hold_cnt_ >= hold_frames_) {
    hold_cnt_ = 0;
    if (++phase_ > 2) {
      phase_ = 0;
      ++word_index_;
      if (word_index_ >= cfg_.input_words) {
        state_ = State::Waiting;
        status_ = "input frame sent, collecting output";
      }
    }
  }
  return word;
}

void SeriWrapProtocol::processRead(uint64_t word) {
  ready_seen_ = cfg_.has_ready ? bitAt(word, pins_.ready) : true;

  const bool clk_out = bitAt(word, pins_.clk_out);
  const bool valid = bitAt(word, pins_.data_valid);

  bool take = false;
  if (cfg_.sync_mode) {
    // s_clk_out is tied low in sync mode; the data-valid level marks the word.
    take = valid;
  } else {
    take = valid && clk_out && !prev_clk_out_;   // rising edge of s_clk_out
  }
  prev_clk_out_ = clk_out;

  if (take && out_received_ < cfg_.output_words) {
    uint64_t data = 0;
    for (int k = 0; k < cfg_.word_width; ++k) {
      if (bitAt(word, (k < static_cast<int>(pins_.data_out.size())) ? pins_.data_out[k] : -1)) {
        data |= (1ULL << k);
      }
    }
    out_words_[out_received_] = data;
    received_words_.push_back(data);
    scatterOutputWord(out_received_, data);
    ++out_received_;
    if (out_received_ >= cfg_.output_words) {
      state_ = State::Done;
      status_ = "frame complete";
    } else {
      status_ = "received word " + std::to_string(out_received_) + "/" +
                std::to_string(cfg_.output_words);
    }
  }
}

uint64_t SeriWrapProtocol::packInputWord(int word_index) const {
  return packInputWordFrom(in_values_, word_index);
}

std::vector<uint64_t> SeriWrapProtocol::previewInputWords(
    const std::vector<uint64_t> &values) const {
  std::vector<uint64_t> out;
  out.reserve(static_cast<size_t>(std::max(0, cfg_.input_words)));
  for (int i = 0; i < cfg_.input_words; ++i) out.push_back(packInputWordFrom(values, i));
  return out;
}

uint64_t SeriWrapProtocol::packInputWordFrom(const std::vector<uint64_t> &values,
                                             int word_index) const {
  uint64_t word = 0;
  if (word_index < 0 || word_index >= static_cast<int>(cfg_.in_packing.size())) {
    return word;
  }
  for (const Field &f : cfg_.in_packing[word_index]) {
    if (f.port_index < 0 || f.port_index >= static_cast<int>(values.size())) {
      continue;
    }
    const int width = f.port_hi - f.port_lo + 1;
    const uint64_t field = (values[f.port_index] >> f.port_lo) & maskOf(width);
    const int nbits = f.word_hi - f.word_lo + 1;
    for (int i = 0; i < nbits; ++i) {
      if ((field >> i) & 1ULL) {
        const int wire_bit = f.word_lo + i;
        const int frame_bit =
            (wire_bit < static_cast<int>(pins_.data_in.size())) ? pins_.data_in[wire_bit] : -1;
        if (frame_bit >= 0) word |= (1ULL << frame_bit);
      }
    }
  }
  return word;
}

void SeriWrapProtocol::scatterOutputWord(int word_index, uint64_t word) {
  if (word_index < 0 || word_index >= static_cast<int>(cfg_.out_packing.size())) return;
  for (const Field &f : cfg_.out_packing[word_index]) {
    if (f.port_index < 0 || f.port_index >= static_cast<int>(out_values_.size())) continue;
    const int width = f.word_hi - f.word_lo + 1;
    const uint64_t field = (word >> f.word_lo) & maskOf(width);
    const uint64_t placed = field << f.port_lo;
    const uint64_t keep = ~(maskOf(width) << f.port_lo);
    // fields of one word never overlap (the generator packs them), so OR-ing
    // the masked result is enough and keeps ports that span several words.
    out_values_[f.port_index] = (out_values_[f.port_index] & keep) | placed;
  }
}

uint64_t SeriWrapProtocol::outputPort(int index) const {
  if (index < 0 || index >= static_cast<int>(out_values_.size())) return 0;
  return out_values_[index];
}

}  // namespace seriwrap
