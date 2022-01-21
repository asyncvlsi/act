module blk1(a,b,out);
  output [2:0] out;
  input [1:0] a;
  input b;
  
 assign out = {a[1:0],b};
endmodule
