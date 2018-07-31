`default_nettype none
// ====================================================================
//                         VECTOR-06C FPGA REPLICA
//
// Copyright (C) 2007, Viacheslav Slavinsky
//
// This core is distributed under modified BSD license. 
// For complete licensing information see LICENSE.TXT.
// -------------------------------------------------------------------- 
//
// An open implementation of Vector-06C home computer
//
// Author: Viacheslav Slavinsky, http://sensi.org/~svo
// 
// Design File: timer100hz.v
//
// A simple tick-tock timer with async load. 
// Used as a peripheral for the floppy CPU.
//
// --------------------------------------------------------------------

module timer100hz(clk, reset, di, wren, q);
parameter MCLKFREQ = 24000000;

input 			clk;
input           reset;
input [7:0]		di;
input			wren;
output reg[7:0]	q;

reg [17:0] timerctr;

wire hz100 = timerctr == 0;

always @(posedge clk)
    if (reset)
        timerctr <= 0;
    else if (timerctr == 0) 
`ifdef SIMULATION
        timerctr <= 4;
`else
        timerctr <= MCLKFREQ/100;
`endif
    else
        timerctr <= timerctr - 1'b1;


always @(posedge clk)
    if (reset) 
        q <= 0;
    else if (wren) begin
        q <= di;
    end 
    else if (q != 0 && hz100) q <= q - 1'b1;

endmodule

