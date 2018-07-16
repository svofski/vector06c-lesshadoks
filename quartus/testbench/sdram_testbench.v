`timescale 1ns/10ps

module sdram_testbench(
output [15:0] DRAM_DQ,
output [11:0] DRAM_ADDR,
output DRAM_LDQM,
output DRAM_UDQM,
output DRAM_WE_N,
output DRAM_CAS_N,
output DRAM_RAS_N,
output DRAM_CS_N,
output DRAM_BA_0,
output DRAM_BA_1

);



reg clk = 0;
reg reset = 0;
reg [21:0] sdram_addr = 0;
reg [15:0] data_to_sdram = 0;

reg sdram_read = 0;
reg sdram_write = 0;
reg sdram_pls = 0;

initial begin
  
  #1  reset <= 1;
  #10 reset <= 0;

  #100 sdram_pls <= 1;
  #5 sdram_pls <= 0;
  
  
  
  
  #1000 $stop;
end

always #5 clk <= ~clk;


assign DRAM_CLK=clk;              //  SDRAM Clock
assign DRAM_CKE=1;                      //  SDRAM Clock Enable

wire sdram_lb = 1;
wire sdram_ub = 1;
wire [15:0] sdram_do;
wire sdram_busy;



SDRAM_Controller ramd(
    .clk(clk),                    	//  SDRAM clock
    .reset(sys_reset),                  //  System reset
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
    
    .iaddr(sdram_addr),			// address from cpu [21:0]
    .dataw(data_to_sdram),		// data from cpu [15:0]
    .rd(sdram_read),			// read request
    .we_n(~sdram_write), 		// write request neg
    .ilb_n(~sdram_lb),			// lower byte mask
    .iub_n(~sdram_ub),			// upper byte mask
    .datar(sdram_do),			// data from sdram [15:0]
    .membusy(sdram_busy),		// sdram busy flag
    .refresh(sdram_pls)
);


purifier pu(.clk(clk), .reset(sys_reset), .in(dirty), .out(clean));

endmodule
