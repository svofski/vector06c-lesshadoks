// ====================================================================
// VECTOR-06C FPGA REPLICA Copyright (C) 2007 Viacheslav Slavinsky
// LES SHADOKS POMPAIENT Copyright (C) 2018 Viacheslav Slavinsky
//
// This core is distributed under modified BSD license. 
// For complete licensing information see LICENSE.TXT.
// -------------------------------------------------------------------- 
//
// An open implementation of Vector-06C home computer
//
// Author: Viacheslav Slavinsky, http://sensi.org/~svo
// 
// Barkar kvaz support added by Ivan Gorodetsky
//
// Design File: kvaz.v
//
// RAM disk memory mapper. This unit maps standard Vector-06C RAM disk
// into pages 1, 2, 3, 4 of SDRAM address space. 
//
// --------------------------------------------------------------------
//`default_nettype none

module kvaz(
	input clk, 
	input clke, 
	input reset,
	input [7:0] shavv,
	input [15:0] address, 
	input select,
	input [7:0] data_in,
	input stack, 
	input boot,
	input memwr, 
	input memrd,
	input iord,
	input iowr,
	output reg[2:0] bigram_addr,
	output blk_n,
	output [7:0] debug);
			

// control register
reg [7:0]		control_reg;
assign debug = {stack_sel, ram_sel};

always @(posedge clk) begin
	if (reset) begin
		control_reg <= 0;
	end 
	else if (clke & select) begin
		control_reg <= data_in;
	end
end

// control register breakdown
wire [2:0]	cr_ram_page 	= control_reg[1:0];
wire [2:0]	cr_stack_page 	= control_reg[3:2];
wire	   	cr_stack_on	= control_reg[4];
wire		cr_ram_on	= control_reg[5];

wire [3:0] adsel = shavv[7:4];

wire boot_not = memwr | iord | iowr;
wire boot_sel = boot & ~shavv[7] & ~boot_not; // overlay bootrom

wire addr_sel = adsel == 4'hA | adsel == 4'hB | adsel == 4'hC | adsel == 4'hD | 	// standard KVAZ
	((adsel==4'h8)|(adsel==4'h9)&&(control_reg[6]==1))|((adsel==4'hE)|(adsel==4'hF)&&(control_reg[7]==1)); //Barkar

wire ram_sel = cr_ram_on & addr_sel;

wire stack_sel = cr_stack_on & stack;

assign blk_n = ~((cr_ram_on & addr_sel) | (cr_stack_on & stack) | boot_sel);

always @* begin
	bigram_addr <= stack_sel ? cr_stack_page : ram_sel ? cr_ram_page : boot_sel ? 3'b000 : 3'b000; // todo: boot_sel -> 3'b100
end	

endmodule
