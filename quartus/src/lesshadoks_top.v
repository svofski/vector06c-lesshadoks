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
	
	output		ESP_SS_N,
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
	
	input		VU_RESET,		//  ВУ ~СБРОС (41)
	input		FREE_IO2,

	output 	[7:0]	virt_kvaz_control_word
	);
	

// Debug wires
assign virt_kvaz_control_word = {sdram_pls, sdram_distributor, kvaz_memwr ^ kvaz_memrd};

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

bus_sampler busync(.clk(clk_cpu), .reset(sys_reset),
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

wire 		kvaz_do_e = ~(VU_BLK_N | (cchtzu_n & cchtvv_n));
assign 		VU_SHD = kvaz_do_e ? kvaz_do : 8'bzzzzzzzz;
assign 		VU_DIR_N = ~kvaz_do_e;

// ЧТЗУ может вылезти до завершения полного цикла RAS/CAS, 
// поэтому для него специальный ритуал синхронизации
// kvaz_memrd является выходом нижеследующего КА и держится 1 такт 
// после получения валидного decoded_a
parameter KVAZ_MEMRD_S0 = 0, KVAZ_MEMRD_S1 = 1, KVAZ_MEMRD_S2 = 2, KVAZ_MEMRD_S3 = 3;
reg [2:0] kvaz_memrd_state;
wire	  kvaz_memrd_flag = (~VU_BLK_N) & negedge_chtzu_n;
always @(posedge clk_cpu or posedge sys_reset) begin: _kvaz_memrd_sync
	if (sys_reset) begin
		kvaz_memrd_state <= KVAZ_MEMRD_S0;
	end 
	else 
	case (kvaz_memrd_state)
	KVAZ_MEMRD_S0:	casez ({kvaz_memwr,kvaz_memrd_flag,decoded_a_valid}) 
			3'b1??:	begin {sdram_write,sdram_read} <= 2'b10; kvaz_memrd_state <= KVAZ_MEMRD_S3; end
			3'b011: begin {sdram_write,sdram_read} <= 2'b01; kvaz_memrd_state <= KVAZ_MEMRD_S3; end
			3'b010: begin {sdram_write,sdram_read} <= 2'b00; kvaz_memrd_state <= KVAZ_MEMRD_S1; end
			endcase
	KVAZ_MEMRD_S1:	if (decoded_a_valid) begin {sdram_write,sdram_read} <= 2'b01; kvaz_memrd_state <= KVAZ_MEMRD_S3; end
	KVAZ_MEMRD_S3:	begin
			{sdram_read,sdram_write} <= 2'b00;
			kvaz_memrd_state <= KVAZ_MEMRD_S0;
			end
	endcase
end

wire kvaz_memrd = kvaz_memrd_state == KVAZ_MEMRD_S3;

wire [15:0] 	decoded_a;
wire	  	decoded_a_valid;
shap_decoder address_decoder(.clk(clk_cpu), .reset(sys_reset), 
	.negedge_ras_n(negedge_ras_n), .negedge_cas_n(negedge_cas_n),
	.clean_ras_n(cras_n), .clean_cas_n(ccas_n), .VU_SHAP_N(VU_SHAP_N),
	.decoded_a(decoded_a), .valid(decoded_a_valid));


kvaz ramdisk(
    .clk(clk_cpu),
    .clke(ce_cpu), 
    .reset(sys_reset),
    .shavv(~VU_SHAVV_N),
    .address(decoded_a),
    .select(ramdisk_control_write),
    .data_in(VU_SHD),
    .stack(cstack), 
    .memwr(kvaz_memwr), 		// todo: not used in kvaz module
    .memrd(kvaz_memrd), 		// todo: not used in kvaz module
    .bigram_addr(kvaz_page),
    .blk_n(VU_BLK_N),
    .debug(kvaz_debug)
);

wire [17:0] kvaz_sdram_addr = {kvaz_page, decoded_a[15:1]};
wire kvaz_lb = decoded_a[0];
wire kvaz_ub = ~decoded_a[0];
		
wire [7:0] kvaz_do = kvaz_lb ? sdram_do[7:0] : sdram_do[15:8];
	
wire ce_cpu = 1'b1;	

// system reset debouncer	
reg [5:0] sys_reset_counter = 1;
wire sys_reset = |sys_reset_counter;
always @(posedge clk_cpu) begin
	if (~BUTT1 | VU_RESET) begin
		sys_reset_counter <= 1;
	end
	else begin
		if (sys_reset) sys_reset_counter <= sys_reset_counter + 1;
	end
end
//
	
wire [21:0] sdram_addr;			// SDRAM addr, 4M words

assign sdram_addr = kvaz_sdram_addr; 

wire [15:0] sdram_do;
wire sdram_ub = kvaz_ub;
wire sdram_lb = kvaz_lb;
wire sdram_busy;
reg sdram_read;			// read request to controller
reg sdram_write;		// write request to controller

`ifdef SDRAM_TRU

assign DRAM_CLK=clk_sdram;              //  SDRAM Clock
assign DRAM_CKE=1;                      //  SDRAM Clock Enable

SDRAM_Controller ramd(
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
    
    .iaddr(sdram_addr),			// address from cpu [21:0]
    .dataw(data_to_sdram),		// data from cpu [15:0]
    .rd(sdram_read),			// read request
    .we_n(~sdram_write), 		// write request neg
    .ilb_n(~sdram_lb),			// lower byte mask
    .iub_n(~sdram_ub),			// upper byte mask
    .datar(sdram_do),			// data from sdram [15:0]
    .membusy(sdram_busy)		// sdram busy flag
   ,.refresh(sdram_pls)
);


// приглашение выполнить рефреш когда кваз неактивен и приходит 
// posdedge RAS_N без CAS_N
// эмпирические данные о частоте выборки:
// кваз незадействован
// 	30 раз за 100мкс, то есть 1 раз каждые 3.3мкс
// 	или 19394 раза за 64мс, то есть в 4.7 раз чаще, чем требуемые 4096
// кваз максимально загружен push/pop:
//	15 раз за 100мкс, то есть 1 раз каждые 7мкс
//	или 9143 раза за 64 мкс, то есть в 2.2 раза чаще, чем требуемые 4096
// кваз максимально загружен mov m, m
// 	21 раз за 100мкс, или 1 раз каждый 4.8мкс
//	или 13333, или в 3.2 раза чаще, чем требуемые 4096

// доступ для третьей стороны один свободный цикл из двух:
// худшая оценка 14мкс, скорость заполнения 142857 байт/с
// лучшая оценка 6.6мкс, скорость заполнения 303030 байт/с


// когда вообще не страшно лезть в SDRAM
wire sdram_free_access_slot = posedge_ras_n & ccas_n & VU_BLK_N;
// крышка парораспределителя
reg sdram_distributor;
// команда запуска авторефреша
wire sdram_pls = sdram_distributor & sdram_free_access_slot;
// разрешение доступа зпу
wire sdram_zpu = ~sdram_distributor & sdram_free_access_slot;
always @(posedge clk_cpu) begin: _gen_refresh_cmd
	if (sdram_free_access_slot)
		sdram_distributor <= ~sdram_distributor;
end

`else

assign DRAM_CLK=0;              //  SDRAM Clock
assign DRAM_CKE=0;              //  SDRAM Clock Enable


testram dummyram(
	.address(sdram_addr - 'h5000),
	.clock(clk_cpu),
	.byteena({sdram_ub,sdram_lb}),
	.data(data_to_sdram),
	.wren(sdram_write),
	.q(sdram_do));
assign sdram_busy = 1'b0;

`endif
		
wire [15:0] data_to_sdram = {VU_SHD,VU_SHD};

wire [7:0] sound_data_o;
soundinterfaces soundinterfaces(.clk(clk_cpu), .reset(sys_reset),
	.shavv(shavv_r), .data(VU_SHD), .data_o(sound_data_o), 
	.negedge_zpvv_n(negedge_zpvv_n), .negedge_chtvv_n(negedge_chtvv_n),
	.audio_l(AUDIO_L), .audio_r(AUDIO_R));

	
//wire [31:0] 	sdram32_do;
//wire [31:0] 	sdram32_di;
//wire [15:0] 	zpu_mem_addr;
//wire 		zpu_mem_wren;
//wire 		zpu_mem_rden;
//wire [3:0] 	zpu_writemask;	
//wire 		zpu_break;	
//zpu_core_flex zpushnik(.clk(clk_cpu), .reset(sys_reset),
//	.enable(1),
//	.in_mem_busy(sdram_busy),
//	.mem_read(sdram32_do),
//	.mem_write(sdram32_di),
//	.out_mem_addr(zpu_mem_addr),
//	.out_mem_writeEnable(zpu_mem_wren),
//	.out_mem_readEnable(zpu_mem_rden),
//	.mem_writeMask(zpu_writemask),
//	.interrupt(0),
//	.break(zpu_break)
//      );	
	
	
endmodule
