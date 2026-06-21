#include "codegen.hpp"
#include <variant>

namespace Fanta {

template <class... Ts> struct overloaded : Ts... {
  using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

auto Codegen::extractGlobalNames(const Parser &p) -> void {
  uint32_t globalOffsets = 0;
  for (auto &ridx : p.getRootIndices()) {
    const auto &node = p.getNodeAtIndex(ridx);

    std::visit(overloaded{[&](const Fanta::AST::VariableDecl &d) {
                            GlobalVarInfo gni{globalOffsets, 0, d.type};
                            globalNameSpace[d.name] = gni;
                            globalOffsets += 4;
                          },
                          [&](const Fanta::AST::FunctionDef &d) {
                            GlobalFuncInfo gni{
                                0, d.retType, ridx, d.params.size(), {}};
                            for (const auto &pType : d.params) {
                              const auto var =
                                  std::get<Fanta::AST::FunctionParamDef>(
                                      p.getNodeAtIndex(pType).t);
                              gni.paramTypes.push_back(var.type);
                            }
                            globalNameSpace[d.name] = gni;
                          },
                          [&](const auto &other) {}},
               node.t);
  }
  return;
}
} // namespace Fanta
