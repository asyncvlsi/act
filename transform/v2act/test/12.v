module \bslk1$ (x, y);
  input x;
  output y;

  assign y = x;
endmodule

module blk0(e0, out);
  input [4:0] e0;
  output [4:0] out;
  wire f;
  assign out[0] = 1'b0;
  always @(posedge out[2]) begin
     a <= b;
     c <= d;
  end
  initial begin
     a <= 1'b0;
     c <= 1'b1;
  end
  \bslk1$  q(f,out[1]);
endmodule
