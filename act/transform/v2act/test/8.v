module blk1(a,b,out);
  output [2:0] out;
  input [1:0] a;
  input b;
  
 assign {out[2], out[1], out[0]} = {a[1:0],b};
endmodule
