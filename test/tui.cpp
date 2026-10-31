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

TEST_CASE("Reset Recompiles And Restarts A .fn Program") {
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
