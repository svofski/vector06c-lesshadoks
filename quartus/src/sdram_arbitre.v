`default_nettype none

module sdram_arbiter #(parameter VU_ABUS_WIDTH=18, ZPU_ABUS_WIDTH=22) (
	input clk, input reset,
	
	input [VU_ABUS_WIDTH-1:0] vu_adrs,
	input [7:0] vu_data,
	input vu_write,
	input vu_read,
	input access_slot,
	
	input [ZPU_ABUS_WIDTH-1:0] zpu_adrs,
	input [31:0] zpu_data,
	input zpu_write,
	input zpu_read,
	input zpu_halfword,
	input zpu_byte,
	output zpu_ram_busy,
	
	output [21:0] sdram_addr,
	output [15:0] data_to_sdram,
	output sdram_read,
	output sdram_write,
	output sdram_lb,
	output sdram_ub,
	output sdram_refresh,
	input  [15:0] sdram_dq,
		
	output busy,
	output [31:0] q);
	
wire [17:0] kvaz_sdram_addr = vu_adrs[17:1];
wire kvaz_lb = vu_adrs[0];
wire kvaz_ub = ~vu_adrs[0];
wire [15:0] kvaz_data_to_sdram = {vu_data,vu_data};
	
assign sdram_addr = kvaz_sdram_addr;
assign sdram_lb = kvaz_lb;
assign sdram_ub = kvaz_ub;
assign data_to_sdram = kvaz_data_to_sdram;
assign sdram_read = vu_read;
assign sdram_write = vu_write;
assign q = {sdram_dq,sdram_dq};
	
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
//wire sdram_free_access_slot = posedge_ras_n & ccas_n & VU_BLK_N;
// крышка парораспределителя
reg sdram_distributor;
// команда запуска авторефреша
assign sdram_refresh = sdram_distributor & access_slot;
// разрешение доступа зпу
wire sdram_zpu = ~sdram_distributor & access_slot;
always @(posedge clk) begin: _gen_refresh_cmd
	if (access_slot)
		sdram_distributor <= ~sdram_distributor;
end

parameter Z_IDLE = 0, Z_READ8 = 1, Z_READ16 = 2, Z_READ32 = 3, Z_WRITE8 = 4, Z_WRITE16 = 5, Z_WRITE32 = 6;

reg [3:0] ztate;
	
always @(posedge clk) begin: _zpupkin
	if (reset) begin
		ztate <= Z_IDLE;
	end
	else
	case (ztate)
	Z_IDLE:	case({zpu_read,zpu_write,zpu_halfword,zpu_byte})
		4'b1000:	ztate <= Z_READ32;
		4'b1010:	ztate <= Z_READ16;
		4'b1001:	ztate <= Z_READ8;
		
		4'b0100:	ztate <= Z_WRITE32;
		4'b0110:	ztate <= Z_READ16;
		4'b0101:	ztate <= Z_READ8;
		default:	ztate <= Z_IDLE;
		endcase
		
	Z_READ8:begin
		ztate <= Z_IDLE;
		end
	Z_READ16:begin
		ztate <= Z_IDLE;
		end
	Z_READ32:begin
		ztate <= Z_IDLE;
		end
		
	Z_WRITE8:begin
		ztate <= Z_IDLE;
		end
	Z_WRITE16:begin
		ztate <= Z_IDLE;
		end
	Z_WRITE32:begin
		ztate <= Z_IDLE;
		end
	endcase
	
end	
	
endmodule
