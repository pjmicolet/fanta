#include "build.hpp"
#include "disasm.hpp"
#include "editor.hpp"
#include "layout.hpp"
#include "program.hpp"
#include "terminal.hpp"
#include "tui.hpp"
#include <filesystem>
#include <fstream>
#include <string_assembler.hpp>
#include <testframework/testing.hpp>

namespace {

auto tempPath(const std::string &name) -> std::string {
  return (std::filesystem::temp_directory_path() / name).string();
}

/// Small programs written by the tests themselves. The demos in test/*.asm
/// are gitignored, so tests must not depend on them (and under ctest a
/// missing path would silently open as a new, empty buffer).
auto fixture(const std::string &name, const std::string &text) -> std::string {
  auto path = tempPath(name);
  std::ofstream out(path);
  out << text;
  return path;
}

// Sends "Hi" over the serial port, then halts.
auto serialFixture() -> std::string {
  return fixture("fanta_serial_hi.asm", "MOV R1, #200\n"
                                        "LSH R1, R1, #10\n" // 0x0200_0000
                                        "MOV R2, #48\n"
                                        "STORE R2, 0(R1)\n"
                                        "MOV R2, #69\n"
                                        "STORE R2, 0(R1)\n"
                                        "HALT\n");
}

// Keeps changing pixel 0 of VRAM forever.
auto vramFixture() -> std::string {
  return fixture("fanta_vram_loop.asm", "MOV R1, #80\n"
                                        "LSH R1, R1, #10\n" // 0x800000
                                        "LOOP:\n"
                                        "ADD R2, R2, #1\n"
                                        "STORE R2, 0(R1)\n"
                                        "JMP LOOP\n");
}

auto writeBinary(const std::string &path, const std::vector<uint32_t> &words)
    -> void {
  std::ofstream out(path, std::ios::binary);
  out.write(reinterpret_cast<const char *>(words.data()),
            static_cast<std::streamsize>(words.size() * sizeof(uint32_t)));
}

/// assemble(disassemble(w)) == w for a word assembled from `src` at `addr`.
auto roundTrips(const std::string &src, uint32_t addr = 0) -> bool {
  Assembler assem{};
  uint32_t word = assem.assemble(src, static_cast<int>(addr));
  if (word == static_cast<uint32_t>(-1))
    return false;
  std::string text = Fanta::disassemble(word, addr);
  return assem.assemble(text, static_cast<int>(addr)) == word;
}

} // namespace

// --- Disassembler ----------------------------------------------------------

TEST_CASE("Disasm NOP Is Not Shown As HALT") {
  REQUIRE_SAME(std::string{"HALT"}, Fanta::disassemble(0));
  REQUIRE_SAME(std::string{"NOP"},
               Fanta::disassemble(Instructions::Nop::emit()));
  REQUIRE_SAME(std::string{"RET"}, Fanta::disassemble(0x16u << 26));
}

TEST_CASE("Disasm Register And Immediate Forms") {
  uint32_t addReg = (0x1u << 26) | (1u << 21) | (2u << 16) | (3u << 11);
  REQUIRE_SAME(std::string{"ADD R1, R2, R3"}, Fanta::disassemble(addReg));
  Assembler assem{};
  REQUIRE_SAME(std::string{"ADD R1, R2, #FF"},
               Fanta::disassemble(assem.assemble("ADD R1, R2, #FF", 0)));
  REQUIRE_SAME(std::string{"LOAD R1, #10(R2)"},
               Fanta::disassemble(assem.assemble("LOAD R1, 10(R2)", 0)));
  REQUIRE_SAME(std::string{"CIP #0"}, Fanta::disassemble(0x1Fu << 26));
}

TEST_CASE("Disasm Prints R15 Not FP So Output Re-Assembles") {
  Assembler assem{};
  REQUIRE_SAME(std::string{"MOV R15, R1"},
               Fanta::disassemble(assem.assemble("MOV R15, R1", 0)));
}

TEST_CASE("Disasm Round Trips Every Non-Branch Format") {
  for (const auto *src :
       {"ADD R1, R2, R3", "ADD R1, R2, #FF", "SUB R3, R4, #1",
        "LSH R1, R1, #10", "AND R1, R2, R3", "OR R1, R2, #5",
        "XOR R7, R8, R9", "MOV R1, #80", "MOV R1, R2", "CMP R1, #A",
        "CMP R1, R2", "LOAD R1, 10(R2)", "STORE R3, #FFFC(R4)", "PUSH R5",
        "POP R6", "RET", "NOP", "HALT", "CIP #0", "JMP #20"}) {
    if (!roundTrips(src))
      REQUIRE_SAME(std::string{"round trip"}, std::string{src});
  }
}

TEST_CASE("Disasm Round Trips Branch Offsets In Both Directions") {
  for (const auto *src :
       {"BNE -4", "BEQ #10", "BMI -#20", "BPL #8", "JREL -#C", "CALL #40"}) {
    if (!roundTrips(src, 0x100))
      REQUIRE_SAME(std::string{"round trip"}, std::string{src});
  }
}

TEST_CASE("Disasm Shows Branch Target And Uses Labels When Known") {
  Assembler assem{};
  uint32_t bne = assem.assemble("BNE -8", 0x10);
  REQUIRE_SAME(std::string{"BNE -#8 ; -> 0x8"},
               Fanta::disassemble(bne, 0x10));

  auto labels = [](uint32_t a) -> std::optional<std::string> {
    if (a == 0x8)
      return "LOOP";
    return std::nullopt;
  };
  REQUIRE_SAME(std::string{"BNE LOOP ; 0x8"},
               Fanta::disassemble(bne, 0x10, labels));
}

TEST_CASE("Disasm Unknown Opcode") {
  REQUIRE_SAME(std::string{"UNKNOWN (0x3f)"},
               Fanta::disassemble(0x3Fu << 26));
}

// --- Program loading -------------------------------------------------------

TEST_CASE("Assemble Blank Lines And Indented Comments Become NOPs") {
  CPU cpu{};
  Assembler assem{};
  std::vector<std::string> lines{"; header", "", "    ; indented comment",
                                 "START:", "    MOV R1, #1"};
  auto errors = Fanta::Program::assembleInto(cpu, assem, lines);
  REQUIRE_TRUE(errors.empty());
  for (uint32_t a = 0; a < 16; a += 4)
    REQUIRE_SAME(Instructions::Nop::emit(), cpu.load(a));
  REQUIRE_SAME(assem.assemble("MOV R1, #1", 16), cpu.load(16));
}

TEST_CASE("Assemble Bad Line Is Reported And Does Not Stop The Rest") {
  CPU cpu{};
  Assembler assem{};
  std::vector<std::string> lines{"MOV R1, #1", "FROB R1", "MOV R2, #2"};
  auto errors = Fanta::Program::assembleInto(cpu, assem, lines);
  REQUIRE_SAME(1, errors.size());
  REQUIRE_SAME(1, errors[0].line);
  REQUIRE_SAME(std::string{"FROB R1"}, errors[0].text);
  REQUIRE_SAME(Instructions::Nop::emit(), cpu.load(4));
  REQUIRE_SAME(assem.assemble("MOV R2, #2", 8), cpu.load(8));
}

TEST_CASE("Load Binary Stores The Image Word For Word") {
  auto path = tempPath("fanta_tui_test.bin");
  std::vector<uint32_t> image{0x10200080, 0x50000000, 0x0};
  writeBinary(path, image);

  CPU cpu{};
  std::vector<Fanta::Program::AsmError> errors;
  REQUIRE_TRUE(Fanta::Program::loadInto(cpu, path, errors));
  for (size_t i = 0; i < image.size(); ++i)
    REQUIRE_SAME(image[i], cpu.load(static_cast<uint32_t>(i * 4)));
  std::filesystem::remove(path);
}

TEST_CASE("Read Lines Strips CRLF And Trailing Spaces") {
  auto path = tempPath("fanta_tui_test.asm");
  {
    std::ofstream out(path, std::ios::binary);
    out << "MOV R1, #1  \r\nRET\r\n";
  }
  auto lines = Fanta::Program::readLines(path);
  REQUIRE_TRUE(lines.has_value());
  REQUIRE_SAME(2, lines->size());
  REQUIRE_SAME(std::string{"MOV R1, #1"}, (*lines)[0]);
  REQUIRE_SAME(std::string{"RET"}, (*lines)[1]);
  std::filesystem::remove(path);
}

// --- TUI session (headless, no ncurses init) -------------------------------

TEST_CASE("Reset After Loading A Binary Keeps The Program") {
  // Previously reset reassembled the disassembly text, which turned
  // branches into NOPs and mangled immediates.
  auto path = tempPath("fanta_tui_reset.bin");
  Assembler assem{};
  std::vector<uint32_t> image{assem.assemble("MOV R1, #80", 0),
                              assem.assemble("BNE -4", 4),
                              assem.assemble("CALL #8", 8), 0};
  writeBinary(path, image);

  CPU cpu{};
  TUI tui(cpu);
  REQUIRE_TRUE(tui.load_file(path));
  tui.reset();
  for (size_t i = 0; i < image.size(); ++i)
    REQUIRE_SAME(image[i], cpu.load(static_cast<uint32_t>(i * 4)));
  std::filesystem::remove(path);
}

TEST_CASE("Reset Restores A Clean Word-Aligned CPU") {
  CPU cpu{};
  TUI tui(cpu);
  cpu.registers[3] = 42;
  cpu.registers[16] = 0x1234;
  cpu.status_reg[0] = 1;
  cpu.cip_interrupts[0] = 1;
  cpu.store(0x200000, 0xDEADBEEF); // above the old 1 MB clear
  cpu.halted = true;
  cpu.inst_count = 49999; // one instruction before VBLANK

  tui.reset();

  REQUIRE_SAME(0, cpu.registers[3]);
  REQUIRE_SAME(Fanta::Program::kStackTop, cpu.registers[16]);
  REQUIRE_SAME(0, cpu.status_reg[0]);
  REQUIRE_SAME(0, cpu.cip_interrupts[0]);
  REQUIRE_SAME(0, cpu.load(0x200000));
  REQUIRE_TRUE(!cpu.halted);
  REQUIRE_SAME(0, cpu.inst_count); // B14: VBLANK timing restarts too
}

TEST_CASE("TUI Disassembles Near The Top Of 32MB RAM") {
  CPU cpu{};
  TUI tui(cpu);
  auto size = static_cast<uint32_t>(cpu.ram.getSize());
  uint32_t addr = size - 4;
  cpu.store(addr, Instructions::Nop::emit());
  REQUIRE_SAME(std::string{"NOP"}, tui.disassemble(addr));
  REQUIRE_SAME(std::string{"OUT OF BOUNDS"},
               tui.disassemble(size));
}

// --- Editor ----------------------------------------------------------------

TEST_CASE("Editor Newline Splits And Backspace Joins") {
  Editor ed;
  for (char c : std::string{"ADDX"})
    ed.insert(c);
  ed.left();
  ed.newline();
  REQUIRE_SAME(2, ed.lines().size());
  REQUIRE_SAME(std::string{"ADD"}, ed.lines()[0]);
  REQUIRE_SAME(std::string{"X"}, ed.lines()[1]);
  ed.backspace(); // at column 0: join back
  REQUIRE_SAME(1, ed.lines().size());
  REQUIRE_SAME(std::string{"ADDX"}, ed.lines()[0]);
  REQUIRE_SAME(3, ed.x());
}

TEST_CASE("Editor Read-Only Ignores Edits But Moves") {
  Editor ed;
  ed.set_lines({"NOP", "HALT"}, /*read_only=*/true);
  ed.insert('X');
  ed.backspace();
  ed.newline();
  ed.erase_at_cursor();
  ed.replace_first_word("RET");
  REQUIRE_SAME(std::string{"NOP"}, ed.lines()[0]);
  REQUIRE_SAME(2, ed.lines().size());
  ed.down();
  REQUIRE_SAME(1, ed.y());
}

TEST_CASE("Editor Tab Completion Keeps Indentation") {
  Editor ed;
  ed.set_lines({"    AD"});
  ed.replace_first_word("ADD");
  REQUIRE_SAME(std::string{"    ADD "}, ed.lines()[0]);
  REQUIRE_SAME(8, ed.x());
}

TEST_CASE("Editor Moving To A Shorter Line Clamps The Column") {
  Editor ed;
  ed.set_lines({"MOV R1, #1", "RET"});
  for (int i = 0; i < 10; ++i)
    ed.right();
  ed.down();
  REQUIRE_SAME(3, ed.x());
}

// --- Serial terminal layout ------------------------------------------------

TEST_CASE("Terminal Newline And Carriage Return") {
  auto lines = Fanta::Terminal::layout("Hi\nthere\rT", 40);
  REQUIRE_SAME(2, lines.size());
  REQUIRE_SAME(std::string{"Hi"}, lines[0]);
  REQUIRE_SAME(std::string{"There"}, lines[1]); // '\r' then overwrite
}

TEST_CASE("Terminal Full Line Then Newline Is One Line Break") {
  // Deferred wrap: exactly `width` chars then '\n' must not leave a blank.
  auto lines = Fanta::Terminal::layout("abcd\nef", 4);
  REQUIRE_SAME(2, lines.size());
  REQUIRE_SAME(std::string{"abcd"}, lines[0]);
  REQUIRE_SAME(std::string{"ef"}, lines[1]);
}

TEST_CASE("Terminal Wraps Long Lines") {
  auto lines = Fanta::Terminal::layout("abcdef", 4);
  REQUIRE_SAME(2, lines.size());
  REQUIRE_SAME(std::string{"abcd"}, lines[0]);
  REQUIRE_SAME(std::string{"ef"}, lines[1]);
}

TEST_CASE("Terminal Backspace Tab And Unprintables") {
  auto lines = Fanta::Terminal::layout("ab\bX\tY\x01", 40);
  REQUIRE_SAME(1, lines.size());
  REQUIRE_SAME(std::string{"aX      Y."}, lines[0]);
}

TEST_CASE("Dump Includes Serial Output") {
  auto path = tempPath("fanta_tui_dump.txt");
  CPU cpu{};
  TUI tui(cpu);
  cpu.store(0x0200'0000, 'H');
  cpu.store(0x0200'0000, 'i');
  tui.dump_state(path);
  std::ifstream in(path);
  std::string dump((std::istreambuf_iterator<char>(in)),
                   std::istreambuf_iterator<char>());
  REQUIRE_TRUE(dump.contains("[SERIAL]\nHi\n"));
  std::filesystem::remove(path);
}

TEST_CASE("Reset Clears Serial Output") {
  CPU cpu{};
  TUI tui(cpu);
  cpu.store(0x0200'0000, 'x');
  tui.reset();
  REQUIRE_TRUE(cpu.bus.get<BasicSerial<>>().output.empty());
}

// --- Code mode (Fanta source) ----------------------------------------------

namespace {
auto writeText(const std::string &path, const std::string &text) -> void {
  std::ofstream out(path);
  out << text;
}

auto dumpOf(TUI &tui) -> std::string {
  auto path = tempPath("fanta_tui_codemode_dump.txt");
  tui.dump_state(path);
  std::ifstream in(path);
  std::string dump((std::istreambuf_iterator<char>(in)),
                   std::istreambuf_iterator<char>());
  std::filesystem::remove(path);
  return dump;
}

constexpr auto kAddProgram = "fn main() -> int {\n"
                             "    let b : int = 10;\n"
                             "    let c : int = 20;\n"
                             "    return b + c;\n"
                             "}\n";
} // namespace

TEST_CASE("Build Compile Matches The Default Compiler") {
  auto built = Fanta::Build::compile(kAddProgram);
  REQUIRE_TRUE(built.has_value());
  auto direct = Fanta::DefaultCompiler{}.compile(kAddProgram);
  REQUIRE_TRUE(*built == direct);
}

TEST_CASE("Build Compile Reports Errors Instead Of Throwing") {
  auto built = Fanta::Build::compile("fn main() -> int { return nope(1); }");
  REQUIRE_TRUE(!built.has_value());
  REQUIRE_TRUE(!built.error().empty());
}

TEST_CASE("Build Compile Times Out On A Parser Hang") {
  // EOF inside a function body loops forever in the parser (review B6). The
  // TUI must get control back instead of freezing.
  auto start = std::chrono::steady_clock::now();
  auto built = Fanta::Build::compile("fn main() -> int { let x: int = 1;",
                                     std::chrono::milliseconds(200));
  auto took = std::chrono::steady_clock::now() - start;
  REQUIRE_TRUE(!built.has_value());
  REQUIRE_TRUE(built.error().contains("did not finish"));
  REQUIRE_TRUE(took < std::chrono::seconds(2));
}

TEST_CASE("Loading A .fn File Compiles It Into Memory") {
  auto path = tempPath("fanta_tui_add.fn");
  writeText(path, kAddProgram);

  CPU cpu{};
  TUI tui(cpu);
  REQUIRE_TRUE(tui.load_file(path));
  auto expected = Fanta::DefaultCompiler{}.compile(kAddProgram);
  for (size_t i = 0; i < expected.size(); ++i)
    REQUIRE_SAME(expected[i], cpu.load(static_cast<uint32_t>(i * 4)));

  cpu.run_until_halt();
  REQUIRE_SAME(30, cpu.registers[0]);
  REQUIRE_TRUE(dumpOf(tui).contains("SOURCE: FANTA"));
  std::filesystem::remove(path);
}

TEST_CASE("Reset Restarts A .fn Program Without Rebuilding") {
  auto path = tempPath("fanta_tui_reset.fn");
  writeText(path, kAddProgram);

  CPU cpu{};
  TUI tui(cpu);
  REQUIRE_TRUE(tui.load_file(path));
  cpu.run_until_halt();
  tui.reset();
  REQUIRE_TRUE(!cpu.halted);
  REQUIRE_SAME(0, cpu.get_pc());
  cpu.run_until_halt();
  REQUIRE_SAME(30, cpu.registers[0]);
  std::filesystem::remove(path);
}

TEST_CASE("A .fn File That Fails To Compile Still Opens") {
  auto path = tempPath("fanta_tui_bad.fn");
  writeText(path, "fn main() -> int { return nope(1); }\n");

  CPU cpu{};
  TUI tui(cpu);
  cpu.store(0, 0xDEADBEEF); // stale program from before the load
  REQUIRE_TRUE(tui.load_file(path));
  auto dump = dumpOf(tui);
  REQUIRE_TRUE(dump.contains("MESSAGE: Compile error"));
  REQUIRE_TRUE(dump.contains("return nope(1);")); // source is in the editor
  REQUIRE_SAME(0, cpu.load(0)); // nothing stale left running
  std::filesystem::remove(path);
}

// --- Headless screenshots --------------------------------------------------

TEST_CASE("Screenshot Renders The Real Screen At The Requested Size") {
  auto path = tempPath("fanta_tui_screen.txt");
  CPU cpu{};
  TUI tui(cpu);
  REQUIRE_TRUE(tui.screenshot(path, 100, 30));
  std::ifstream in(path);
  std::string first;
  std::getline(in, first);
  REQUIRE_SAME(102, first.size()); // border: '+' + 100 columns + '+'
  std::string screen((std::istreambuf_iterator<char>(in)),
                     std::istreambuf_iterator<char>());
  REQUIRE_TRUE(screen.contains("REGISTERS"));
  REQUIRE_TRUE(screen.contains("size 100x30"));
  std::filesystem::remove(path);
}

TEST_CASE("Screenshot Feeds Keys Before Rendering") {
  auto path = tempPath("fanta_tui_screen_keys.txt");
  CPU cpu{};
  TUI tui(cpu);
  REQUIRE_TRUE(tui.screenshot(path, 100, 30, "iNOP<ESC>"));
  std::ifstream in(path);
  std::string screen((std::istreambuf_iterator<char>(in)),
                     std::istreambuf_iterator<char>());
  REQUIRE_TRUE(screen.contains(">0x000000   NOP")); // PC marker + address gutter
  REQUIRE_TRUE(screen.contains("mode NORMAL"));
  std::filesystem::remove(path);
}

// --- Layout (spec §3.3): checked for every kind at many sizes ---------------

namespace {
using Fanta::Layout::Kind;

constexpr std::pair<int, int> kSizes[] = {
    {80, 24},  {80, 31},  {80, 32},  {99, 30},  {100, 30}, {120, 40},
    {159, 50}, {160, 50}, {200, 60}, {300, 90}, {60, 15} /* clamped */};
constexpr Kind kKinds[] = {Kind::SOURCE, Kind::ASM, Kind::BINARY};
} // namespace

TEST_CASE("Layout Panes Never Overlap And Stay On Screen") {
  for (auto [cols, rows] : kSizes) {
    for (auto kind : kKinds) {
      auto l = Fanta::Layout::compute(cols, rows, kind);
      int w = std::max(cols, Fanta::Layout::kMinCols);
      int h = std::max(rows, Fanta::Layout::kMinRows);
      auto panes = l.panes();
      for (size_t i = 0; i < panes.size(); ++i) {
        if (!panes[i])
          continue;
        REQUIRE_TRUE(!panes[i]->empty());
        REQUIRE_TRUE(panes[i]->inside(w, h));
        for (size_t j = i + 1; j < panes.size(); ++j)
          if (panes[j])
            REQUIRE_TRUE(!panes[i]->overlaps(*panes[j]));
      }
    }
  }
}

TEST_CASE("Layout Always Keeps Status, Hint, Code And Registers") {
  for (auto [cols, rows] : kSizes) {
    for (auto kind : kKinds) {
      auto l = Fanta::Layout::compute(cols, rows, kind);
      REQUIRE_SAME(0, l.status.y);
      REQUIRE_SAME(std::max(rows, Fanta::Layout::kMinRows) - 1, l.hint.y);
      REQUIRE_TRUE(l.code.h >= 10); // room for real code, even at 80x24
      REQUIRE_TRUE(l.registers.has_value());
    }
  }
}

TEST_CASE("Layout Drops The Output Panel First On Short Terminals") {
  REQUIRE_TRUE(!Fanta::Layout::compute(120, 31, Kind::SOURCE).output);
  REQUIRE_TRUE(Fanta::Layout::compute(120, 32, Kind::SOURCE).output.has_value());
  // Two output tabs side by side only when wide.
  REQUIRE_TRUE(!Fanta::Layout::compute(159, 50, Kind::ASM).output_secondary);
  REQUIRE_TRUE(
      Fanta::Layout::compute(160, 50, Kind::ASM).output_secondary.has_value());
}

TEST_CASE("Layout SOURCE Keeps Source And Derived ASM Side By Side At 80x24") {
  auto l = Fanta::Layout::compute(80, 24, Kind::SOURCE);
  REQUIRE_TRUE(l.derived.has_value());
  REQUIRE_SAME(l.code.y, l.derived->y);
  REQUIRE_TRUE(l.register_form == Fanta::Layout::RegisterForm::COMPACT);
}

TEST_CASE("Layout ASM And BINARY Have A Single Code Pane") {
  for (auto kind : {Kind::ASM, Kind::BINARY})
    for (auto [cols, rows] : kSizes)
      REQUIRE_TRUE(!Fanta::Layout::compute(cols, rows, kind).derived);
}

TEST_CASE("Clip Cuts Text At The Pane Edge") {
  using Fanta::Layout::clip;
  using Fanta::Layout::is_cut;
  REQUIRE_SAME(std::string{"hello"}, std::string{clip("hello", 0, 10)});
  REQUIRE_SAME(std::string{"hel"}, std::string{clip("hello", 2, 5)});
  REQUIRE_TRUE(is_cut("hello", 2, 5));
  REQUIRE_TRUE(!is_cut("hello", 0, 5));
  REQUIRE_SAME(std::string{"llo"}, std::string{clip("hello", -2, 10)});
  REQUIRE_TRUE(clip("hello", 10, 10).empty());
}

TEST_CASE("Ctrl+C Leaves Insert Like ESC") {
  auto path = tempPath("fanta_tui_ctrlc.txt");
  CPU cpu{};
  TUI tui(cpu);
  // Type an instruction, leave INSERT with Ctrl+C: back in NORMAL, and the
  // line was assembled into memory exactly as ESC would have done.
  REQUIRE_TRUE(tui.screenshot(path, 100, 30, "iMOV R1, #5<C-c>"));
  std::ifstream in(path);
  std::string screen((std::istreambuf_iterator<char>(in)),
                     std::istreambuf_iterator<char>());
  REQUIRE_TRUE(screen.contains("mode NORMAL"));
  Assembler assem{};
  REQUIRE_SAME(assem.assemble("MOV R1, #5", 0), cpu.load(0));
  std::filesystem::remove(path);
}

TEST_CASE("Ctrl+C Cancels A Command") {
  auto path = tempPath("fanta_tui_ctrlc_cmd.txt");
  CPU cpu{};
  TUI tui(cpu);
  REQUIRE_TRUE(tui.screenshot(path, 100, 30, ":q<C-c>"));
  std::ifstream in(path);
  std::string screen((std::istreambuf_iterator<char>(in)),
                     std::istreambuf_iterator<char>());
  REQUIRE_TRUE(screen.contains("mode NORMAL")); // still running, not quit
  std::filesystem::remove(path);
}

// --- Execution keys (spec §4.1) --------------------------------------------

namespace {
auto loadAdd(TUI &tui) -> std::string {
  auto path = tempPath("fanta_tui_keys.fn");
  writeText(path, kAddProgram);
  REQUIRE_TRUE(tui.load_file(path));
  return path;
}

auto screenAfter(TUI &tui, std::string_view keys) -> std::string {
  auto path = tempPath("fanta_tui_keys_screen.txt");
  REQUIRE_TRUE(tui.screenshot(path, 120, 40, keys));
  std::ifstream in(path);
  std::string screen((std::istreambuf_iterator<char>(in)),
                     std::istreambuf_iterator<char>());
  std::filesystem::remove(path);
  return screen;
}
} // namespace

TEST_CASE("F7 Steps One Instruction And s No Longer Does") {
  CPU cpu{};
  TUI tui(cpu);
  auto path = loadAdd(tui);
  tui.handle_key('s');
  REQUIRE_SAME(0, cpu.get_pc());
  tui.handle_key(KEY_F(7));
  REQUIRE_SAME(8, cpu.get_pc()); // CALL main
  std::filesystem::remove(path);
}

TEST_CASE("F8 Steps Over A Call To The Next Instruction") {
  CPU cpu{};
  TUI tui(cpu);
  auto path = loadAdd(tui);
  tui.handle_key(KEY_F(8)); // over CALL main: main runs to its RET
  REQUIRE_SAME(4, cpu.get_pc());
  REQUIRE_TRUE(!cpu.halted);
  REQUIRE_SAME(30, cpu.registers[0]);
  REQUIRE_SAME(Fanta::Program::kStackTop, cpu.get_sp());
  std::filesystem::remove(path);
}

TEST_CASE("F5 Runs And c No Longer Does") {
  CPU cpu{};
  TUI tui(cpu);
  auto path = loadAdd(tui);
  REQUIRE_TRUE(screenAfter(tui, "c<FRAME>").contains("PAUSED pc 0x000000"));
  REQUIRE_TRUE(screenAfter(tui, "<F5><FRAME>").contains("HALTED at 0x000004"));
  std::filesystem::remove(path);
}

TEST_CASE("F6 Restarts Without Rebuilding, F9 Rebuilds") {
  CPU cpu{};
  TUI tui(cpu);
  auto path = loadAdd(tui);
  // Change "10" to "11" in the source, then restart: the old image runs.
  for (int ch : {KEY_DOWN, int{'i'}})
    tui.handle_key(ch);
  for (int i = 0; i < 20; ++i)
    tui.handle_key(KEY_RIGHT);
  tui.handle_key(KEY_BACKSPACE); // "10;" -> "1;"
  tui.handle_key('1');           // -> "11;"
  tui.handle_key(KEY_F(6));      // also leaves INSERT
  cpu.run_until_halt();
  REQUIRE_SAME(30, cpu.registers[0]);

  tui.handle_key(KEY_F(9));
  cpu.run_until_halt();
  REQUIRE_SAME(31, cpu.registers[0]);
  std::filesystem::remove(path);
}

TEST_CASE("F-Key In INSERT Leaves INSERT First") {
  CPU cpu{};
  TUI tui(cpu);
  auto screen = screenAfter(tui, "iMOV R1, #5<F7>");
  REQUIRE_TRUE(screen.contains("mode NORMAL"));
  REQUIRE_SAME(5, cpu.registers[1]); // assembled on leaving INSERT, then stepped
}

TEST_CASE("F6 Restarts An ASM Program From Its Assembled Image") {
  CPU cpu{};
  TUI tui(cpu);
  screenAfter(tui, "iMOV R1, #5<ESC>");
  cpu.store(0, 0); // the program overwrote itself, say
  tui.handle_key(KEY_F(6));
  Assembler assem{};
  REQUIRE_SAME(assem.assemble("MOV R1, #5", 0), cpu.load(0));
}

TEST_CASE("Commands Mirror The F-Keys") {
  CPU cpu{};
  TUI tui(cpu);
  auto path = loadAdd(tui);
  auto type = [&](std::string_view cmd) {
    tui.handle_key(':');
    for (char ch : cmd)
      tui.handle_key(ch);
    tui.handle_key('\n');
  };
  type("step");
  REQUIRE_SAME(8, cpu.get_pc());
  type("reset");
  REQUIRE_SAME(0, cpu.get_pc());
  type("over");
  REQUIRE_SAME(4, cpu.get_pc());
  REQUIRE_TRUE(screenAfter(tui, ":out memory<CR>").contains("[Memory]"));
  REQUIRE_TRUE(screenAfter(tui, ":help<CR>").contains("HELP"));
  std::filesystem::remove(path);
}

TEST_CASE("Help Explains How To Switch Between Code And Assembly") {
  CPU cpu{};
  TUI tui(cpu);
  auto screen = screenAfter(tui, "?");
  REQUIRE_TRUE(screen.contains(":e prog.fn"));
  REQUIRE_TRUE(screen.contains(":e prog.asm"));
  // Any key closes it without acting on the key.
  REQUIRE_TRUE(!screenAfter(tui, "q").contains("HELP"));
}

// --- Session model (spec §2) -----------------------------------------------

namespace {
auto typeCmd(TUI &tui, std::string_view cmd) -> void {
  tui.handle_key(':');
  for (char ch : cmd)
    tui.handle_key(ch);
  tui.handle_key('\n');
}

auto statusLine(TUI &tui) -> std::string {
  auto screen = screenAfter(tui, "");
  auto start = screen.find('\n') + 1; // skip the top border
  return screen.substr(start, screen.find('\n', start) - start);
}

auto readFile(const std::string &path) -> std::string {
  std::ifstream in(path);
  return {(std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>()};
}

auto editLine2(TUI &tui) -> void { // insert 'X' at the start of line 2
  for (int ch : {KEY_DOWN, int{'i'}, int{'X'}, 27})
    tui.handle_key(ch);
}
} // namespace

TEST_CASE("Editing Marks The Buffer Dirty And :w Saves It") {
  CPU cpu{};
  TUI tui(cpu);
  auto path = loadAdd(tui);
  REQUIRE_TRUE(!statusLine(tui).contains("[+]"));
  editLine2(tui);
  REQUIRE_TRUE(statusLine(tui).contains("[+]"));
  typeCmd(tui, "w");
  REQUIRE_TRUE(!statusLine(tui).contains("[+]"));
  REQUIRE_TRUE(readFile(path).contains("X    let b : int = 10;"));
  std::filesystem::remove(path);
}

TEST_CASE("q Refuses To Quit With Unsaved Changes, q! Quits") {
  CPU cpu{};
  TUI tui(cpu);
  auto path = loadAdd(tui);
  editLine2(tui);
  REQUIRE_TRUE(screenAfter(tui, "q").contains("Unsaved changes"));
  REQUIRE_TRUE(screenAfter(tui, ":q<CR>").contains("Unsaved changes"));
  // :q! stops the key script (the TUI quit), so later keys never run.
  REQUIRE_TRUE(!screenAfter(tui, ":q!<CR>iZZZ").contains("ZZZ"));
  std::filesystem::remove(path);
}

TEST_CASE(":e Refuses To Drop Unsaved Changes, :e! Discards Them") {
  CPU cpu{};
  TUI tui(cpu);
  auto path = loadAdd(tui);
  editLine2(tui);
  typeCmd(tui, "e " + serialFixture());
  REQUIRE_TRUE(statusLine(tui).contains("SOURCE"));
  typeCmd(tui, "e! " + serialFixture());
  REQUIRE_TRUE(statusLine(tui).contains("ASM"));
  std::filesystem::remove(path);
}

TEST_CASE(":e On A Missing File Opens A New Buffer That :w Creates") {
  auto path = tempPath("fanta_tui_new.fn");
  std::filesystem::remove(path);
  CPU cpu{};
  TUI tui(cpu);
  typeCmd(tui, "e " + path);
  REQUIRE_TRUE(statusLine(tui).contains("SOURCE"));
  REQUIRE_TRUE(!std::filesystem::exists(path));
  for (char ch : std::string{"ifn main() -> int { return 7; }"})
    tui.handle_key(ch);
  tui.handle_key(27);
  typeCmd(tui, "w");
  REQUIRE_TRUE(std::filesystem::exists(path));
  tui.handle_key(KEY_F(9));
  cpu.run_until_halt();
  REQUIRE_SAME(7, cpu.registers[0]);
  std::filesystem::remove(path);
}

TEST_CASE("Editing Source Makes The Build STALE Until F9") {
  CPU cpu{};
  TUI tui(cpu);
  auto path = loadAdd(tui);
  REQUIRE_TRUE(!statusLine(tui).contains("STALE"));
  editLine2(tui);
  REQUIRE_TRUE(statusLine(tui).contains("STALE"));
  // Undo the edit so it compiles, then rebuild.
  for (int ch : {int{'i'}, KEY_BACKSPACE, 27})
    tui.handle_key(ch);
  REQUIRE_TRUE(statusLine(tui).contains("STALE")); // any edit counts
  tui.handle_key(KEY_F(9));
  REQUIRE_TRUE(!statusLine(tui).contains("STALE"));
  std::filesystem::remove(path);
}

TEST_CASE("ASM Edit In Place Is Live-Patched") {
  auto path = tempPath("fanta_tui_patch.asm");
  writeText(path, "MOV R1, #1\nMOV R2, #2\nHALT\n");
  CPU cpu{};
  TUI tui(cpu);
  REQUIRE_TRUE(tui.load_file(path));
  // Change "#2" to "#3" on line 1: same lines, so memory is patched.
  tui.handle_key(KEY_DOWN);
  tui.handle_key('i');
  for (int i = 0; i < 10; ++i)
    tui.handle_key(KEY_RIGHT);
  tui.handle_key(KEY_BACKSPACE);
  tui.handle_key('3');
  tui.handle_key(27);
  Assembler assem{};
  REQUIRE_SAME(assem.assemble("MOV R2, #3", 4), cpu.load(4));
  REQUIRE_TRUE(!statusLine(tui).contains("STALE"));
  std::filesystem::remove(path);
}

TEST_CASE("ASM Line Inserted Before Code Is Not Patched And Goes STALE") {
  auto path = tempPath("fanta_tui_moved.asm");
  writeText(path, "MOV R1, #1\nMOV R2, #2\nHALT\n");
  CPU cpu{};
  TUI tui(cpu);
  REQUIRE_TRUE(tui.load_file(path));
  uint32_t before = cpu.load(4);
  // A new line at the top shifts every instruction by 4.
  for (int ch : {int{'i'}, int{'\n'}, 27})
    tui.handle_key(ch);
  REQUIRE_SAME(before, cpu.load(4)); // memory left alone
  REQUIRE_TRUE(statusLine(tui).contains("STALE"));
  tui.handle_key(KEY_F(9));
  Assembler assem{};
  REQUIRE_SAME(assem.assemble("MOV R2, #2", 8), cpu.load(8));
  REQUIRE_TRUE(!statusLine(tui).contains("STALE"));
  std::filesystem::remove(path);
}

TEST_CASE("ASM Line Appended At The End Is Live-Patched") {
  auto path = tempPath("fanta_tui_append.asm");
  writeText(path, "MOV R1, #1\nHALT\n");
  CPU cpu{};
  TUI tui(cpu);
  REQUIRE_TRUE(tui.load_file(path));
  tui.handle_key(KEY_DOWN);
  tui.handle_key('i');
  for (int i = 0; i < 4; ++i)
    tui.handle_key(KEY_RIGHT);
  tui.handle_key('\n');
  for (char ch : std::string{"MOV R3, #3"})
    tui.handle_key(ch);
  tui.handle_key(27);
  Assembler assem{};
  REQUIRE_SAME(assem.assemble("MOV R3, #3", 8), cpu.load(8));
  REQUIRE_TRUE(!statusLine(tui).contains("STALE"));
  std::filesystem::remove(path);
}

TEST_CASE(":eject Only Works On A Built .fn") {
  CPU cpu{};
  TUI tui(cpu);
  REQUIRE_TRUE(tui.load_file(serialFixture()));
  typeCmd(tui, "eject");
  REQUIRE_TRUE(screenAfter(tui, ":out log<CR>").contains("open a .fn first"));
}

TEST_CASE(":eject Refuses What Wouldn't Assemble Back, Writing Nothing") {
  // Compiled prologues contain MOV R15, SP, which the string assembler
  // encodes as MOV R15, #16 (review B16). Until that's fixed, eject must
  // refuse rather than write a program that differs from the build.
  CPU cpu{};
  TUI tui(cpu);
  auto path = loadAdd(tui);
  auto asm_path = std::filesystem::path(path).replace_extension(".asm");
  std::filesystem::remove(asm_path);
  typeCmd(tui, "eject");
  if (std::filesystem::exists(asm_path)) {
    // B16 fixed: the ejected program must be the build, word for word.
    auto image = Fanta::DefaultCompiler{}.compile(kAddProgram);
    REQUIRE_TRUE(statusLine(tui).contains("ASM"));
    for (size_t i = 0; i < image.size(); ++i)
      if ((image[i] >> 26) != 0) // HALT payload bits may differ
        REQUIRE_SAME(image[i], cpu.load(static_cast<uint32_t>(i * 4)));
    std::filesystem::remove(asm_path);
  } else {
    REQUIRE_TRUE(screenAfter(tui, ":out log<CR>")
                     .contains("MOV R15, SP")); // named in the Build log
    REQUIRE_TRUE(statusLine(tui).contains("SOURCE"));
  }
  std::filesystem::remove(path);
}

// --- Review #1 decisions and fixes (docs/reviews/2026-10-31-tui.md) --------

namespace {
auto screenAt(TUI &tui, int cols, int rows, std::string_view keys)
    -> std::string {
  auto path = tempPath("fanta_tui_review1.txt");
  REQUIRE_TRUE(tui.screenshot(path, cols, rows, keys));
  std::string screen = readFile(path);
  std::filesystem::remove(path);
  return screen;
}
} // namespace

TEST_CASE("z Shows VRAM Over The Work Area Without Pausing (Q2)") {
  CPU cpu{};
  TUI tui(cpu);
  REQUIRE_TRUE(tui.load_file(vramFixture()));
  auto screen = screenAt(tui, 120, 40, "<F5><FRAME>z+<FRAME>");
  REQUIRE_TRUE(screen.contains("[VRAM]"));
  REQUIRE_TRUE(screen.contains("zoom 1.25x"));
  REQUIRE_TRUE(screen.contains("RUNNING"));       // z doesn't pause
  REQUIRE_TRUE(!screen.contains("ASM  fanta_vram")); // the code pane is covered
  // z again closes it and resets the zoom.
  REQUIRE_TRUE(screenAt(tui, 120, 40, "z").contains("ASM  fanta_vram_loop.asm"));
}

TEST_CASE("Moved ASM Code Dims The Gutter And Hides The PC Marker (Q3)") {
  auto path = tempPath("fanta_tui_q3.asm");
  writeText(path, "MOV R1, #1\nMOV R2, #2\nHALT\n");
  CPU cpu{};
  TUI tui(cpu);
  REQUIRE_TRUE(tui.load_file(path));
  REQUIRE_TRUE(screenAt(tui, 120, 40, "").contains(">0x000000"));
  auto screen = screenAt(tui, 120, 40, "i<CR><ESC>");
  REQUIRE_TRUE(screen.contains("STALE: code moved"));
  REQUIRE_TRUE(!screen.contains(">0x0000"));
  REQUIRE_TRUE(screen.contains("Line at 0x000000 added/removed"));
  REQUIRE_TRUE(screenAt(tui, 120, 40, "<F9>").contains(">0x000000"));
  std::filesystem::remove(path);
}

TEST_CASE("The Derived Pane Is Called COMPILED (Q4)") {
  CPU cpu{};
  TUI tui(cpu);
  auto path = loadAdd(tui);
  REQUIRE_TRUE(screenAt(tui, 120, 40, "").contains("COMPILED  build #1"));
  std::filesystem::remove(path);
}

TEST_CASE("Output Follows The Device Last Written Until A Tab Is Picked (Q5)") {
  CPU cpu{};
  TUI tui(cpu);
  REQUIRE_TRUE(tui.load_file(vramFixture()));
  REQUIRE_TRUE(screenAt(tui, 120, 40, "<F5><FRAME><FRAME>").contains("[VRAM]"));
  REQUIRE_TRUE(tui.load_file(serialFixture()));
  REQUIRE_TRUE(screenAt(tui, 120, 40, "<F5><FRAME>").contains("[Serial]"));
  // Picking a tab pins it; a restart follows devices again.
  REQUIRE_TRUE(tui.load_file(vramFixture()));
  REQUIRE_TRUE(
      screenAt(tui, 120, 40, ":out memory<CR><F5><FRAME><FRAME>").contains(
          "[Memory]"));
  REQUIRE_TRUE(screenAt(tui, 120, 40, "<F6><F5><FRAME><FRAME>").contains(
      "[VRAM]"));
}

TEST_CASE("80x24 Status Line Keeps Room For The Message") {
  CPU cpu{};
  TUI tui(cpu);
  auto path = loadAdd(tui);
  // Break the source and rebuild: the error must be on screen at 80x24.
  for (int ch : {KEY_DOWN, int{'i'}, int{'@'}, 27})
    tui.handle_key(ch);
  auto screen = screenAt(tui, 80, 24, "<F9>");
  auto status = screen.substr(screen.find('\n') + 1, 82);
  REQUIRE_TRUE(status.contains("#2 FAILED"));
  REQUIRE_TRUE(status.contains("Compile error: "));
  std::filesystem::remove(path);
}

TEST_CASE("80x24 Compact Registers Show The Flags") {
  CPU cpu{};
  TUI tui(cpu);
  auto path = loadAdd(tui);
  REQUIRE_TRUE(screenAt(tui, 80, 24, "").contains("F znvc"));
  std::filesystem::remove(path);
}

TEST_CASE("Help Fits In 80x24") {
  CPU cpu{};
  TUI tui(cpu);
  REQUIRE_TRUE(screenAt(tui, 80, 24, "?").contains("Hold fn"));
}

TEST_CASE("Hint Bar Follows The Run State And Session Kind") {
  CPU cpu{};
  TUI tui(cpu);
  REQUIRE_TRUE(tui.load_file(vramFixture()));
  REQUIRE_TRUE(screenAt(tui, 120, 40, "<F5><FRAME>").contains("F5 pause"));
  auto bin = tempPath("fanta_tui_hint.bin");
  writeBinary(bin, {0x10200080, 0});
  REQUIRE_TRUE(tui.load_file(bin));
  auto screen = screenAt(tui, 120, 40, "");
  REQUIRE_TRUE(!screen.contains("F9 build"));
  REQUIRE_TRUE(!screen.contains("i edit"));
  std::filesystem::remove(bin);
}

TEST_CASE("F9 On A Binary Does Nothing") {
  auto bin = tempPath("fanta_tui_f9.bin");
  Assembler assem{};
  writeBinary(bin, {assem.assemble("MOV R1, #5", 0), 0});
  CPU cpu{};
  TUI tui(cpu);
  REQUIRE_TRUE(tui.load_file(bin));
  tui.handle_key(KEY_F(7));
  tui.handle_key(KEY_F(9));
  REQUIRE_SAME(4, cpu.get_pc()); // not restarted
  std::filesystem::remove(bin);
}

TEST_CASE(":mem Jumps The Memory Tab To An Address") {
  CPU cpu{};
  TUI tui(cpu);
  cpu.store(0x1230, 0x44434241); // "ABCD"
  auto screen = screenAt(tui, 120, 40, ":mem 1234<CR>");
  REQUIRE_TRUE(screen.contains("[Memory]"));
  REQUIRE_TRUE(screen.contains("0x001230  41 42 43 44"));
}

TEST_CASE("Messages Expire On The Next Key, Build Errors Stay") {
  CPU cpu{};
  TUI tui(cpu);
  auto path = loadAdd(tui);
  REQUIRE_TRUE(screenAt(tui, 120, 40, "").contains("Loaded"));
  REQUIRE_TRUE(!screenAt(tui, 120, 40, "j").contains("Loaded"));
  REQUIRE_TRUE(screenAt(tui, 120, 40, ":out log<CR>").contains("Loaded"));
  for (int ch : {int{'i'}, int{'@'}, 27})
    tui.handle_key(ch);
  REQUIRE_TRUE(screenAt(tui, 120, 40, "<F9>jjk").contains("Compile error"));
  std::filesystem::remove(path);
}

TEST_CASE("A New .fn Says It Isn't Built Instead Of Showing Empty Memory") {
  auto path = tempPath("fanta_tui_unbuilt.fn");
  std::filesystem::remove(path);
  CPU cpu{};
  TUI tui(cpu);
  REQUIRE_TRUE(tui.load_file(path));
  auto screen = screenAt(tui, 120, 40, "");
  REQUIRE_TRUE(screen.contains("not built yet"));
  REQUIRE_TRUE(!screen.contains("HALT"));
}

TEST_CASE("Headless F10 Uses The Screenshot Size") {
  CPU cpu{};
  TUI tui(cpu);
  // At 120x40 there is an output panel, so F10 cycles tabs, not the overlay.
  auto screen = screenAt(tui, 120, 40, "<F10>");
  REQUIRE_TRUE(screen.contains("[VRAM]"));
  REQUIRE_TRUE(screen.contains("REGISTERS"));
}
