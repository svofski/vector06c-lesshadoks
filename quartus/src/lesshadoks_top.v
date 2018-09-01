// ====================================================================
// LES SHADOKS POMPAIENT: Vector-06c Expansion Board
//
// Copyright (C) 2018, Viacheslav Slavinsky
//
// This core is distributed under modified BSD license. 
// For complete licensing information see LICENSE.TXT.
// -------------------------------------------------------------------- 

`default_nettype none

`define SDRAM_TRU
`define WITH_AY
`define WITH_VI53

module lesshadoks_top(
	input		CLK50, 
	inout   [15:0]  DRAM_DQ,                //  SDRAM Data bus 16 Bits
	output  [11:0]  DRAM_ADDR,              //  SDRAM Address bus 12 Bits
	output          DRAM_LDQM,              //  SDRAM Low-byte Data Mask 
	output          DRAM_UDQM,              //  SDRAM High-byte Data Mask
	output          DRAM_WE_N,              //  SDRAM Write Enable
	output          DRAM_CAS_N,             //  SDRAM Column Address Strobe
	output          DRAM_RAS_N,             //  SDRAM Row Address Strobe
	output          DRAM_CS_N,              //  SDRAM Chip Select
	output          DRAM_BA_0,              //  SDRAM Bank Address 0
	output          DRAM_BA_1,              //  SDRAM Bank Address 0
	output          DRAM_CLK,		//  SDRAM Clock
	output          DRAM_CKE,		//  SDRAM Clock Enable
		
	input		ADC_SND,
	output		ADC_SNDINT,
	
	output		SPI_CLK,
	output		SPI_MOSI,
	input		SPI_MISO,
	
	input		BUTT1,
	
	output		ESP_SS_N,		// ESP8266 GPIO15
	input		ESP_RXD,
	output		ESP_TXD,
	
	output		SD_SS_N,
	output		JOY_SS_N,
	
	output		AUDIO_L,
	output		AUDIO_R,

	input	[7:0]	VU_SHAVV_N,		//  ВУ ~ШАВВ
	input	[7:0]	VU_SHAP_N,		//  ВУ ~ШАП
	inout	[7:0]	VU_SHD,			//  ВУ  ШД
	input		VU_RAS_N,		//  ВУ ~RAS
	input		VU_CAS_N,		//  ВУ ~CAS
	input		VU_ZPZU_N,		//  ВУ ~ЗПЗУ (~MEMWR)
	input		VU_CHTZU_N,		//  ВУ ~ЧТЗУ (~MEMRD)
	input		VU_CHTVV_N,		//  ВУ ~ЧТВВ (~IORD)
	input		VU_ZPVV_N,		//  ВУ ~ЗПВВ (~IOWR)
	output		VU_BLK_N,		//  ВУ ~БЛК
	output		VU_DIR_N,		//  ВУ направление ШД: 0 = от Шадков к Вектору
	input		VU_STACK,		//  ВУ  СТЕК
	input		VU_STROB_SOST, 		//  ВУ  СТРОБ.СОСТ	
	
	input		VU_RESET,		//  ВУ ~ВВОД СИСТЕМЫ (39) 
	input		FREE_IO2,

	output 	[7:0]	virt_kvaz_control_word
	);
	

// Debug wires

reg [7:0] strobdelay;
always @(posedge clk_cpu)
	strobdelay <= {strobdelay[6:0], VU_STROB_SOST};
	
assign virt_kvaz_control_word = {floppy_sdram_busy, ~VU_ZPVV_N & (shavv_r==8'h1), ramc_read, kvaz_memrd_flag};

wire sys_reset;
reset_debouncer reset_debouncer(.clk(clk_cpu), 
	.butt_n(BUTT1), 
	.vu_reset(VU_RESET),  // sys_reset от БЛК+СБР получается деструктивно
	.reset_o(sys_reset));	

// --------------
// CLOCK SECTION
// --------------
wire clk_24;

`ifdef SDRAM_TRU
wire clk_sdram;	
`endif

reloj montre(.inclk0(CLK50), .c0(clk_24)
`ifdef SDRAM_TRU
,.c1(clk_sdram)
`endif
);
wire clk_cpu = clk_24;

// --------------
// BUS INTERFACE
// --------------

// синхронизированные фронты сигналов ВУ
wire negedge_ras_n, negedge_cas_n, negedge_zpzu_n, negedge_chtzu_n,
	negedge_chtvv_n, negedge_zpvv_n, posedge_ras_n, posedge_stack,
	posedge_strob_sost;
	
wire cras_n, ccas_n, cstack, cstrob_sost;
wire cchtzu_n, cchtvv_n;
wire [7:0] shavv_r;

bus_sampler busync(.clk(clk_cpu), 
	.reset(sys_reset),
	.VU_RAS_N(VU_RAS_N), .VU_CAS_N(VU_CAS_N), .VU_ZPZU_N(VU_ZPZU_N),
	.VU_CHTZU_N(VU_CHTZU_N), .VU_ZPVV_N(VU_ZPVV_N), .VU_CHTVV_N(VU_CHTVV_N),
	.VU_STACK(VU_STACK), .VU_STROB_SOST(VU_STROB_SOST), 
	.VU_SHAVV_N(VU_SHAVV_N),
	
	.negedge_ras_n(negedge_ras_n), .negedge_cas_n(negedge_cas_n),
	.negedge_zpzu_n(negedge_zpzu_n), .negedge_chtzu_n(negedge_chtzu_n),
	.negedge_chtvv_n(negedge_chtvv_n), .negedge_zpvv_n(negedge_zpvv_n),
	
	.posedge_ras_n(posedge_ras_n), .posedge_stack(posedge_stack),
	.posedge_strob_sost(posedge_strob_sost),
	
	.clean_ras_n(cras_n), .clean_cas_n(ccas_n), .clean_stack(cstack),
	.clean_chtzu_n(cchtzu_n), .clean_chtvv_n(cchtvv_n),
	.clean_strob_sost(cstrob_sost),
	.clean_shavv(shavv_r));

	
wire [2:0]	kvaz_page;
wire 		ramdisk_control_write = (shavv_r == 8'h10) && negedge_zpvv_n;
wire [7:0]  	kvaz_debug;
wire 		kvaz_memwr = (~VU_BLK_N) & negedge_zpzu_n;

wire [7:0]	iodev_do = fdc_sel ? fdc_do :
			   sound_sel ? sound_do : 
                           joy_sel ? joy_do : 8'hff;

wire		iodev_do_e = ~(iodev_blk_n | cchtvv_n);
wire		kvaz_do_e = ~(kvaz_blk_n | cchtzu_n);
wire 		vu_shd_oe = iodev_do_e | kvaz_do_e;
wire		kvaz_blk_n;

wire		iodev_blk_n = ~((fdc_sel | joy_sel) & ~cchtvv_n);

assign 		VU_BLK_N = kvaz_blk_n & iodev_blk_n;
assign 		VU_DIR_N = ~vu_shd_oe;
assign 		VU_SHD = kvaz_do_e ? kvaz_do :
			iodev_do_e ? iodev_do : 8'bzzzzzzzz;

// ЧТЗУ может вылезти до завершения полного цикла RAS/CAS, 
// поэтому для него специальный ритуал синхронизации
// kvaz_memrd является выходом нижеследующего КА и держится 1 такт 
// после получения валидного decoded_a
parameter KVAZ_MEMRD_S0 = 0, KVAZ_MEMRD_S1 = 1, KVAZ_MEMRD_S2 = 2, KVAZ_MEMRD_S3 = 3;
reg [2:0] kvaz_memrd_state;

reg [7:0] psw;
wire 		psw_inp = psw[6];
wire		psw_out = psw[4];
wire		psw_wo_n = psw[1];
wire		psw_memr = psw[7];

reg	  memrd_psw;
always @(posedge clk_cpu or posedge sys_reset)
	if (sys_reset) begin
		memrd_psw <= 1'b0;
		psw <= 8'h00;
	end
	else if (posedge_strob_sost) begin
		memrd_psw <= VU_SHD[7];
		psw <= VU_SHD;
	end
	else if (kvaz_memrd_state == KVAZ_MEMRD_S3)
		memrd_psw <= 1'b0;
	

wire	  kvaz_memrd_flag = (~kvaz_blk_n) & memrd_psw; //negedge_chtzu_n;
reg	  kvaz_write;


reg 	  kvaz_read;
// this ok: 
wire      kvaz_read_fast = kvaz_read;// | (kvaz_memrd_flag & decoded_a_valid) | ((kvaz_memrd_state == KVAZ_MEMRD_S1) & decoded_a_valid);
// this not: wire      kvaz_read_fast = kvaz_read | ((kvaz_memrd_state == KVAZ_MEMRD_S1) & decoded_a_valid);
//wire      kvaz_read_fast = kvaz_read | (kvaz_memrd_flag & decoded_a_valid);

always @(posedge clk_cpu or posedge sys_reset) begin: _kvaz_memrd_sync
	if (sys_reset) begin
		kvaz_memrd_state <= KVAZ_MEMRD_S0;
	end 
	else 
	case (kvaz_memrd_state)
	KVAZ_MEMRD_S0:	casez ({kvaz_memwr,kvaz_memrd_flag,decoded_a_valid}) 
			3'b1??:	begin {kvaz_write,kvaz_read} <= 2'b10; kvaz_memrd_state <= KVAZ_MEMRD_S3; end
			3'b011: begin {kvaz_write,kvaz_read} <= 2'b01; kvaz_memrd_state <= KVAZ_MEMRD_S3; end
			3'b010: begin {kvaz_write,kvaz_read} <= 2'b00; kvaz_memrd_state <= KVAZ_MEMRD_S1; end
			endcase
	KVAZ_MEMRD_S1:	if (decoded_a_valid) begin {kvaz_write,kvaz_read} <= 2'b01; kvaz_memrd_state <= KVAZ_MEMRD_S3; end
	KVAZ_MEMRD_S3:	begin
			{kvaz_write,kvaz_read} <= 2'b00;
			kvaz_memrd_state <= KVAZ_MEMRD_S0;
			end
	endcase
end

wire kvaz_enable_fakerom;	// <- fakerom_controller <- floppy

wire kvaz_memrd = kvaz_memrd_state == KVAZ_MEMRD_S3;
wire [15:0] 	decoded_a;
wire	  	decoded_a_valid;
wire at_zero = decoded_a_valid && decoded_a == 0;
shap_decoder address_decoder(.clk(clk_cpu), .reset(sys_reset), 
	.negedge_ras_n(negedge_ras_n), .negedge_cas_n(negedge_cas_n),
	.clean_ras_n(cras_n), .clean_cas_n(ccas_n), .VU_SHAP_N(VU_SHAP_N),
	.decoded_a(decoded_a), .valid(decoded_a_valid));

	
kvaz ramdisk(
    .clk(clk_cpu),
    .clke(ce_cpu), 
    .reset(sys_reset),
    .shavv(shavv_r),
    .address(decoded_a),
    .select(ramdisk_control_write),
    .data_in(VU_SHD),
    .stack(cstack), 
    
    .boot(kvaz_enable_fakerom),
    .memrd(memrd_psw),    
    .memwr(~psw_wo_n),
    .iord(psw_inp),
    .iowr(psw_out),
    
    .bigram_addr(kvaz_page),
    .blk_n(kvaz_blk_n),
    .debug(kvaz_debug)
);

wire [17:0] kvaz_addr = {kvaz_page, decoded_a};		
wire [7:0] kvaz_do = ramc_lb ? ramc_data_from[7:0] : ramc_data_from[15:8];
	
wire ce_cpu = 1'b1;	

assign DRAM_CLK=clk_sdram;              //  SDRAM Clock
assign DRAM_CKE=1;                      //  SDRAM Clock Enable

wire [22:0] ramc_addr;
wire [15:0] ramc_data_to;
wire [15:0] ramc_data_from;
wire        ramc_read;
wire        ramc_write;
wire        ramc_lb, ramc_ub;
wire        ramc_refresh;
wire        ramc_busy;

SDRAM_Controller ramc(
    .clk(clk_sdram),                    //  SDRAM clock
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
    
    .iaddr(ramc_addr),			// address from cpu [21:0]
    .dataw(ramc_data_to),		// data from cpu [15:0]
    .datar(ramc_data_from),		// data from sdram [15:0]
    .ilb_n(~ramc_lb),			// lower byte mask
    .iub_n(~ramc_ub),			// upper byte mask
    .rd(ramc_read),			// read request
    .we_n(~ramc_write), 		// write request neg
    .membusy(ramc_busy),		// sdram busy flag
    .refresh(ramc_refresh)
);

		
wire sound_sel; // todo -- route selector from soundinterfaces
wire [7:0] sound_do;
soundinterfaces soundinterfaces(.clk(clk_cpu), .reset(sys_reset),
	.shavv(shavv_r), .data(VU_SHD), .data_o(sound_do), 
	.negedge_zpvv_n(negedge_zpvv_n), .negedge_chtvv_n(negedge_chtvv_n),
	.audio_l(AUDIO_L), .audio_r(AUDIO_R));
	
wire ruslat;
ioports_spy iospy0(.clk(clk_cpu), .reset(sys_reset),
	.shavv(shavv_r), .data(VU_SHD), .negedge_zpvv_n(negedge_zpvv_n),
	.ruslat_o(ruslat));

// когда вообще не страшно лезть в SDRAM
wire sdram_free_access_slot = posedge_ras_n & ccas_n & VU_BLK_N;
	
/////////
sdram_arbitre arbitre0(
    .clk(clk_cpu), .reset(sys_reset),

    .access_slot(sdram_free_access_slot),

    .vu_adrs(kvaz_addr),
    .vu_data_i(VU_SHD),
    .vu_write(kvaz_write),
    .vu_read(kvaz_read_fast),

    .disk_adrs(floppy_sdram_addr),
    .disk_data_i(floppy_sdram_do),
    .disk_data_o(floppy_sdram_di),
    .disk_write(floppy_sdram_write),
    .disk_read(floppy_sdram_read),
    .disk_halfword(1'b0),
    .disk_byte(1'b1),
    .disk_ram_busy(floppy_sdram_busy),

    .sdram_addr(ramc_addr),
    .sdram_di(ramc_data_from),
    .sdram_do(ramc_data_to),
    .sdram_read(ramc_read),
    .sdram_write(ramc_write),
    .sdram_lb(ramc_lb),
    .sdram_ub(ramc_ub),
    .sdram_refresh(ramc_refresh),

    .sdram_busy(ramc_busy));
	

wire [22:0] floppy_sdram_addr;
wire [7:0]  floppy_sdram_do;
wire [7:0]  floppy_sdram_di;
wire        floppy_sdram_read;
wire        floppy_sdram_write;
wire        floppy_sdram_busy;

// ports 18..1b, 1c
wire		fdc_sel = shavv_r[7:3] == 5'b00011; // crude, low trib should be 000,001,010,011,100 not 101,110,111
wire 		fdc_wr = fdc_sel & negedge_zpvv_n;
wire		fdc_rd = fdc_sel & negedge_chtvv_n;
wire	[3:0]	fdc_adrs = {shavv_r[2],~shavv_r[1:0]};
wire	[7:0]	fdc_do;
wire 		fdc_boot_loaded;

// joysticks, read-only ports
wire    [7:0]   fdc_player1;
wire    [7:0]   fdc_player2;
wire            joy1_sel = (shavv_r == 8'h0e) & ~cchtvv_n;
wire            joy2_sel = (shavv_r == 8'h0f) & ~cchtvv_n;
wire            joy_sel = joy1_sel | joy2_sel;
wire    [7:0]   joy_do = joy1_sel ? fdc_player1 : joy2_sel ? fdc_player2 : 8'hff;

floppy floppy0(
    .clk(clk_cpu),
    .ce(1'b1),
    //.reset_n(1),//~sys_reset),
    .reset_n(~sys_reset),   // БЛК+ВВОД надо провести отдельно для начальной установки fakerom

    .sd_dat(SPI_MISO),      // sd card signals
    .sd_dat3(SD_SS_N),      // sd card signals
    .sd_cmd(SPI_MOSI),      // sd card signals
    .sd_clk(SPI_CLK),       // sd card signals
    .uart_txd(ESP_TXD),     // uart tx pin
    .esp_ss_n(ESP_SS_N),    // esp8266 slave select
    .joy_ss_n(JOY_SS_N),    // mcp23s17 slave select
    
    // I/O interface to host system (Vector-06C)
    .hostio_addr(fdc_adrs),
    .hostio_idata(VU_SHD),
    .hostio_odata(fdc_do),
    .hostio_rd(fdc_rd),
    .hostio_wr(fdc_wr),
    
    .fakerom_en(fdc_boot_loaded), // turn on fake bootrom at $0000

    // path to SDRAM
    .sdram_addr(floppy_sdram_addr),
    .sdram_data_o(floppy_sdram_do),
    .sdram_data_i(floppy_sdram_di),
    .sdram_read(floppy_sdram_read),
    .sdram_write(floppy_sdram_write),
    .sdram_busy(floppy_sdram_busy),

    // joysticks polled by SoC
    .player1(fdc_player1),
    .player2(fdc_player2)
);

fakerom_controller fakeromctrl(.clk(clk_cpu), 
	//.reset(sys_reset), 
	.reset(0), 
	.boot_loaded(fdc_boot_loaded), .ruslat(ruslat), 
	.enable_fakerom(kvaz_enable_fakerom));		
	
endmodule


module fakerom_controller(input clk, input reset, input boot_loaded, input ruslat, output enable_fakerom);

reg boot_loaded_r;
reg ruslat_r;

reg [3:0] state;
reg [3:0] ruslat_cnt;

localparam STATE_DISABLED = 0, STATE_ENABLED = 1;

assign enable_fakerom = state == STATE_ENABLED;

always @(posedge clk or posedge reset) begin: _sampler
	if (reset) begin
		boot_loaded_r <= 0;
		ruslat_r <= 0;
	end 
	else begin
		boot_loaded_r <= boot_loaded;
		ruslat_r <= ruslat;
	end
end

always @(posedge clk or posedge reset) begin: _fsm
	if (reset) 
		state <= STATE_DISABLED;
	else 
		case (state)
		STATE_DISABLED:	if (~boot_loaded_r & boot_loaded) state <= STATE_ENABLED;
		STATE_ENABLED: 	if (ruslat_cnt == 4) state <= STATE_DISABLED;
		endcase
end

// ruslat blink counter, enabled in fakerom state
always @(posedge clk or posedge reset) begin: _cnt
	if (reset)
		ruslat_cnt <= 0;
	else 
	case (state) 
	STATE_DISABLED: ruslat_cnt <= 0;
	STATE_ENABLED: 	if (ruslat_r & ~ruslat) ruslat_cnt <= ruslat_cnt + 1'b1;
	endcase
end

endmodule


module ioports_spy(input clk, input reset, input [7:0] shavv,
	input [7:0] data, input negedge_zpvv_n,
	output reg ruslat_o);
	
	
wire pc_sel = shavv == 8'h01;
wire cw_sel = shavv == 8'h00;
	
always @(posedge clk or posedge reset)
	if (reset) begin
		ruslat_o <= 1'b0;
	end
	else begin
		//if (negedge_zpvv_n & pc_sel) ruslat_o <= data[3];
		// загрузчик щелкает портом Ц в режиме BSR
		if (negedge_zpvv_n & cw_sel & (data[2:1]==2'b11)) ruslat_o <= data[0];
	end		
endmodule


module reset_debouncer(input clk, input butt_n, input vu_reset, output reset_o);

// дебаунса только чуть чуть, иначе он будет держаться, 
// когда уже начала исполняться программа
reg [5:0] ctr = 1;
assign reset_o = |ctr;
always @(posedge clk)
	if (~butt_n | vu_reset) 
		ctr <= 1;
	else if (reset_o)
		ctr <= ctr + 1;

endmodule

