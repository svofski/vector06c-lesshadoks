// ====================================================================
// LES SHADOKS POMPAIENT: Vector-06c Expansion Board
//
// Copyright (C) 2018, Viacheslav Slavinsky
//
// This core is distributed under modified BSD license. 
// For complete licensing information see LICENSE.TXT.
// -------------------------------------------------------------------- 


// This module debounces an input signal using a shift register memory.
// Sample length can be specified as a parameter.
module purifier #(parameter MEMORY=8)(input clk, input reset, input in, 
	output reg out);

reg [MEMORY-1:0] memory;

wire all_hi = &memory;
wire all_lo = ~|memory;
wire stable = all_hi | all_lo;

reg stable_r;

parameter S1 = 1, S2 = 2;
reg [1:0] state;

always @(posedge clk)
begin
	memory <= {memory[MEMORY-2:0],in};
	stable_r <= stable;
end


always @(posedge clk) begin
	if (reset) begin
		state <= S1;
		out <= in;
	end
	else
	case (state)
	S1:	if (stable_r & !stable) begin	// value begins to change, set new output value
			out <= in;
			state <= S2;
		end
	S2:	if (stable) begin
			out <= in;
			state <= S1;	// once stabilised over all period of memory, return to idle state
		end
	endcase
end

endmodule

		
module bus_sampler(
	input clk, 
	input reset, 

	input VU_RAS_N,
	input VU_CAS_N,
	input VU_ZPZU_N,
	input VU_CHTZU_N,
	input VU_ZPVV_N,
	input VU_CHTVV_N,
	input VU_STACK,
	input VU_STROB_SOST,
	input [7:0] VU_SHAVV_N,
	
	output negedge_ras_n,
	output negedge_cas_n,
	output negedge_zpzu_n,
	output negedge_chtzu_n,
	output negedge_chtvv_n,
	output negedge_zpvv_n,
	
	output posedge_ras_n,
	output posedge_stack,
	output posedge_strob_sost,
	
	output clean_ras_n,
	output clean_cas_n,
	output clean_stack,
	output clean_chtzu_n,
	output clean_chtvv_n,
	output clean_strob_sost,
	output reg [7:0] clean_shavv);

reg samp_zpzu_n;		
reg samp_chtzu_n;
reg samp_chtvv_n;		
reg samp_zpvv_n;
reg samp_cas_n;
reg samp_stack;
reg samp_strob_sost;
reg samp_ras_n;

wire ccas_n;
wire cras_n;				
wire czpzu_n;
wire c_chtzu_n;
wire czpvv_n;
wire cchtvv_n;
wire rstack;
wire rstrob_sost;

purifier #(.MEMORY(4)) ras_purifier(.clk(clk), .reset(reset), .in(VU_RAS_N), .out(cras_n));				
purifier cas_purifier(.clk(clk), .reset(reset), .in(VU_CAS_N), .out(ccas_n));
purifier zpzu_purifier(.clk(clk), .reset(reset), .in(VU_ZPZU_N), .out(czpzu_n));
purifier chtzu_purifier(.clk(clk), .reset(reset), .in(VU_CHTZU_N), .out(c_chtzu_n));
purifier zpvv_purifier(.clk(clk), .reset(reset), .in(VU_ZPVV_N), .out(czpvv_n));
purifier chtvv_purifier(.clk(clk), .reset(reset), .in(VU_CHTVV_N), .out(cchtvv_n));
purifier stack_purifier(.clk(clk), .reset(reset), .in(VU_STACK), .out(rstack));
purifier strob_purifier(.clk(clk), .reset(reset), .in(VU_STROB_SOST), .out(rstrob_sost));

/* sample control signals */
always @(posedge clk) begin: _vu_sampler
	samp_zpzu_n <= czpzu_n;
	samp_stack <= rstack;
	samp_strob_sost <= rstrob_sost;

	samp_ras_n <= cras_n;
	samp_cas_n <= ccas_n;
	samp_chtzu_n <= c_chtzu_n;
	samp_zpvv_n <= czpvv_n;
	samp_chtvv_n <= cchtvv_n;
	
	clean_shavv <= ~VU_SHAVV_N;	
end


assign negedge_zpzu_n = {samp_zpzu_n,czpzu_n} == 2'b10;
assign negedge_cas_n = samp_cas_n & ~ccas_n;
assign posedge_stack = {samp_stack,rstack} == 2'b01;
assign posedge_strob_sost = {samp_strob_sost,rstrob_sost} == 2'b01;
assign negedge_ras_n = {samp_ras_n,cras_n} == 2'b10;
assign posedge_ras_n = {samp_ras_n,cras_n} == 2'b01;
assign negedge_chtzu_n = samp_chtzu_n & ~c_chtzu_n;
assign negedge_zpvv_n = {samp_zpvv_n,czpvv_n} == 2'b10;
assign negedge_chtvv_n = {samp_chtvv_n,cchtvv_n} == 2'b10;

assign clean_cas_n = ccas_n;
assign clean_ras_n = cras_n;
assign clean_stack = rstack;
assign clean_chtzu_n = c_chtzu_n;
assign clean_chtvv_n = cchtvv_n;
				
endmodule


// Garbled RA/CA to normal address decoder
// strobed RA/CA addresses are arranged like so:
// RA: 8,9,10,11,12,0,1,~13
//     0 1  2  3  4 5 6   7

// CA: 2,3,4,5,6,7,15,~14
//     0 1 2 3 4 5  6   7

module shap_decoder(input clk, input reset, input [7:0] VU_SHAP_N, 
	input negedge_ras_n, input negedge_cas_n, input clean_cas_n,
	input clean_ras_n, output reg [15:0] decoded_a, output reg valid);
	
reg [7:0]	ra_z1;
reg [7:0]	ra_z2;
reg [7:0]	ra_z3;
reg [7:0]	ra_z4;

reg [7:0]	ra;
reg [7:0]	ca;

always @(posedge clk) begin
	ra_z1 <= VU_SHAP_N;
	ra_z3 <= ra_z1;
end

always @(negedge clk) begin
	ra_z2 <= VU_SHAP_N;
	ra_z4 <= ra_z2;
end

reg valid_delay;

always @(negedge clk) begin
	valid_delay <= negedge_cas_n ? 1'b1 : 
		(clean_cas_n & clean_ras_n) ? 1'b0 : valid_delay;
	valid <= valid_delay;
	
	if (negedge_ras_n)
		ra <= ra_z3;
	if (negedge_cas_n)
		ca <= ra_z4;
end

always decoded_a <= {ca[6], ~ca[7], ~ra[7], ra[4], ra[3], ra[2], ra[1],ra[0],
		     ca[5],  ca[4],  ca[3], ca[2], ca[1], ca[0], ra[6],ra[5]};		

endmodule


`ifdef FALSE
	
/* семплировать всё так почти работает, но обламывается, 
   например, перед последней частью NONAME DEMO */
module async_sampler #(parameter WIDTH=1)(input clk, input reset, 
	input [WIDTH-1:0] in, 
	output [WIDTH-1:0] neg_edge, 
	output [WIDTH-1:0] pos_edge, 
	output [WIDTH-1:0] registered);

reg [WIDTH-1:0] memory [0:1];

integer i;

always @(posedge clk)
	if (reset)
		for (i = 0; i < 2; i = i + 1) memory[i] <= 0;
	else begin
		memory[1] <= memory[0];
		memory[0] <= in;
	end
	
assign neg_edge = memory[1] & ~memory[0];
assign pos_edge = ~memory[1] & memory[0];
assign registered = memory[0];

endmodule

`endif	
	