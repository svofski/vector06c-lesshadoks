`timescale 1ns/10ps

module purifier_testbench;

reg clk = 0;
reg reset = 0;
reg dirty = 0;
wire clean;

initial begin
  
  #1  reset <= 1;
      dirty <= 1;
  #10 reset <= 0;

  #100 dirty <= 0;
  #5 dirty <= 1;
  #5 dirty <= 0;
  #5 dirty <= 0;
  #5 dirty <= 1;
  #5 dirty <= 1; 	// return to 1 early
  
  #100 dirty <= 1;	
  #5 dirty <= 0;
  #5 dirty <= 1;
  #5 dirty <= 0;
  #100 dirty <= 1; 	// return to 1 in due time
  
  
  
  
  #1000 $stop;
end

always #5 clk <= ~clk;



purifier pu(.clk(clk), .reset(reset), .in(dirty), .out(clean));

endmodule
