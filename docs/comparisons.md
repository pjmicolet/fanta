# Managing comparisons with a single CMP instruction

CMP sets a bunch of different flags:

```
template <typename DestDecoder, typename OptDecoder> struct OpCmp {
  static auto exec(CPU &cpu, uint32_t inst) {
    auto data = static_cast<std::int32_t>(DestDecoder::decode(cpu, inst));
    auto data2 = static_cast<std::int32_t>(OptDecoder::decode(cpu, inst));
    if (data == data2) {
      cpu.set_zero(0);
    } else {
      cpu.set_zero(1);
    }
    if (data > data2) {
      cpu.set_carry(1);
    } else {
      cpu.set_carry(0);
    }

    cpu.set_neg(data - data2);
  }
};
```

This means that different operators (==, !=, <,>,<=,>=) for branching will use the CMP followed 
by a branch on one of the flags.

For example:

```
if(a >= b) {
    //BODY
}
//OUT
``` 

should do:

CMP A, B
Bmi OUT

Rather than double a double Branch of either zero is set or carry is set


So this leaves us with:

- >= : CMP + Bmi 
- > : CMP + Bnc
- == : CMP + Bne
- != : CMP + Beq
- <= : CMP + Bec
- < : CMP + Bpl

