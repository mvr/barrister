naive_next_on |= current_on & on2 & (~on1) & (~on0) & (~unk2) & (~unk1) & (~unk0);
naive_next_unknown |= (~current_unknown) & (~on2) & (~on1) & (~on0) & (~unk3) & (~unk2) & (~unk0);
naive_next_on |= (~on2) & on1 & on0 & (~unk2) & (~unk1) & (~unk0);
naive_next_unknown |= (~current_unknown) & (~on2) & (~on1) & (~unk3) & (~unk2) & (~unk1);
{ uint64_t temp = current_on & (~on2) & on1 & on0 & (~unk2) & (~unk1); naive_next_unknown |= temp; naive_next_on |= temp; }
naive_next_unknown |= (~current_unknown) & (~current_on) & on2;
naive_next_unknown |= (~unk3) & (~unk2) & (~unk1) & (~unk0);
naive_next_unknown |= current_on & on2 & on0;
naive_next_unknown |= current_on & on2 & on1;
naive_next_unknown = ~naive_next_unknown;

// naive_next_on |= current_on & on2 & (~on1) & (~on0) & (~unk1) & (~unk0);
// naive_next_on |= (~on2) & on1 & on0 & (~unk1) & (~unk0);
// naive_next_unknown |= (~current_unknown) & (~on2) & (~on1) & (~on0) & (~unk0);
// { uint64_t temp = current_on & (~on2) & on1 & on0 & (~unk1); naive_next_unknown |= temp; naive_next_on |= temp; }
// naive_next_unknown |= (~current_unknown) & (~on2) & (~on1) & (~unk1);
// naive_next_unknown |= (~current_unknown) & (~current_on) & on2;
// naive_next_unknown |= current_on & on2 & on0;
// naive_next_unknown |= current_on & on2 & on1;
// naive_next_unknown |= (~unk1) & (~unk0);
// naive_next_unknown = ~naive_next_unknown;
