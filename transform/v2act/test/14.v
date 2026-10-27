module testbuf(in,out);
  input in;
  output out;
  wire p;
  inv1a1 i0(.A(in), .Y(p));
  buf1a3 b(.A(in), .Y(out));
  buf1a3 b2(.A(in), .Y(p));
  inv1a1 i2(.A(in), .Y(out));
  buf1a15 b8(.A(in), .Y(out));
endmodule
