{ uint64_t temp = d0 & d1 & d2 & d4 & d5 & d6 & (~current_on) & (~s2) & (~s1) & on1; next_on |= temp; next_unknown |= temp; }
next_on |= d1 & d2 & d5 & d6 & (~current_unknown) & (~s1) & (~s0) & on1 & (~on0);
{ uint64_t temp = d0 & d1 & d2 & d4 & d5 & d6 & (~current_on) & (~s1) & (~s0) & (~on1); next_on |= temp; next_unknown |= temp; }
next_unknown |= d0 & d1 & d2 & (~d5) & d6 & current_on & (~s0) & on0;
next_on |= l3 & d4 & d5 & (~current_unknown) & s1 & s0 & on1 & (~on0);
next_unknown |= d2 & d4 & d5 & d6 & current_unknown & s1 & (~s0) & on0;
{ uint64_t temp = l2 & l3 & d0 & (~current_on) & (~s2) & (~s1) & (~s0) & on1 & on0; next_on |= temp; next_unknown |= temp; }
{ uint64_t temp = d2 & d4 & d5 & d6 & (~current_on) & s1 & (~s0) & on1 & on0; next_on |= temp; next_unknown |= temp; }
{ uint64_t temp = d0 & d1 & d2 & d4 & d5 & d6 & (~current_on) & on2; next_on |= temp; next_unknown |= temp; }
next_unknown |= d1 & d2 & (~d5) & d6 & current_on & s0 & (~on0);
{ uint64_t temp = d0 & d1 & current_on & (~s2) & (~s1) & (~s0) & on1 & on0; next_on |= temp; next_unknown |= temp; }
next_on |= l3 & d5 & current_unknown & s1 & s0 & (~on1) & on0;
{ uint64_t temp = l2 & d2 & (~current_unknown) & (~s2) & s1 & (~s0) & on1 & on0; next_on |= temp; next_unknown |= temp; }
next_unknown |= (~d1) & d2 & d4 & d5 & d6 & current_on & on0;
{ uint64_t temp = d1 & d2 & current_on & (~s2) & (~s1) & s0 & on1 & on0; next_on |= temp; next_unknown |= temp; }
next_unknown |= d1 & d2 & d4 & current_on & (~s0) & (~on2) & (~on0);
{ uint64_t temp = l2 & d1 & d2 & current_on & (~s2) & (~s1) & on1 & (~on0); next_on |= temp; next_unknown |= temp; }
{ uint64_t temp = l3 & d4 & (~current_on) & s2 & (~s1) & (~s0) & on1 & on0; next_on |= temp; next_unknown |= temp; }
{ uint64_t temp = l3 & d4 & d5 & (~current_unknown) & s1 & (~s0) & (~on1) & on0; next_on |= temp; next_unknown |= temp; }
{ uint64_t temp = l2 & l3 & d4 & d5 & (~s1) & s0 & (~on2) & (~on1); next_on |= temp; next_unknown |= temp; }
next_unknown |= d0 & d4 & d5 & d6 & current_on & (~s0) & (~on0);
{ uint64_t temp = l2 & l3 & d2 & (~current_on) & (~s2) & s1 & (~s0) & on1; next_on |= temp; next_unknown |= temp; }
{ uint64_t temp = l2 & l3 & d1 & (~current_on) & (~s2) & (~s0) & on1 & (~on0); next_on |= temp; next_unknown |= temp; }
{ uint64_t temp = l2 & l3 & d1 & (~current_on) & (~s2) & s0 & on1 & on0; next_on |= temp; next_unknown |= temp; }
{ uint64_t temp = l2 & l3 & d2 & (~s2) & (~s1) & s0 & on1 & (~on0); next_on |= temp; next_unknown |= temp; }
next_on |= l2 & d2 & d6 & (~s1) & s0 & on1 & (~on0);
next_unknown |= l3 & d0 & d1 & d2 & d4 & d5 & d6;
next_unknown |= l2 & d0 & d1 & d2 & d4 & d5 & d6;
{ uint64_t temp = l2 & l3 & d2 & (~s2) & (~s1) & (~s0) & (~on1) & on0; next_on |= temp; next_unknown |= temp; }
next_unknown |= l2 & l3 & d0 & d1 & d2 & d5 & d6;
{ uint64_t temp = d5 & d6 & (~current_unknown) & s1 & (~on2) & (~on1) & (~on0); next_on |= temp; next_unknown |= temp; }
next_unknown |= l2 & l3 & d0 & d1 & d4 & d5 & d6;
next_unknown |= l2 & l3 & d0 & d1 & d2 & d4 & d6;
{ uint64_t temp = l2 & l3 & d4 & (~current_on) & s1 & (~s0) & (~on1) & on0; next_on |= temp; next_unknown |= temp; }
next_unknown |= l2 & l3 & d0 & d2 & d4 & d5 & d6;
next_unknown |= l2 & l3 & d0 & d1 & d2 & d4 & d5;
next_on |= l2 & l3 & d4 & s1 & s0 & on1 & on0;
next_unknown |= l2 & l3 & (~current_on) & (~s1) & s0 & (~on1) & on0;
next_unknown_stable |= d0 & d1 & d2 & d4 & d5 & d6;
next_unknown |= l2 & l3 & d1 & d2 & d4 & d5 & d6;
{ uint64_t temp = d4 & (~d5) & current_on & (~s1) & s0 & (~on1) & on0; next_on |= temp; next_unknown |= temp; }
{ uint64_t temp = l3 & d6 & s1 & s0 & (~on2) & (~on1) & (~on0); next_on |= temp; next_unknown |= temp; }
{ uint64_t temp = d5 & d6 & (~current_unknown) & s1 & s0 & (~on1) & on0; next_on |= temp; next_unknown |= temp; }
{ uint64_t temp = d4 & current_on & (~s1) & (~s0) & (~on2) & (~on1) & (~on0); next_on |= temp; next_unknown |= temp; }
{ uint64_t temp = l3 & d4 & s2 & (~s1) & (~s0) & (~on1) & (~on0); next_on |= temp; next_unknown |= temp; }
next_on |= l2 & (~current_on) & s1 & (~s0) & on1 & (~on0);
next_on |= l2 & (~current_on) & s1 & s0 & on1 & on0;
next_on |= l2 & (~current_on) & (~s1) & s0 & (~on1) & on0;
next_unknown |= d4 & d5 & d6 & current_on & s0 & on0;
{ uint64_t temp = l2 & l3 & d4 & (~current_on) & s1 & on1 & (~on0); next_on |= temp; next_unknown |= temp; }
{ uint64_t temp = l2 & l3 & (~current_on) & (~s1) & (~s0) & (~on1) & (~on0); next_on |= temp; next_unknown |= temp; }
next_unknown |= d1 & d2 & d4 & current_on & s0 & on0;
{ uint64_t temp = l2 & l3 & d4 & (~current_on) & (~s1) & (~on1) & (~on0); next_on |= temp; next_unknown |= temp; }
{ uint64_t temp = l2 & l3 & d1 & d5 & (~current_on) & (~s0) & (~on0); next_on |= temp; next_unknown |= temp; }
{ uint64_t temp = l2 & l3 & d1 & d5 & (~current_on) & s0 & on0; next_on |= temp; next_unknown |= temp; }
{ uint64_t temp = d0 & current_on & (~s2) & (~s1) & (~s0) & on2; next_on |= temp; next_unknown |= temp; }
next_unknown |= (~l2) & current_unknown & s1 & s0 & (~on0);
{ uint64_t temp = l2 & l3 & d4 & s1 & (~s0) & on1 & (~on0); next_on |= temp; next_unknown |= temp; }
{ uint64_t temp = d2 & current_on & (~s2) & s1 & (~s0) & on2; next_on |= temp; next_unknown |= temp; }
{ uint64_t temp = l2 & d1 & current_on & (~s2) & s0 & on2; next_on |= temp; next_unknown |= temp; }
{ uint64_t temp = d1 & d5 & current_on & (~s1) & s0 & on2; next_on |= temp; next_unknown |= temp; }
next_unknown_stable |= (~l3) & (~d4) & s0 & (~on0);
next_unknown_stable |= (~l2) & (~d2) & s0 & (~on0);
next_on |= (~d6) & (~s1) & (~s0) & on1 & on0;
next_unknown_stable |= (~l2) & (~d2) & (~s0) & on0;
next_unknown_stable |= (~l2) & s0 & (~on1) & (~on0);
{ uint64_t temp = (~l3) & (~s1) & (~s0) & (~on1) & on0; next_on |= temp; next_unknown_stable |= temp; }
next_unknown_stable |= (~l3) & (~s0) & on1 & on0;
next_unknown_stable |= (~d4) & s1 & (~on1);
next_unknown_stable |= (~l2) & s1 & (~on1);
next_unknown_stable |= (~l3) & (~s1) & on1;
{ uint64_t temp = s2 & (~on2) & (~on1) & (~on0); next_on |= temp; next_unknown |= temp; }
next_unknown_stable |= s1 & (~on1) & (~on0);
next_unknown_stable |= s1 & s0 & (~on1);
next_unknown_stable |= (~s1) & (~s0) & on1;
next_unknown_stable |= (~s1) & on1 & on0;
{ uint64_t temp = d6 & s2 & (~on1) & on0; next_on |= temp; next_unknown |= temp; }
{ uint64_t temp = l2 & l3 & (~current_on) & on2; next_on |= temp; next_unknown |= temp; }
{ uint64_t temp = current_on & on2 & on1; next_on |= temp; next_unknown |= temp; }
next_unknown_stable |= l2 & l3;
{ uint64_t temp = current_on & on2 & on0; next_on |= temp; next_unknown |= temp; }
next_unknown_stable |= on2;
next_on = ~next_on;
next_unknown = ~next_unknown;
next_unknown_stable = ~next_unknown_stable;
