module blk0(e0, out);
  input [4:0] e0;
  output [4:0] out;
  wire f;
  assign out[4:0] = {e0[2:0],e0[3],e0[4]};
endmodule
