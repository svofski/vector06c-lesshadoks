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
// Design File: spi.v
//
// SPI host, mimics AVR SPI in its most basic mode
//
// --------------------------------------------------------------------

module spi(
    input               clk,
    input               ce,
    input               reset_n,
    output  reg         mosi,
    input               miso,
    output              sck,

    input       [7:0]   di,
    input               wr,

    output      [7:0]   do,
    output  reg         dsr,

    input               slow);

assign sck = slow ? slow_sck : ~clk & scken;
assign do = slow ? slow_shiftreg : shiftreg;

reg [7:0]   shiftreg;
reg [7:0]   shiftski;

reg [1:0]   state = 0;
reg         scken = 0;

wire ce_int = slow ? slow_ce : ce;

reg [1:0] slow_cnt;
reg slow_sck_r;

wire slow_ce = &slow_cnt;
wire cock_ce = slow_cnt == 2'b10;
wire slow_sck_ce = slow_cnt[0];
wire slow_sck = slow_sck_r;

always @(posedge clk) begin: _could_mean_anything
    if (!reset_n) begin
        slow_cnt <= 0;
        slow_sck_r <= 1'b0;
    end
    else begin
        slow_cnt <= slow_cnt + 1'b1;
    end

    if (scken && slow_sck_ce) slow_sck_r <= ~slow_sck_r;
end

reg samp_sck;
reg samp_miso;
reg [7:0] slow_shiftreg;
always @(posedge clk or negedge reset_n) begin
    if (!reset_n) begin
        samp_sck <= sck;
        samp_miso <= miso;
        slow_shiftreg <= 0;
    end    
    else begin
        samp_sck <= sck;
        samp_miso <= miso;
        if (~samp_sck & sck) begin
            $display("fuck: ", miso);
            slow_shiftreg <= {slow_shiftreg[6:0],samp_miso};
        end
    end
end

always @(posedge clk or negedge reset_n) begin
    if (!reset_n) begin
        state <= 0;
        mosi <= 1'b0;
        dsr <= 0;
        scken <= 0;
    end else begin
        case (state)
        0:  if (ce) begin
                if (wr) begin
                    dsr <= 1'b0;
                    state <= 1;
                    shiftreg <= di;
                    shiftski <= 8'b11111111;
                end
            end
        1:  if (ce_int) begin
                scken <= 1;
                mosi <= shiftreg[7];
                shiftreg <= {shiftreg[6:0],miso};
                shiftski <= {1'b0,shiftski[7:1]};
                
                if (|shiftski == 0) begin 
                    state <= 2;
                    scken <= 0;
                end
            end
        2:  if (ce_int) begin
                mosi <= 1'b0; // shouldn't be necessary but a nice debug view
                dsr <= 1'b1;
                state <= 0;
                //scken <= 0;
            end
        default: ;
        endcase
    end
end


endmodule

