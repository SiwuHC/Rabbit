// ============================================================================
// Headless unit test for SeriWrapProtocol -- no Qt, no board, no GUI.
//
// It drives the protocol core exactly the way the Rabbit backend does (one
// nextWriteWord() and one processRead() per host frame) and checks the
// pin-level behaviour implied by the SeriWrap manifest:
//
//   async    data stable, strobe high, word boundary = s_clk_in RISING edge,
//            each phase held for hold_frames host frames
//   sync     one host frame per word with the strobe high
//   s_ready  no new word may start while the wrapper is busy
//   receive  async latches on the s_clk_out rising edge with s_data_valid;
//            sync latches on the s_data_valid level (s_clk_out is tied low)
//   packing  a word carrying two ports unpacks into both; a port spanning two
//            words reassembles in the right order
//
// Build and run it with rabbit_App/tests/run_protocol_test.sh
// ============================================================================
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "Components/SeriWrapProtocol.h"

using namespace seriwrap;

static int g_checks = 0;
static int g_fail = 0;

static void check(bool ok, const std::string &what) {
  ++g_checks;
  if (!ok) {
    ++g_fail;
    std::printf("  [FAIL] %s\n", what.c_str());
  }
}

static bool bit(uint64_t w, int idx) { return idx >= 0 && ((w >> idx) & 1ULL) != 0; }

// ------------------------------------------------------------------ fixtures
static PinMap makePins() {
  PinMap p;
  p.data_in.resize(32);
  for (int i = 0; i < 32; ++i) p.data_in[i] = i;        // frame bits 0..31
  p.clk_in = 32;
  p.strobe_in = 33;
  p.data_out.resize(32);
  for (int i = 0; i < 32; ++i) p.data_out[i] = 1 + i;   // frame bits 1..32
  p.clk_out = 33;
  p.data_valid = 34;
  p.ready = 35;
  return p;
}

// 3 input words (word 2 packs two ports), 2 output words (y1 spans two words).
static LinkConfig makeConfig(bool sync) {
  LinkConfig c;
  c.word_width = 32;
  c.input_words = 3;
  c.output_words = 2;
  c.sync_mode = sync;
  c.has_ready = true;
  c.n_in_ports = 2;
  c.n_out_ports = 2;
  c.in_port_names = {"a0", "a1"};
  c.out_port_names = {"y0", "y1"};
  c.in_packing = {
      {{0, 31, 0, 31, 0}},                      // word0 <- a0[31:0]
      {{1, 31, 0, 31, 0}},                      // word1 <- a1[31:0]
      {{0, 15, 0, 15, 0}, {1, 15, 0, 31, 16}},  // word2 <- a0[15:0] | a1[15:0]
  };
  c.out_packing = {
      {{1, 31, 16, 15, 0}},                     // y1[31:16] <- word0[15:0]
      {{0, 31, 0, 31, 0}, {1, 15, 0, 31, 16}},  // y0[31:0] <- word1[31:0]; y1[15:0] <- word1[31:16]
  };
  return c;
}

static uint64_t packData(const PinMap &p, uint32_t data) {
  uint64_t w = 0;
  for (int i = 0; i < 32; ++i) {
    if ((data >> i) & 1U) w |= (1ULL << p.data_out[i]);
  }
  return w;
}

static uint64_t rxFrame(const PinMap &p, uint32_t data, bool clk, bool valid, bool ready) {
  uint64_t w = packData(p, data);
  if (clk) w |= (1ULL << p.clk_out);
  if (valid) w |= (1ULL << p.data_valid);
  if (ready) w |= (1ULL << p.ready);
  return w;
}

/// One host access: write this frame's word, then hand back what came in.
struct Host {
  SeriWrapProtocol &proto;
  const PinMap &pins;
  uint64_t wrote = 0;
  uint64_t rx(uint32_t data, bool clk, bool valid, bool ready) {
    wrote = proto.nextWriteWord();
    return rxFrame(pins, data, clk, valid, ready);
  }
};

// --------------------------------------------------------------------- tests
static void test_async_send() {
  std::printf("async send sequence (hold=2, 3 words)\n");
  PinMap pins = makePins();
  SeriWrapProtocol proto(makeConfig(false), pins);
  proto.set_hold_frames(2);
  proto.startFrame({0x11223344ULL, 0xAABBCCDDULL});
  const uint32_t a0 = 0x11223344U, a1 = 0xAABBCCDDU;

  // frames per word, per phase: (data, strobe, clk) with each phase twice
  // The very first host access only learns s_ready (the host must write a word
  // before it can read one), so prime one frame first.
  proto.nextWriteWord();
  proto.processRead(rxFrame(pins, 0, false, false, true));

  // word0 <- a0[31:0]; word1 <- a1[31:0]; word2 <- a0[15:0] | (a1[15:0] << 16)
  const uint32_t words[3] = {a0, a1, ((a1 & 0xFFFFU) << 16) | (a0 & 0xFFFFU)};
  int frame = 0;
  bool ok_pattern = true, ok_data = true;
  int rising_edges = 0;
  bool prev_clk = false;
  for (int wi = 0; wi < 3; ++wi) {
    const int expect_clk[6] = {0, 0, 1, 1, 0, 0};
    const int expect_stb[6] = {1, 1, 1, 1, 0, 0};
    for (int k = 0; k < 6; ++k) {
      // host access
      uint64_t w = proto.nextWriteWord();
      ++frame;
      const bool stb = bit(w, pins.strobe_in);
      const bool clk = bit(w, pins.clk_in);
      if (stb != (expect_stb[k] != 0) || clk != (expect_clk[k] != 0)) ok_pattern = false;
      if (clk && !prev_clk) ++rising_edges;
      prev_clk = clk;
      // data bits must equal the word during the setup/rising phases
      if (k < 4) {
        uint32_t got = 0;
        for (int b = 0; b < 32; ++b) {
          if (bit(w, pins.data_in[b])) got |= (1U << b);
        }
        if (got != words[wi]) {
          ok_data = false;
          std::printf("    word %d access %d: data=0x%08X want 0x%08X\n", wi, k, got, words[wi]);
        }
      } else {
        for (int b = 0; b < 32; ++b) {
          if (bit(w, pins.data_in[b])) ok_data = false;   // quiet phase drives zeros
        }
      }
      proto.processRead(rxFrame(pins, 0, false, false, true));
    }
  }
  check(ok_pattern, "strobe/clk phase pattern is setup,rising,falling x hold");
  check(ok_data, "data bits carry the packed word and are zero while idle");
  check(rising_edges == 3, "exactly one s_clk_in rising edge per input word");
  check(frame == 18, "3 words x 3 phases x hold(2) = 18 host frames");
  check(proto.state() == SeriWrapProtocol::State::Waiting,
        "input burst finished -> waiting for the output stream");
}

static void test_ready_gating() {
  std::printf("s_ready back-pressure\n");
  PinMap pins = makePins();
  SeriWrapProtocol proto(makeConfig(false), pins);
  proto.set_hold_frames(1);
  proto.startFrame({0x1ULL, 0x2ULL});

  // first access: nothing is known about s_ready yet, so stay quiet
  check(proto.nextWriteWord() == 0, "the first access learns s_ready before driving");
  // wrapper reports busy -> still gated
  proto.processRead(rxFrame(pins, 0, false, false, false));
  check(proto.nextWriteWord() == 0, "no word is driven while s_ready is low");
  // wrapper reports ready -> the burst starts
  proto.processRead(rxFrame(pins, 0, false, false, true));
  const uint64_t w2 = proto.nextWriteWord();
  check(bit(w2, pins.strobe_in) && !bit(w2, pins.clk_in),
        "word 0 starts as soon as s_ready returns high");
  proto.processRead(rxFrame(pins, 0, false, false, true));
}

static void test_async_receive() {
  std::printf("async receive (edge on s_clk_out + s_data_valid)\n");
  PinMap pins = makePins();
  SeriWrapProtocol proto(makeConfig(false), pins);
  proto.set_hold_frames(1);
  proto.startFrame({0x1111ULL, 0x2222ULL});
  proto.processRead(rxFrame(pins, 0, false, false, true));

  // word 0 = 0xDEAD0001, word 1 = 0xBEEF1234  (y0 <- word1, y1 <- word0[15:0]|word1[31:16])
  // no latch without a rising edge
  proto.processRead(rxFrame(pins, 0xDEAD0001U, false, true, true));
  check(proto.outputWordsReceived() == 0, "s_data_valid alone does not latch (async)");
  // valid + rising edge latches
  proto.processRead(rxFrame(pins, 0xDEAD0001U, true, true, true));
  check(proto.outputWordsReceived() == 1, "valid + s_clk_out rising edge latches a word");
  // no double latch while the clock stays high
  proto.processRead(rxFrame(pins, 0xDEAD0001U, true, true, true));
  check(proto.outputWordsReceived() == 1, "a held clock does not latch twice");
  // falling then rising latches the next word
  proto.processRead(rxFrame(pins, 0xDEAD0001U, false, true, true));
  proto.processRead(rxFrame(pins, 0xBEEF1234U, true, true, true));
  check(proto.outputWordsReceived() == 2, "second word latched");
  check(proto.outputsReady(), "frame complete after output_words latches");
  check(proto.state() == SeriWrapProtocol::State::Done, "state is Done");

  // packing: y0 = word1, y1 = word0[15:0] << 16 | word1[31:16]
  const uint64_t y0 = proto.outputPort(0);
  const uint64_t y1 = proto.outputPort(1);
  check(y0 == 0xBEEF1234ULL, "y0 reassembles from serial word 1 (was 0x" +
                                 std::to_string(y0) + ")");
  const uint64_t want_y1 = ((0xDEAD0001ULL & 0xFFFFULL) << 16) | ((0xBEEF1234ULL >> 16) & 0xFFFFULL);
  check(y1 == want_y1, "y1 combines low bits of word0 with high bits of word1 (was 0x" +
                           std::to_string(y1) + ")");
}

static void test_sync_mode() {
  std::printf("sync mode (one frame per word, level-triggered receive)\n");
  PinMap pins = makePins();
  SeriWrapProtocol proto(makeConfig(true), pins);
  proto.set_hold_frames(20);           // ignored in sync mode
  proto.startFrame({0xCAFEBABEULL, 0x0BADF00DULL});
  proto.processRead(rxFrame(pins, 0, false, false, true));

  int frames = 0;
  bool strobe_ok = true, clk_quiet = true;
  for (int i = 0; i < 3; ++i) {
    const uint64_t w = proto.nextWriteWord();
    ++frames;
    if (!bit(w, pins.strobe_in)) strobe_ok = false;
    if (bit(w, pins.clk_in)) clk_quiet = false;
    proto.processRead(rxFrame(pins, 0, false, false, true));
  }
  check(frames == 3, "sync mode sends exactly one host frame per input word");
  check(strobe_ok, "the strobe is raised for that frame");
  check(clk_quiet, "s_clk_in is not toggled in sync mode");
  check(proto.state() == SeriWrapProtocol::State::Waiting, "input burst finished");

  // sync receive: s_clk_out is tied low, so the valid level marks the word
  proto.processRead(rxFrame(pins, 0x000000AAU, false, true, true));
  check(proto.outputWordsReceived() == 1, "sync latches on the s_data_valid level");
  // out_packing[0] puts word0[15:0] into y1[31:16]
  check(proto.outputPort(1) == 0x00AA0000ULL, "sync receive scatters into the right port bits");
}

static void test_no_ready_variant() {
  std::printf("wrapper without s_ready\n");
  PinMap pins = makePins();
  LinkConfig c = makeConfig(false);
  c.has_ready = false;
  SeriWrapProtocol proto(c, pins);
  proto.set_hold_frames(1);
  proto.startFrame({0x1ULL, 0x2ULL});
  const uint64_t w = proto.nextWriteWord();
  check(bit(w, pins.strobe_in), "a wrapper without s_ready never blocks the host");
}

int main() {
  test_async_send();
  test_ready_gating();
  test_async_receive();
  test_sync_mode();
  test_no_ready_variant();
  std::printf("\n========== %d checks, %d failures ==========\n", g_checks, g_fail);
  std::printf(g_fail == 0 ? "SERIWRAP PROTOCOL PASS\n" : "SERIWRAP PROTOCOL FAIL\n");
  return g_fail == 0 ? 0 : 1;
}
