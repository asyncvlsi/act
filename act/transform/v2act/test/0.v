
module b01 ( line1, line2, reset, outp, overflw, clock );
  input line1, line2, reset, clock;
  output outp, overflw;
  wire   N12, N14, N17, N21, N24, N26, N29, N32, N34, N36, N38, N40, N41, N42,
         N43, N44, N45, N48, N49, N50, N51, N52, N53, N54, N55, N56, N57, N58,
         N59, N60, N61, N62, N63, N64, N65, N68, n6, n8, n9, n10, n11, n12,
         n13, n14, n15, n16, n17, n18, n19, n20, n21, n22, n23, n24, n25, n26,
         n27, n28, n29, n30, n31, n32, n33, n34, n35;
  wire   [2:0] stato;

  or2a3 C186 ( .A(line1), .B(line2), .Y(N38) );
  and2a6 C184 ( .A(line1), .B(line2), .Y(N36) );
  or2a3 C181 ( .A(line1), .B(line2), .Y(N34) );
  or2a3 C178 ( .A(line1), .B(line2), .Y(N32) );
  and2a6 C176 ( .A(line1), .B(line2), .Y(N29) );
  and2a6 C174 ( .A(line1), .B(line2), .Y(N26) );
  and2a6 C172 ( .A(line1), .B(line2), .Y(N24) );
  and2a6 C170 ( .A(line1), .B(line2), .Y(N21) );
  or2a3 C166 ( .A(N48), .B(N49), .Y(N64) );
  or2a3 C165 ( .A(N64), .B(N51), .Y(N65) );
  or2a3 C161 ( .A(N48), .B(N49), .Y(N62) );
  or2a3 C160 ( .A(N62), .B(stato[0]), .Y(N63) );
  or2a3 C156 ( .A(N48), .B(stato[1]), .Y(N60) );
  or2a3 C155 ( .A(N60), .B(N51), .Y(N61) );
  inv1a9 I_11 ( .A(N61), .Y(N17) );
  or2a3 C152 ( .A(N48), .B(stato[1]), .Y(N58) );
  or2a3 C151 ( .A(N58), .B(stato[0]), .Y(N59) );
  or2a3 C147 ( .A(stato[2]), .B(N49), .Y(N56) );
  or2a3 C146 ( .A(N56), .B(N51), .Y(N57) );
  or2a3 C143 ( .A(stato[2]), .B(N49), .Y(N54) );
  or2a3 C142 ( .A(N54), .B(stato[0]), .Y(N55) );
  inv1a9 I_8 ( .A(N55), .Y(N14) );
  or2a3 C138 ( .A(N52), .B(N51), .Y(N53) );
  and2a6 C133 ( .A(N48), .B(N49), .Y(N50) );
  and2a6 C132 ( .A(N50), .B(N51), .Y(N12) );
  or2a3 C115 ( .A(N14), .B(N17), .Y(N45) );
  fdef2a1 overflw_reg ( .D(N44), .E(1), .CLK(clock), .CLR(n6), .Q(overflw)
         );
  fdef2a1 outp_reg ( .D(N43), .E(1), .CLK(clock), .CLR(n6), .Q(outp) );
  inv1a3 U7 ( .A(reset), .Y(n6) );
  ao1f1 U9 ( .A(n8), .B(n9), .C(n10), .Y(N43) );
  ao1f1 U10 ( .A(n11), .B(n12), .C(n9), .Y(n10) );
  or2a1 U11 ( .A(n13), .B(n14), .Y(n12) );
  inv1a1 U12 ( .A(N68), .Y(n9) );
  and3a1 U13 ( .A(N63), .B(N55), .C(n15), .Y(n8) );
  and3a1 U14 ( .A(N57), .B(n16), .C(N53), .Y(n15) );
  or3a1 U15 ( .A(n17), .B(n18), .C(n19), .Y(N42) );
  ao1f1 U16 ( .A(n16), .B(n20), .C(n21), .Y(n19) );
  inv1a1 U17 ( .A(N12), .Y(n16) );
  and2a3 U18 ( .A(N29), .B(N44), .Y(n17) );
  inv1a1 U19 ( .A(N57), .Y(N44) );
  or3a1 U20 ( .A(n22), .B(n23), .C(n24), .Y(N41) );
  ao1f1 U21 ( .A(N53), .B(N24), .C(n21), .Y(n24) );
  inv1a1 U22 ( .A(N45), .Y(n21) );
  and2a3 U23 ( .A(n25), .B(n13), .Y(n22) );
  inv1a1 U24 ( .A(N59), .Y(n13) );
  or4a3 U25 ( .A(n26), .B(n18), .C(n23), .D(n27), .Y(N40) );
  ao1f1 U26 ( .A(N57), .B(N29), .C(n28), .Y(n27) );
  oa1f1 U27 ( .A(N26), .B(n29), .C(n30), .Y(n28) );
  and2a3 U28 ( .A(N12), .B(n20), .Y(n30) );
  inv1a1 U29 ( .A(N21), .Y(n20) );
  inv1a1 U30 ( .A(N55), .Y(n29) );
  ao1a1 U31 ( .A(N38), .B(n11), .C(n31), .Y(n23) );
  and2a3 U32 ( .A(N36), .B(n32), .Y(n31) );
  inv1a1 U33 ( .A(N63), .Y(n32) );
  inv1a1 U34 ( .A(N65), .Y(n11) );
  ao1f1 U35 ( .A(N59), .B(n25), .C(n33), .Y(n18) );
  or2a1 U36 ( .A(N53), .B(n34), .Y(n33) );
  inv1a1 U37 ( .A(N24), .Y(n34) );
  inv1a1 U38 ( .A(N32), .Y(n25) );
  and2a3 U39 ( .A(N34), .B(n14), .Y(n26) );
  inv1a1 U40 ( .A(N61), .Y(n14) );
  xor2a6 U41 ( .A(line2), .B(line1), .Y(n35) );
  buf1a15 U42 ( .A(n35), .Y(N68) );
  fdef2a6 \stato_reg[0]  ( .D(N40), .E(1), .CLK(clock), .CLR(n6), .Q(
        stato[0]) );
  fdef2a6 \stato_reg[2]  ( .D(N42), .E(1), .CLK(clock), .CLR(n6), .Q(
        stato[2]) );
  fdef2a6 \stato_reg[1]  ( .D(N41), .E(1), .CLK(clock), .CLR(n6), .Q(
        stato[1]) );
  inv1a3 U43 ( .A(stato[1]), .Y(N49) );
  inv1a3 U44 ( .A(stato[2]), .Y(N48) );
  or2a1 U45 ( .A(stato[2]), .B(stato[1]), .Y(N52) );
  inv1a3 U46 ( .A(stato[0]), .Y(N51) );
endmodule

