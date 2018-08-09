`default_nettype none

// ====================================================================
//                         VECTOR-06C FPGA REPLICA
//
//                Copyright (C) 2007,2008 Viacheslav Slavinsky
//
// This core is distributed under modified BSD license. 
// For complete licensing information see LICENSE.TXT.
// -------------------------------------------------------------------- 
//
// An open implementation of Vector-06C home computer
//
// Author: Viacheslav Slavinsky, http://sensi.org/~svo
// 
// Design File: floppy.v
//
// Floppy drive emulation toplevel
//
// --------------------------------------------------------------------

module floppy(
    input               clk,
    input               ce,
    input               reset_n,
    input               sd_dat,     // sd card signals
    output  reg         sd_dat3,    // sd card signals
    output              sd_cmd,     // sd card signals
    output              sd_clk,     // sd card signals
    output              uart_txd,   // uart tx pin
    
    // I/O interface to host system (Vector-06C)
    input       [2:0]   hostio_addr,
    input       [7:0]   hostio_idata,
    output      [7:0]   hostio_odata,
    input               hostio_rd,
    input               hostio_wr,

    // SDRAM interface
    output      [22:0]  sdram_addr,
    output      [7:0]   sdram_data_o,
    input       [7:0]   sdram_data_i,
    output              sdram_read,
    output              sdram_write,
    input               sdram_busy,
    
    // keyboard interface
    input       [5:0]   keyboard_keys,  // {reserved,left,right,up,down,enter}
    
    // screen memory
    output      [7:0]   display_addr,
    output      [7:0]   display_data,
    output              display_wren,
    input       [7:0]   display_idata,
    
    output  reg [7:0]   osd_command,
    output  reg [7:0]   green_leds,
    
    output      [7:0]   red_leds,
    output      [7:0]   debug,
    output      [7:0]   debugidata,
    output              host_hold,
    
    output      [3:0]   vg93debug
);

    
parameter IOBASE = 16'hE000;
parameter SDRAM_WINDOW_BASE = 16'h5000;      // 32kb page mapped to sdram
parameter SDRAM_WINDOW_SIZE = 16'h8000; 

parameter PORT_MMCA= 0;
parameter PORT_SPDR= 1;
parameter PORT_SPSR= 2;
parameter PORT_JOY = 3;
parameter PORT_TXD = 4;
parameter PORT_RXD = 5;
parameter PORT_CTL = 6;

parameter PORT_TMR1 = 7;
parameter PORT_TMR2 = 8;

parameter PORT_CPU_REQUEST  = 9;
parameter PORT_CPU_STATUS   = 10;
parameter PORT_TRACK        = 11;
parameter PORT_SECTOR       = 12;

parameter PORT_DMA_MSB = 14;    // spi dma target address msb
parameter PORT_DMA_LSB = 15;    // spi dma target address lsb

parameter PORT_LED = 16;
parameter PORT_OSD_COMMAND = 17;        // {F11,F12,HOLD}

parameter PORT_SDRAM_PAGE = 18;

//assign addr = cpu_a;
//assign odata = cpu_do;
assign red_leds = {spi_wren,dma_debug[6:0]};
assign debug = wdport_status;
assign debugidata = {ce & bufmem_en, ce, hostio_rd, wd_ram_rd};

wire    [15:0]  cpu_ax;
wire            memwrx;
wire    [7:0]   cpu_dox;

wire    [15:0]  cpu_a = dma_ready ? cpu_ax  : dma_oaddr;
wire		memwr = dma_ready ? memwrx  : dma_memwr;
wire    [7:0]   cpu_do = dma_ready ? cpu_dox: dma_odata;
reg     [7:0]   cpu_di;

//// Workhorse 6502 CPU
wire ready = /*ce & */ ~(wd_ram_rd|wd_ram_wr|~dma_ready|~sdram_ready);
cpu cpu(.clk(clk),
    .clken(ce),
    .reset(~reset_n),
    .AB(cpu_ax),
    .DI(cpu_di),
    .DO(cpu_dox),
    .WE(memwrx),
    .IRQ(1'b0),
    .NMI(1'b0),
    .RDY(ready));

//always @(cpu_ax) $display("cpu_ax=%04x", cpu_ax);

// Main RAM, Low-mem, Buffer-mem, I/O ports to CPU connections
wire    [7:0]   ram_do;
wire    [7:0]   lowmem_do;
wire    [7:0]   bufmem_do;
wire    [7:0]   sdram_do;
reg     [7:0]   ioports_do;

reg [7:0] boot_vec;
always @(posedge clk) 
    boot_vec <= cpu_a[0] ? 8'h08 : 8'h00;

    
// problem: Arlet's 6502 advances AB instantly when reading ram
// so sdram_en will likely be up simultaneously with e.g. rammem_en
// for sdram the address is locked in sdram_interface, but for the selectors
// we have to prioritise sdram_en
    
reg [5:0] di_select;
always @(posedge clk)
    if (ce)
        di_select = {&cpu_a[15:4], lowmem_en, bufmem_en, rammem_en, osd_en, 
                    sdram_en};

always @* begin: _cpu_datain
    casez(di_select) 
    6'b1?????:   cpu_di <= boot_vec; //(cpu_a[0] ? 8'h08:8'h00); // boot vector
    6'b?????1:   cpu_di <= sdram_do; // do not move this line
    6'b010000:   cpu_di <= lowmem_do;
    6'b001000:   cpu_di <= bufmem_do;
    6'b000100:   cpu_di <= ram_do;
    6'b000010:   cpu_di <= display_idata;
    default:    cpu_di <= ioports_do;
    endcase
end                         


//always @cpu_di $display("cpu_di=%02x", cpu_di);

// memory enables
wire lowmem_en = |cpu_a[15:9] == 0;
wire bufmem_en = (wd_ram_rd|wd_ram_wr) || (cpu_a >= 16'h200 && cpu_a < 16'h600);
wire rammem_en = (cpu_a >= 16'h0800) && (cpu_a < 16'h5000);

wire sdram_address_match = cpu_a >= SDRAM_WINDOW_BASE && 
    cpu_a < (SDRAM_WINDOW_BASE + SDRAM_WINDOW_SIZE);

reg sdram_en_delay = 0;
wire sdram_en = sdram_en_delay | sdram_address_match;
always @(negedge clk)
    if (sdram_address_match & ~memwr)
        sdram_en_delay <= 1'b1;
    else if (sdram_ready)
        sdram_en_delay <= 1'b0;

wire ioports_en= cpu_a >= IOBASE && cpu_a < IOBASE + 256;
wire osd_en = cpu_a >= IOBASE + 256 && cpu_a < IOBASE + 512;

assign display_addr = cpu_a[7:0];
assign display_data = cpu_do;
assign display_wren = osd_en & memwr;

wire [14:0] ram_adrs = cpu_a-16'h0800;
ram 
    #(.ADDR_WIDTH(15),.DEPTH(18432),
        .HEXFILE("../firmware/6502/disk.hax")) 
    flopramnik(
    .clk(clk),
    .cs(ce & rammem_en),
    .addr(ram_adrs),
    .we(memwr),
    .data_in(cpu_do),
    .data_out(ram_do)
);

ram #(.ADDR_WIDTH(9),.DEPTH(512)) zeropa(
    .clk(clk),
    .cs(ce & lowmem_en),
    .addr(cpu_a[8:0]),
    .we(memwr),
    .data_in(cpu_do),
    .data_out(lowmem_do));

wire [9:0]  bufmem_addr = (wd_ram_rd|wd_ram_wr) ? wd_ram_addr : cpu_a - 10'h200;
wire        bufmem_wren = wd_ram_wr | memwr;
wire [7:0]  bufmem_di = wd_ram_wr ? wd_ram_odata : cpu_do;

ram #(.ADDR_WIDTH(10),.DATA_WIDTH(8),.DEPTH(1024)) bufpa(
    .clk(clk),
    .cs(ce & bufmem_en),
    .addr(bufmem_addr),
    .we(bufmem_wren),
    .data_in(bufmem_di),
    .data_out(bufmem_do));

/////////////////////
// CPU INPUT PORTS //
/////////////////////
always @(posedge clk) begin
    case (cpu_a)         
    IOBASE+PORT_CTL:    ioports_do <= {7'b0,uart_busy}; // uart status
    IOBASE+PORT_TMR1:   ioports_do <= timer1q;
    IOBASE+PORT_TMR2:   ioports_do <= timer2q;
    IOBASE+PORT_SPDR:   ioports_do <= spdr_do;
    IOBASE+PORT_SPSR:   ioports_do <= {7'b0,~spdr_dsr};
    IOBASE+PORT_CPU_REQUEST:ioports_do <= wdport_cpu_request;
    IOBASE+PORT_TRACK:  ioports_do <= wdport_track;
    IOBASE+PORT_SECTOR: ioports_do <= wdport_sector;
    
    IOBASE+PORT_JOY:    ioports_do <= keyboard_keys;
    default:            ioports_do <= 8'hFF;
    endcase
end

//always @(cpu_a)
//begin
//    if (cpu_a == 16'h3c25) $display("->zerobss");
//    if (cpu_a == 16'h3575) $display("->copydata");
//    if (cpu_a == 16'h3c48) $display("->initlib");
//    if (cpu_a == 16'h3475) $display("->_main");
//end

/////////////////////
// CPU OUTPUT PORTS //
/////////////////////
always @(posedge clk or negedge reset_n) begin
    if (!reset_n) begin
        green_leds <= 0;
        uart_state <= 3;
        uart_send <= 0;
        sd_dat3 <= 1;
        sdram_page <= 0;
    end else begin
        if (ce) begin
            if (memwr && cpu_a[15:8] == 8'hE0) begin
                if (cpu_a[7:0] == 8'h10) begin
                    green_leds <= cpu_do;
                end
                
                // E004: send data
                if (cpu_a[7:0] == PORT_TXD) begin
                    uart_data <= cpu_do;
                    uart_state <= 0;
                end
                
                // MMCA: SD/MMC card chip select
                if (cpu_a[7:0] == PORT_MMCA) begin
                    sd_dat3 <= cpu_do[0];
                end
                
                // CPU status return
                if (cpu_a[7:0] == PORT_CPU_STATUS) begin
                    wdport_cpu_status <= cpu_do;
                end
                
                if (cpu_a[7:0] == PORT_OSD_COMMAND) begin
                    osd_command <= cpu_do;
                end
                
                // DMA
                if (cpu_a[7:0] == PORT_DMA_MSB) dma_msb <= cpu_do;
                if (cpu_a[7:0] == PORT_DMA_LSB) dma_lsb <= cpu_do;
                
                if (cpu_a[7:0] == PORT_SPSR) begin
                    dma_blocks <= cpu_do[7:4];
                end 

                // SDRAM mapping
                if (cpu_a[7:0] == PORT_SDRAM_PAGE) sdram_page <= cpu_do;
            end
            else dma_blocks <= 4'h0;
            
            // uart state machine
            case (uart_state) 
            0:  begin
                    if (~uart_busy) begin
                        uart_send <= 1;
                        uart_state <= 1;
                    end
                end
            1:  begin
                    if (uart_busy) begin
                        uart_send <= 0;
                        uart_state <= 2;
                    end
                end
            2:  begin
                    if (~uart_busy) begin
                        uart_data <= uart_data + 1;
                        if (uart_data == 65+27) uart_data <= 8'd65;
                        uart_state <= 3;
                    end
                end
            3:  begin
                end
            endcase     
        end
    end
end

//////////////////
// UART Console //
//////////////////
reg         uart_send;
reg  [7:0]  uart_data;
wire        uart_busy;
reg  [1:0]  uart_state = 3;

`ifdef SIMULATION
`define BAUDRATE 12000000
`else
`define BAUDRATE 115200
`endif

uart_interface #(.SYS_CLK(24000000),.BAUDRATE(`BAUDRATE)) uart0(
    .clk(clk),
    .reset(~reset_n),
    .cs(ce),
    .rs(1'b1),
    .we(uart_send),
    .din(uart_data),
    .uart_tx(uart_txd),
    .tx_busy(uart_busy));
   
////////////
// TIMERS //
////////////

wire [7:0] timer1q;
wire [7:0] timer2q;

timer100hz timer1(.clk(clk), .reset(~reset_n), .di(cpu_do), 
    .wren(ce && cpu_a==(IOBASE+PORT_TMR1) && memwr), .q(timer1q));
timer100hz timer2(.clk(clk), .reset(~reset_n), .di(cpu_do), 
    .wren(ce && cpu_a==(IOBASE+PORT_TMR2) && memwr), .q(timer2q));

//////////////////////
// SPI/SD INTERFACE //
//////////////////////

wire [7:0]  spdr_do;
wire        spdr_dsr;
wire        spi_wren = (ce && (cpu_a == (IOBASE+PORT_SPDR) && memwr)) || dma_spiwr;

spi sd0(.clk(clk),
        .ce(1'b1),
        .reset_n(reset_n),
        .mosi(sd_cmd),
        .miso(sd_dat),
        .sck(sd_clk),
        .di(dma_ready ? cpu_do : dma_spido), 
        .wr(spi_wren), 
        .do(spdr_do), 
        .dsr(spdr_dsr)
        );

reg  [7:0]  dma_lsb, dma_msb;
wire [15:0] dma_oaddr;
wire [7:0]  dma_odata;
wire        dma_memwr;
reg  [3:0]  dma_blocks;
wire        dma_ready;
wire [7:0]  dma_spido;
wire        dma_spiwr;
wire [7:0]  dma_debug;

dma_rw pump0(
        .clk(clk), 
        .ce(ce), 
        .reset_n(reset_n), 
        .iaddr({dma_msb,dma_lsb}),
        .oaddr(dma_oaddr), 
        .odata(dma_odata),
        .idata(cpu_di), 
        .owren(dma_memwr), 
        .nblocks(dma_blocks), 
        .ready(dma_ready), 
        .ospi_data(dma_spido), 
        .ispi_data(spdr_do), 
        .ospi_wr(dma_spiwr), 
        .ispi_dsr(spdr_dsr),
        .debug(dma_debug));

////////////
// WD1793 //
////////////

// here's how 1793's registers are mapped in Vector-06c
// 00011xxx
//      000     $18     Data
//      001     $19     Sector
//      010     $1A     Track
//      011     $1B     Command/Status
//      100     $1C     Control             Write only

wire [7:0]  wdport_track;
wire [7:0]  wdport_sector;
wire [7:0]  wdport_status;
wire [7:0]  wdport_cpu_request;
reg [7:0]   wdport_cpu_status;

wire [9:0]  wd_ram_addr;
wire        wd_ram_rd;
wire        wd_ram_wr;  
wire [7:0]  wd_ram_odata;   // this is to write to ram

wd1793 vg93(
    .clk(clk), 
    .clken(ce), 
    .reset_n(reset_n),

    // host i/o ports 
    .rd(hostio_rd), 
    .wr(hostio_wr), 
    .addr(hostio_addr), 
    .idata(hostio_idata), 
    .odata(hostio_odata), 

    // memory buffer interface
    .buff_addr(wd_ram_addr), 
    .buff_rd(wd_ram_rd), 
    .buff_wr(wd_ram_wr), 
    .buff_idata(bufmem_do),     // data read from ram
    .buff_odata(wd_ram_odata),  // data to write to ram

    // workhorse interface
    .oTRACK(wdport_track),
    .oSECTOR(wdport_sector),
    .oSTATUS(wdport_status),
    .oCPU_REQUEST(wdport_cpu_request),
    .iCPU_STATUS(wdport_cpu_status),

    .wtf(host_hold),
    .debug(vg93debug));

// A window into SDRAM world: 32kb, msb selected by sdram_page
// pages 0..7 map to kvaz

reg [7:0] sdram_page;
wire      sdram_ready;
wire [14:0] sdram_offset = cpu_a - SDRAM_WINDOW_BASE;

sdram_iface sdramiface(.clk(clk), .reset(~reset_n),
    .page(sdram_page), .offset(sdram_offset),
    .addr(sdram_addr), .data_i(sdram_data_i), .data_o(sdram_data_o),
    .ena(sdram_en), .memwr(memwr), .cpu_data(cpu_do), .data_to_cpu(sdram_do),
    .sdram_rd(sdram_read), .sdram_wr(sdram_write), .busy(sdram_busy), 
    .ready(sdram_ready));

endmodule


module sdram_iface(
    input               clk,
    input               reset,

    input       [7:0]   page,
    input       [14:0]  offset,
    input               ena,        // window select 
    input               memwr,      // cpu writes to sdram
    input       [7:0]   cpu_data,   // data from cpu
    output      [7:0]   data_to_cpu,

    output reg  [22:0]  addr,
    input       [7:0]   data_i,
    output reg  [7:0]   data_o,
    output reg          sdram_rd,
    output reg          sdram_wr,
    input               busy,       // from sdram arbitre
    output              ready       // to cpu
);

assign data_to_cpu = data_i;

assign ready = (state == 0);

reg [2:0] state;
always @(posedge clk) begin
    if (reset) begin
        state <= 0;
        {sdram_rd,sdram_wr} = 2'b00;
        addr <= 0;
    end
    else case (state)
        0:  if (ena) begin
            addr <= {page, offset};
            state <= 1;
            end
        1:  begin
            if (ena) begin
                {sdram_rd,sdram_wr} <= memwr ? 2'b01 : 2'b10;
		if (memwr) data_o <= cpu_data;
                //if (memwr)
                //    $display("SDRAM WR: %04x<-%02x", addr, cpu_data);
                //else
                //    $display("SDRAM RD: %04x", addr);
                state <= 2;
            end
            else
                state <= 0;
            end
        2:  begin
            {sdram_rd,sdram_wr} <= 2'b00;
            if (!busy) state <= 0;
            end
    endcase
end

endmodule

