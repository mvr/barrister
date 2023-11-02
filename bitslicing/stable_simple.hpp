{ uint64_t temp = (~stateunk) & (~stateon) & (~on2) & on1 & on0 & (~unk1) & unk0; signal_off |= temp; signal_on |= temp; abort |= temp; }
signal_on |= stateon & (~on2) & (~on1) & (~unk0);
set_off |= (~on1) & (~on0) & (~unk0);
{ uint64_t temp = stateon & (~on2) & (~on0) & (~unk1) & unk0; signal_off |= temp; signal_on |= temp; abort |= temp; }
{ uint64_t temp = stateon & (~on2) & on1 & on0; signal_off |= temp; abort |= temp; }
signal_off |= (~unk1) & (~unk0);
abort |= (~on1) & (~on0);
{ uint64_t temp = (~stateon) & (~on0); set_on |= temp; abort |= temp; }
{ uint64_t temp = (~stateon) & (~on1) & (~unk1); set_off |= temp; set_on |= temp; signal_off |= temp; abort |= temp; }
{ uint64_t temp = (~on2) & unk1; set_on |= temp; signal_off |= temp; abort |= temp; }
{ uint64_t temp = (~stateon) & on2; set_off |= temp; set_on |= temp; signal_off |= temp; abort |= temp; }
{ uint64_t temp = stateunk; signal_off |= temp; abort |= temp; }
set_on = ~set_on;
signal_off = ~signal_off;
abort = ~abort;
