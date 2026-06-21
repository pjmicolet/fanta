#pragma once
#include "assembler.hpp"
#include "instructions.hpp"

constexpr auto generate_line() {
  using namespace Instructions;
  return Program<
     Mov<Reg<0>, Literal<200>>,
     Load<Reg<1>, Reg<0>, Literal<0>>,//Assume this is x0
     Load<Reg<2>, Reg<0>, Literal<4>>,//Assume this is x1
     Load<Reg<3>, Reg<0>, Literal<8>>,//Assume this is y0
     Load<Reg<4>, Reg<0>, Literal<12>>,//Assume this is y1

     Sub<Reg<5>, Reg<2>, Reg<1>>, // dx
     Sub<Reg<6>, Reg<4>, Reg<3>>, // dy

     Lsh<Reg<7>, Reg<6>, Literal<1>>, // D = 2*dy
     Sub<Reg<7>, Reg<7>, Reg<5>>, // D = D - dx

     Mov<Reg<4>,Reg<3>>, // set y1 = y0 for now
     Lsh<Reg<4>, Reg<4>, Literal<8>>, // y * 256
     Lsh<Reg<0>, Reg<3>, Literal<6>>, // y * 64

     Add<Reg<4>, Reg<4>, Reg<0>>, // y = y * 256 + y * 64
     Lsh<Reg<4>, Reg<4>, Literal<2>>, // y = y * 256 + y * 64
     Lsh<Reg<1>, Reg<1>, Literal<2>>, // x = x * 4
     Add<Reg<1>, Reg<1>, Reg<4>>, // x,y = (x+y) addr
     Mov<Reg<0>, Literal<250>>, // load some base addr in reg0
     Add<Reg<0>, Reg<0>, Reg<1>>,// incr the x,y offset

     // at this point r1 is free, r4 is also free, and also so is r3 actually...
     Mov<Reg<3>, Literal<216>>,
     Load<Reg<4>, Reg<3>, Literal<0>>,// Clobber y1 with the color
    // So I need 2dy and 2(dy-dx), I need to keep x, D and ptr and value and xmax
    // so use r0 as addr, r1 is 2(dy-dx), r2 dx (we can dec it until it's 0 and that's the loop)
    // r6 is 2dy, R7 is D, we'll use r5 as color.
    Sub<Reg<1>, Reg<6>, Reg<5>>, // We no longer need R1 as well since we are iterating from x0 to x1, so we can reuse r1 moving fwd. Setting it to dy-dx as it's used elsewhere
    Lsh<Reg<1>, Reg<1>, Literal<1>>, // It's actually *2 for the error
    Mov<Reg<2>, Reg<5>>, // using r2 as dx which we'll decrement
    Add<Reg<2>, Reg<2>, Literal<1>>, // using r2 as dx which we'll decrement
    Mov<Reg<5>, Literal<160>>, // using r5 as precalc 320*4 
    Lsh<Reg<5>, Reg<5>, Literal<3>>, // using r5 as precalc 320*4 
    Lsh<Reg<6>, Reg<6>, Literal<1>>, // dy is only really used as 2dy elsewhere
// Write pixel
    Store<Reg<4>, Reg<0>, Literal<0>>,// do the plot using Reg4 and Reg3 with Reg<0> as memory base
// check if D > 0
    Cmp<Reg<7>, Literal<0>>, // if D > 0
    Bpl<Target<12>>,
// if no
    Add<Reg<7>, Reg<7>, Reg<6>>, // D = D + 2dy
    JmpRel<Target<12>>,
    Add<Reg<0>, Reg<0>, Reg<5>>, // y = y + 1, given we start with y = y0, we're reusing Reg 3  
    Add<Reg<7>, Reg<7>, Reg<1>>, // D = D + 2*(dy-dx)
    Add<Reg<0>, Reg<0>, Literal<4>>,
    Sub<Reg<2>, Reg<2>, Literal<1>>,
    Bne<Target<-36>>,
    Cmp<Reg<0>, Reg<0>>, // just an infinite loop
    Beq<Target<-8>>
  >::load();
}
