d4 |= stateunk & (~on2) & (~on1) & (~on0) & unk2 & (~unk1) & (~unk0);
d5 |= stateunk & (~on2) & (~on1) & unk2 & (~unk1) & (~unk0);
d5 |= stateunk & (~on2) & (~on1) & (~on0) & unk2 & (~unk1);
{ uint64_t temp = stateunk & (~on1) & (~on0) & (~unk2) & unk1 & (~unk0); l2 |= temp; d2 |= temp; }
d5 |= (~on2) & (~on1) & (~on0) & unk2 & (~unk1) & (~unk0);
{ uint64_t temp = stateunk & on0 & (~unk2) & (~unk1) & unk0; l2 |= temp; d2 |= temp; d4 |= temp; }
{ uint64_t temp = (~stateon) & (~on1) & (~unk3) & (~unk2) & (~unk1) & (~unk0); d2 |= temp; d6 |= temp; abort |= temp; }
{ uint64_t temp = (~stateon) & (~on2) & on0 & (~unk2) & (~unk1) & unk0; d5 |= temp; d6 |= temp; abort |= temp; }
d6 |= stateunk & (~on1) & on0 & (~unk1) & unk0;
d4 |= stateunk & (~on1) & on0 & (~unk2) & unk0;
d6 |= stateunk & (~on1) & (~on0) & unk1 & (~unk0);
d6 |= stateunk & on1 & (~on0) & (~unk1) & (~unk0);
{ uint64_t temp = (~on2) & (~on0) & (~unk3) & (~unk2) & (~unk1) & unk0; l3 |= temp; d4 |= temp; d5 |= temp; abort |= temp; }
d6 |= (~on2) & (~on1) & unk2 & (~unk1) & (~unk0);
d4 |= stateunk & on1 & (~on0) & (~unk2) & (~unk0);
{ uint64_t temp = (~on1) & on0 & (~unk2) & (~unk1) & unk0; l3 |= temp; d4 |= temp; }
d6 |= (~on2) & (~on1) & (~on0) & unk2 & (~unk1);
d5 |= stateunk & (~on2) & (~unk2) & unk1 & (~unk0);
{ uint64_t temp = stateunk & (~on0) & (~unk3) & (~unk2) & (~unk1); d1 |= temp; d5 |= temp; }
{ uint64_t temp = (~on0) & (~unk3) & (~unk2) & (~unk1) & (~unk0); d1 |= temp; d5 |= temp; }
{ uint64_t temp = (~stateon) & (~on2) & on1 & (~on0) & (~unk2); d6 |= temp; abort |= temp; }
{ uint64_t temp = (~on2) & (~on1) & (~unk2) & unk1 & (~unk0); l3 |= temp; d4 |= temp; }
{ uint64_t temp = (~on2) & (~on1) & (~on0) & (~unk2) & unk1; l3 |= temp; d4 |= temp; }
{ uint64_t temp = (~on1) & (~on0) & (~unk3) & (~unk2) & (~unk1); l2 |= temp; d2 |= temp; d6 |= temp; }
{ uint64_t temp = (~on2) & (~unk3) & (~unk2) & (~unk1) & (~unk0); l3 |= temp; d4 |= temp; d5 |= temp; }
abort |= stateon & on1 & on0;
d5 |= stateunk & on1 & (~on0) & (~unk2);
d6 |= (~on2) & (~unk2) & unk1 & (~unk0);
d5 |= on1 & (~on0) & (~unk2) & (~unk0);
d6 |= stateunk & (~on2) & on1 & (~unk2);
{ uint64_t temp = (~stateon) & on1 & on0; l2 |= temp; d2 |= temp; }
{ uint64_t temp = (~on2) & (~on1) & (~unk2) & unk1; d5 |= temp; d6 |= temp; }
{ uint64_t temp = on2 & (~on1) & (~on0); l2 |= temp; d0 |= temp; abort |= temp; }
{ uint64_t temp = (~stateunk) & (~stateon); l2 |= temp; l3 |= temp; }
d4 |= on2 & on0;
abort |= (~on2) & unk2;
abort |= (~on2) & unk1;
{ uint64_t temp = on2 & on1; d4 |= temp; d5 |= temp; }
{ uint64_t temp = (~stateon) & on2; l2 |= temp; l3 |= temp; d1 |= temp; d2 |= temp; abort |= temp; }
abort |= unk3;
d0 |= on0;
{ uint64_t temp = on1; d0 |= temp; d1 |= temp; }
{ uint64_t temp = stateon; d1 |= temp; d2 |= temp; d4 |= temp; d5 |= temp; d6 |= temp; }
abort = ~abort;
