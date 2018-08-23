// ====================================================================
// LES SHADOKS POMPAIENT: Vector-06c Expansion Board
//
// Copyright (C) 2018, Viacheslav Slavinsky
//
// This core is distributed under modified BSD license. 
// For complete licensing information see LICENSE.TXT.
// -------------------------------------------------------------------- 

`default_nettype none

module soundinterfaces(input clk, input reset,
	input [7:0] shavv, input [7:0] data, 
	input negedge_zpvv_n, input negedge_chtvv_n,
	output [7:0] data_o, output audio_l, output audio_r);


////////
// AY //
////////
wire [7:0]  ay_soundA,ay_soundB,ay_soundC;
wire [7:0]  rs_soundA,rs_soundB,rs_soundC;

`ifdef WITH_AY
//wire 	ay = (VU_SHAVV_N == ~8'h10) && negedge_zpvv_n;

wire        ay_sel = shavv[7:2] == 'b101 && shavv[1] == 0; // only ports $14 and $15
wire        ay_wren = ay_sel & negedge_zpvv_n;
wire        ay_rden = ay_sel & negedge_chtvv_n;

reg [3:0] aycectr;
always @(posedge clk) if (aycectr<14) aycectr <= aycectr + 1'd1; else aycectr <= 0;

ayglue shrieker(
                .clk(clk),
                .ce(aycectr == 0),
                .reset_n(~reset), 
                .address(shavv[0]),
                .data(data), 
                .q(data_o),
                .wren(ay_wren),
                .rden(ay_rden),
                .soundA(ay_soundA),
                .soundB(ay_soundB),
                .soundC(ay_soundC)
                );              
`else
assign ay_soundA=8'b0;
assign ay_soundB=8'b0;
assign ay_soundC=8'b0;
assign ay_rden = 1'b0;
assign data_o = 8'hFF;
`endif

`ifdef WITH_RSOUND

ym2149 rsound(
    .DI(RSdataIn),
    .DO(RSdataOut),
    .BDIR(RSctrl[2]), 
    .BC(RSctrl[1]),
    .OUT_A(rs_soundA),
    .OUT_B(rs_soundB),
    .OUT_C(rs_soundC),
    .CS(1'b1),
    .ENA(aycectr == 0),
    .RESET(RSctrl[3]|sys_reset),
    .CLK(clk24)
    );                
`else
assign rs_soundA=8'b0;
assign rs_soundB=8'b0;
assign rs_soundC=8'b0;
`endif


////////////////////////////////
// 580VI53 timer: ports 08-0B //
////////////////////////////////
wire            vi53_sel = shavv[7:2] == 6'b000010;
wire            vi53_wren = vi53_sel & negedge_zpvv_n;
wire            vi53_rden = 1'b0;

wire    [2:0]   vi53_out;
wire    [7:0]   vi53_odata;

reg [3:0] vi53cectr;
always @(posedge clk)
	vi53cectr <= vi53cectr + 1'b1;

wire vi53_timer_ce = &vi53cectr;

`ifdef WITH_VI53
pit8253 vi53(
            clk, 
            1'b1, 
            vi53_timer_ce, 
            {~shavv[1],~shavv[0]}, 
            vi53_wren, 
            vi53_rden, 
            data,
            vi53_odata, 
            3'b111, 
            vi53_out);
`endif



wire [7:0] CovoxData = 0;
wire tape_input = 0;
wire audio_pwm;

soundcodec soundnik(
                    .clk24(clk),
                    .pulses({/*vv55int_pc_out[0]*/1'b0,vi53_out}), 
                    .ay_soundA(ay_soundA),  //
                    .ay_soundB(ay_soundB),  //
                    .ay_soundC(ay_soundC),  //
                    .rs_soundA(rs_soundA),  //
                    .rs_soundB(rs_soundB),  //
                    .rs_soundC(rs_soundC),  //
                    .covox(CovoxData),
                    .tapein(tape_input), 
                    .reset_n(~reset),
                    .o_pwm(audio_pwm)
                   );
		   
assign audio_l = audio_pwm;
assign audio_r = audio_pwm;		   

endmodule
		   
		   