`timescale 1ns/10ps

`default_nettype none

module floppy_testbench;


reg clk=0;
reg reset = 0;
always #5 clk <= ~clk;

initial begin
    $dumpvars(0, floppy_testbench);

    #5 reset <= 1;
    #20 reset <= 0;

    #1000 $stop;
end

wire    [15:0]  addr;
wire    [7:0]   idata = 0;
wire    [7:0]   odata;
wire            memwr, sd_dat, sd_dat3, sd_cmd, sd_clk, uart_txd;

assign sd_dat = 0;

// hostias
reg     [2:0]   hostio_addr = 0;
reg     [7:0]   hostio_idata = 0;
wire    [7:0]   hostio_odata;
reg             hostio_rd = 0;
reg             hostio_wr = 0;

wire [22:0] floppy_sdram_addr;
wire [7:0]  floppy_sdram_do;
wire [7:0]  floppy_sdram_di;
wire        floppy_sdram_read;
wire        floppy_sdram_write;
wire        floppy_sdram_busy;

wire    [5:0]   keyboard_keys;

floppy floppy0(
    .clk(clk),
    .ce(1'b1),
    .reset_n(~reset),
    .addr(addr),
    .idata(idata),
    .odata(odata),
    .memwr(memwr),

    .sd_dat(sd_dat),        // sd card signals
    .sd_dat3(sd_dat3),      // sd card signals
    .sd_cmd(sd_cmd),        // sd card signals
    .sd_clk(sd_clk),        // sd card signals
    .uart_txd(uart_txd),    // uart tx pin
    
    // I/O interface to host system (Vector-06C)
    .hostio_addr(hostio_addr),
    .hostio_idata(hostio_idata),
    .hostio_odata(hostio_odata),
    .hostio_rd(hostio_rd),
    .hostio_wr(hostio_wr),

    // path to SDRAM
    .sdram_addr(floppy_sdram_addr),
    .sdram_data_o(floppy_sdram_do),
    .sdram_data_i(floppy_sdram_di),
    .sdram_read(floppy_sdram_read),
    .sdram_write(floppy_sdram_write),
    .sdram_busy(floppy_sdram_busy),
    
    // keyboard interface
    .keyboard_keys(keyboard_keys)// {reserved,left,right,up,down,enter}
);

reg [3:0] clk_counter = 0;
always @(posedge clk) clk_counter <= clk_counter + 1;
wire access_slot = clk_counter[3:0] == 4'b1111;

wire [22:0] ramc_addr;
wire [15:0] ramc_data_r;
wire [15:0] ramc_data_w;
wire        ramc_read;
wire        ramc_write;
wire        ramc_lb, ramc_ub;
wire        ramc_refresh;
wire        ramc_busy;

sdram_arbitre arbitre0(
    .clk(clk), .reset(reset),

    .access_slot(access_slot),

    .vu_adrs(18'h0),
    .vu_data_i(8'h0),
    .vu_write(1'b0),
    .vu_read(1'b0),

    .disk_adrs(floppy_sdram_addr),
    .disk_data_i(floppy_sdram_do),
    .disk_data_o(floppy_sdram_di),
    .disk_write(floppy_sdram_write),
    .disk_read(floppy_sdram_read),
    .disk_halfword(1'b0),
    .disk_byte(1'b1),
    .disk_ram_busy(floppy_sdram_busy),

    .sdram_addr(ramc_addr),
    .sdram_di(ramc_data_r),
    .sdram_do(ramc_data_w),
    .sdram_read(ramc_read),
    .sdram_write(ramc_write),
    .sdram_lb(ramc_lb),
    .sdram_ub(ramc_ub),
    .sdram_refresh(ramc_refresh),

    .sdram_busy(ramc_busy));


wire [15:0] DRAM_DQ;
wire [11:0] DRAM_ADDR;
wire DRAM_LDQM;
wire DRAM_UDQM;
wire DRAM_WE_N;
wire DRAM_CAS_N;
wire DRAM_RAS_N;
wire DRAM_CS_N;
wire DRAM_BA_0;
wire DRAM_BA_1;

assign DRAM_DQ = {floppy_sdram_addr[7:0],floppy_sdram_addr[7:0]};

SDRAM_Controller ramc(
    .clk(clk),                    	//  SDRAM clock
    .reset(reset),                      //  System reset
    .DRAM_DQ(DRAM_DQ),                  //  SDRAM Data bus 16 Bits
    .DRAM_ADDR(DRAM_ADDR),              //  SDRAM Address bus 12 Bits
    .DRAM_LDQM(DRAM_LDQM),              //  SDRAM Low-byte Data Mask 
    .DRAM_UDQM(DRAM_UDQM),              //  SDRAM High-byte Data Mask
    .DRAM_WE_N(DRAM_WE_N),              //  SDRAM Write Enable
    .DRAM_CAS_N(DRAM_CAS_N),            //  SDRAM Column Address Strobe
    .DRAM_RAS_N(DRAM_RAS_N),            //  SDRAM Row Address Strobe
    .DRAM_CS_N(DRAM_CS_N),              //  SDRAM Chip Select
    .DRAM_BA_0(DRAM_BA_0),              //  SDRAM Bank Address 0
    .DRAM_BA_1(DRAM_BA_1),              //  SDRAM Bank Address 1
    
    .iaddr(ramc_addr),			// address from cpu [21:0]
    .dataw(ramc_data_w),	        // data from cpu [15:0]
    .rd(ramc_read),			// read request
    .we_n(~ramc_write), 		// write request neg
    .ilb_n(~ramc_lb),			// lower byte mask
    .iub_n(~ramc_ub),			// upper byte mask
    .datar(ramc_data_r),		// data from sdram [15:0]
    .membusy(ramc_busy),		// sdram busy flag
    .refresh(ramc_refresh)
);

endmodule
