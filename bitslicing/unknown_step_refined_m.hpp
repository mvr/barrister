next_on |= l3 & d4 & d5 & (~current_unknown) & (~m3) & (~m1) & m0;
next_unknown |= (~d1) & d2 & d4 & d5 & d6 & current_on & on0;
next_on |= l2 & l3 & d4 & (~m3) & (~m1) & (~m0);
next_unknown |= d2 & (~d5) & d6 & current_on & (~m2) & m0;
next_unknown |= d0 & d4 & d5 & d6 & current_on & (~m0);
next_unknown |= l3 & d0 & d1 & d4 & d5 & d6;
next_unknown |= l3 & d1 & d2 & d5 & d6 & (~m2);
next_unknown |= l3 & d1 & d2 & d4 & d5 & d6;
next_unknown |= l3 & d0 & d2 & d4 & d5 & d6;
{ uint64_t temp = l2 & l3 & d4 & (~current_on) & (~m3) & (~m1); next_on |= temp; next_unknown |= temp; }
next_unknown |= l3 & d2 & d4 & d6 & (~m2);
{ uint64_t temp = l3 & d5 & (~current_on) & (~m2) & m1 & (~m0); next_on |= temp; next_unknown |= temp; }
{ uint64_t temp = l2 & d2 & m2 & m1 & m0; next_on |= temp; next_unknown |= temp; }
{ uint64_t temp = (~l2) & current_unknown & (~m1) & m0; next_unknown |= temp; next_unknown_stable |= temp; }
{ uint64_t temp = d1 & d2 & m2 & m1 & (~m0); next_on |= temp; next_unknown |= temp; }
{ uint64_t temp = d2 & d6 & (~current_on) & m1 & m0; next_on |= temp; next_unknown |= temp; }
{ uint64_t temp = d5 & d6 & (~current_unknown) & (~m2) & m1; next_on |= temp; next_unknown |= temp; }
next_on |= d6 & (~m2) & m1 & m0;
next_unknown_stable |= (~d4) & (~m1) & m0;
next_unknown |= d4 & current_on & (~m2) & (~m0);
next_unknown |= l2 & d4 & d5 & (~m2);
{ uint64_t temp = l2 & (~current_on) & (~m1) & (~m0); next_on |= temp; next_unknown |= temp; }
{ uint64_t temp = d1 & (~current_on) & m2 & (~m0); next_on |= temp; next_unknown |= temp; }
{ uint64_t temp = d0 & (~current_on) & m2 & (~m1); next_on |= temp; next_unknown |= temp; }
{ uint64_t temp = d0 & d1 & m2 & (~m1); next_on |= temp; next_unknown |= temp; }
{ uint64_t temp = d0 & m2 & (~m1) & (~m0); next_on |= temp; next_unknown |= temp; }
next_unknown_stable |= (~l3) & m2;
next_unknown_stable |= (~d2) & m1;
next_unknown_stable |= m2 & (~m0);
next_unknown_stable |= (~m2) & m1;
next_on |= m3 & (~m2);
next_on = ~next_on;
next_unknown = ~next_unknown;
next_unknown_stable = ~next_unknown_stable;

// WORKING:
// next_unknown_stable |= l3 & d2 & m2 & m1 & m0;
// { uint64_t temp = l2 & d0 & (~current_on) & (~s2) & (~s1) & on0 & (~m1); next_on |= temp; next_unknown |= temp; }
// next_unknown |= (~d1) & d2 & d4 & d5 & d6 & current_on & on0;
// next_unknown |= d1 & d2 & (~d5) & d6 & current_on & (~m2) & m0;
// { uint64_t temp = l2 & d1 & (~current_on) & (~s2) & on1 & (~m0); next_on |= temp; next_unknown |= temp; }
// next_unknown |= d0 & d4 & d5 & d6 & current_on & (~m0);
// next_unknown |= l3 & d0 & d1 & d4 & d5 & d6;
// next_unknown |= l3 & d0 & d1 & d2 & d5 & d6;
// { uint64_t temp = (~current_on) & (~on1) & (~on0) & (~m1) & (~m0); next_on |= temp; next_unknown |= temp; }
// next_unknown |= l3 & d1 & d2 & d4 & d5 & m1;
// next_unknown |= l3 & d0 & d2 & d4 & d5 & d6;
// next_unknown |= l3 & d1 & d2 & d4 & d5 & d6;
// next_unknown |= l3 & d2 & d4 & d6 & (~m2) & (~m0);
// { uint64_t temp = l2 & (~l3) & d4 & (~m2) & (~m1); next_unknown |= temp; next_unknown_stable |= temp; }
// { uint64_t temp = l2 & l3 & d4 & (~m2) & (~m1) & (~m0); next_on |= temp; next_unknown |= temp; }
// { uint64_t temp = l3 & d4 & d5 & (~m2) & (~m1) & m0; next_on |= temp; next_unknown |= temp; }
// next_unknown |= d2 & d4 & current_on & (~m2) & (~m0);
// { uint64_t temp = l2 & d2 & m2 & m1 & m0; next_on |= temp; next_unknown |= temp; }
// { uint64_t temp = d2 & (~current_on) & m2 & m1 & m0; next_on |= temp; next_unknown |= temp; }
// { uint64_t temp = d4 & (~d5) & (~current_on) & (~m2) & (~m1); next_on |= temp; next_unknown |= temp; }
// { uint64_t temp = d1 & d2 & m2 & m1 & (~m0); next_on |= temp; next_unknown |= temp; }
// { uint64_t temp = l2 & d5 & (~current_on) & (~m2) & (~m0); next_on |= temp; next_unknown |= temp; }
// { uint64_t temp = (~current_on) & (~on1) & (~on0) & m2; next_on |= temp; next_unknown |= temp; }
// next_unknown_stable |= (~m2) & (~m1) & (~m0);
// { uint64_t temp = d0 & d1 & m2 & (~m1); next_on |= temp; next_unknown |= temp; }
// { uint64_t temp = d0 & m2 & (~m1) & (~m0); next_on |= temp; next_unknown |= temp; }
// { uint64_t temp = d6 & (~m2) & m1 & m0; next_on |= temp; next_unknown |= temp; }
// { uint64_t temp = d5 & d6 & (~m2) & m1; next_on |= temp; next_unknown |= temp; }
// next_on = ~next_on;
// next_unknown = ~next_unknown;
