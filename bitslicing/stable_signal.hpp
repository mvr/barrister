signaloff |= (~d1) & (~s2) & (~s1) & (~s0);
signaloff |= (~d2) & (~s2) & (~s1) & m1 & m0;
signaloff |= (~d2) & (~s2) & (~s1) & m2;
signaloff |= (~d2) & (~s2) & (~s1) & m3;
signaloff |= (~d4) & (~s2) & m2 & m0;
signaloff |= (~d4) & (~s2) & m2 & m1;
signaloff |= (~d4) & (~s2) & m3;
signaloff |= (~d5) & (~s2) & m2 & m1;
signaloff |= (~d5) & (~s2) & m3;
signaloff |= (~d5) & s2 & (~s1) & (~s0);
signaloff |= (~d6) & (~s1) & m2 & m1 & m0;
signaloff |= (~d6) & (~s1) & m3;
signaloff |= (~d6) & (~s2) & m2 & m1 & m0;
signaloff |= (~d6) & (~s2) & m3;
signaloff |= (~l2) & (~m2) & m1 & m0;
signaloff |= (~l2) & (~s2) & (~s1) & (~m1);
signaloff |= (~l2) & l3 & d4 & d5 & (~s1) & m2;
signaloff |= (~l3) & (~s2) & (~s1) & (~m1);
signaloff |= (~l3) & (~s2) & s1 & (~s0);
signaloff |= (~stateon) & (~l3) & d4 & d5 & (~s1) & m2;
signaloff |= (~stateunk) & (~d2) & (~s2) & (~s1) & m1;
signaloff |= (~stateunk) & (~d4) & (~s2) & m2;
signaloff |= (~stateunk) & (~d5) & m2 & (~m1) & m0;
signaloff |= (~stateunk) & (~d6) & m2 & m1 & (~m0);
signaloff |= (~stateunk) & d2 & (~s2) & s1 & (~s0);
signaloff |= stateon & (~l3) & (~s2);
signaloff = ~signaloff;
signalon |= (~d0) & (~s2) & (~s1) & (~s0);
signalon |= (~d1) & (~m2) & m1 & m0;
signalon |= (~d1) & (~s2) & (~s1) & m2;
signalon |= (~d2) & (~s2) & (~s1) & m2;
signalon |= (~d2) & s1 & (~s0);
signalon |= (~d4) & (~s0) & m2 & m1;
signalon |= (~d4) & (~s2) & m2 & m1;
signalon |= (~d5) & m2 & m1 & m0;
signalon |= (~l2) & (~s2) & (~s1) & m2;
signalon |= (~l2) & d4 & d5 & d6 & m2;
signalon |= (~l2) & s1 & (~s0) & m2;
signalon |= (~l3) & (~m1) & m0;
signalon |= (~l3) & (~s2) & m2 & m1;
signalon |= (~l3) & d5 & m2 & m1 & (~m0);
signalon |= (~stateunk) & (~d1) & (~m2) & m1;
signalon |= (~stateunk) & (~d4) & m2 & m0;
signalon |= (~stateunk) & (~d5) & m2 & m1;
signalon |= (~stateunk) & l2 & m1 & m0;
signalon |= m3;
signalon = ~signalon;
centeron |= (~d1) & (~s1);
centeron |= (~d2) & (~s1);
centeron |= (~d2) & (~s0);
centeron |= (~d0) & (~s1) & (~s0);
centeron |= (~d5) & m2 & m1;
centeron |= (~d6) & m2 & m1 & m0;
centeron |= (~d4) & m2 & m1;
centeron |= (~d4) & (~m2) & (~m1);
centeron |= (~d5) & (~m2) & (~m1);
centeron |= (~d6) & (~m2) & (~m1);
centeron |= (~d4) & (~m1) & m0;
centeron = ~centeron;
centeroff |= s2;
centeroff |= l2 & l3;
centeroff |= l3 & s1 & s0;
centeroff |= (~m3) & (~m2) & (~m1);
centeroff |= l2 & (~m2) & m1;
centeroff |= (~m2) & m1 & (~m0);
