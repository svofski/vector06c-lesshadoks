`default_nettype none

module sdram_arbitre #(parameter VU_ABUS_WIDTH=18, DISK_ABUS_WIDTH=23) (
	input clk, input reset,
	
	input [VU_ABUS_WIDTH-1:0] vu_adrs,
	input [7:0] vu_data_i,
        output [7:0] vu_data_o,
	input vu_write,
	input vu_read,
	input access_slot,
	
	input [DISK_ABUS_WIDTH-1:0] disk_adrs,
	input [31:0] disk_data_i,
        output [31:0] disk_data_o,
	input disk_write,
	input disk_read,
	input disk_halfword,
	input disk_byte,
	output disk_ram_busy,
	
	output [22:0] sdram_addr,
	output sdram_read,
	output sdram_write,
	output sdram_lb,
	output sdram_ub,
	output sdram_refresh,
	output [15:0] sdram_do,
        input  [15:0] sdram_di,
        input sdram_busy);
	

wire [17:0] kvaz_sdram_addr = vu_adrs[17:1];
wire kvaz_lb = vu_adrs[0];
wire kvaz_ub = ~vu_adrs[0];
wire [15:0] kvaz_data_to_sdram = {vu_data_i,vu_data_i};
assign vu_data_o = kvaz_lb ? sdram_di[7:0] : sdram_di[15:8];

reg [15:0] disk_data_to_sdram;
reg [DISK_ABUS_WIDTH-1:0] disk_sdram_addr;
reg zpu_lb;
reg zpu_ub;
reg zpu_sdram_read;
reg zpu_sdram_write;
	
assign sdram_addr = zpu_e ? disk_sdram_addr : kvaz_sdram_addr;
assign sdram_lb = zpu_e ? zpu_lb : kvaz_lb;
assign sdram_ub = zpu_e ? zpu_ub : kvaz_ub;
assign sdram_do = zpu_e ? disk_data_to_sdram : kvaz_data_to_sdram;
assign sdram_read = zpu_e ? zpu_sdram_read : vu_read;
assign sdram_write = zpu_e ? zpu_sdram_write : vu_write;

wire [7:0] disk8o = zpu_lb ? sdram_di[7:0] : sdram_di[15:8];
assign disk_data_o = disk8o;

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
wire zpu_e = ~sdram_distributor & access_slot;
always @(posedge clk) begin: _gen_refresh_cmd
    if (reset) 
        sdram_distributor <= 0;
    else if (access_slot)
	sdram_distributor <= ~sdram_distributor;
end

parameter Z_IDLE = 0, Z_READ8 = 1, Z_READ16 = 2, Z_READ32 = 3, 
    Z_WRITE8 = 4, 
    Z_WRITE16 = 5, 

    Z_WRITEHW2 = 15,
    Z_WRITEDW1 = 16,
    Z_WRITEDW2 = 17,

    Z_WRITEDW_B12 = 18,
    Z_WRITEDW_B3 = 19,

    Z_FINISH = 30,
    Z_WAIT = 31
    ;

reg [4:0] ztate;
reg [4:0] ztate_next;
//assign disk_ram_busy = ztate != Z_IDLE;
// ensure that the busy flag is set instantaneously, not just on the next
// cycle
assign disk_ram_busy = ((ztate != Z_IDLE) | (disk_read|disk_write));
//    (ztate != Z_END);
//reg disk_ram_busy_r = 0;
//assign disk_ram_busy = disk_ram_busy_r | ((disk_read|disk_write)&ztate==Z_IDLE);

always @(posedge clk) begin: _zpupkin
	if (reset) begin
		ztate <= Z_IDLE;
                ztate_next <= Z_IDLE;
                disk_sdram_addr <= 0;
                {zpu_sdram_write,zpu_sdram_read} = 2'b00;
	end
	else
	case (ztate)
	Z_IDLE:	begin
                disk_sdram_addr <= disk_adrs[21:1];
                casez({disk_read,disk_write,disk_halfword,disk_byte,disk_adrs[0]})
		5'b10000:	ztate <= Z_READ32;
		5'b10100:	ztate <= Z_READ16;

		5'b1001?:
                    begin
                        // byte read
                        ztate_next <= Z_FINISH;
                        ztate <= zpu_e ? Z_FINISH : Z_WAIT;
                        {zpu_ub,zpu_lb} <= {~disk_adrs[0],disk_adrs[0]};
                        {zpu_sdram_write,zpu_sdram_read} <= 2'b01;
                    end
		
                5'b01000:	// word-aligned dword write
                    begin 
                    ztate_next <= Z_WRITEDW1;
                    ztate <= zpu_e ? Z_WRITEDW1 : Z_WAIT;

                    disk_data_to_sdram <= disk_data_i[31:16];
                    {zpu_ub,zpu_lb} <= 2'b11;
                    {zpu_sdram_write,zpu_sdram_read} <= 2'b10;
                    end
                5'b01001:       // misaligned dword write
                    begin
                        ztate_next <= Z_WRITEDW_B12;
                        ztate <= zpu_e ? Z_WRITEDW_B12 : Z_WAIT;

                        disk_data_to_sdram <= {8'b0,disk_data_i[31:24]};
                        {zpu_ub,zpu_lb} <= 2'b01;
                        {zpu_sdram_write,zpu_sdram_read} <= 2'b10;
                    end
                5'b0101?:   // byte write
                    begin
                        ztate_next <= Z_FINISH;
                        ztate <= zpu_e ? Z_FINISH : Z_WAIT;
                        disk_data_to_sdram <= {disk_data_i[7:0],disk_data_i[7:0]};
                        {zpu_ub,zpu_lb} <= {~disk_adrs[0],disk_adrs[0]};
                        {zpu_sdram_write,zpu_sdram_read} <= 2'b10;
                    end
                5'b01100:   // word-aligned word write
                    begin
                        ztate_next <= Z_FINISH;
                        ztate <= zpu_e ? Z_FINISH : Z_WAIT;
                        disk_data_to_sdram <= disk_data_i[15:0];
                        {zpu_ub,zpu_lb} <= 2'b11;
                        {zpu_sdram_write,zpu_sdram_read} <= 2'b10;
                    end
                5'b01101:   // misaligned word write
                    begin
                        ztate_next <= Z_WRITEHW2;
                        ztate <= zpu_e ? Z_WRITEHW2 : Z_WAIT;
                        disk_data_to_sdram <= {disk_data_i[15:8],disk_data_i[15:8]};
                        {zpu_ub,zpu_lb} <= 2'b01;
                        {zpu_sdram_write,zpu_sdram_read} <= 2'b10;
                    end

		default:	ztate <= Z_IDLE;
		endcase
                end
        Z_WAIT: if (zpu_e) ztate <= ztate_next;

        Z_WRITEHW2: 
            if (~sdram_busy) begin
                ztate_next <= Z_FINISH;
                ztate <= zpu_e ? Z_FINISH : Z_WAIT;
                disk_data_to_sdram <= {disk_data_i[7:0],disk_data_i[7:0]};
                {zpu_ub,zpu_lb} <= 2'b10;
                {zpu_sdram_write,zpu_sdram_read} <= 2'b10;
            end
		
	Z_WRITEDW1:
            if (~sdram_busy) begin
                ztate_next <= Z_FINISH;
                ztate <= zpu_e ? Z_FINISH : Z_WAIT;
                disk_data_to_sdram <= disk_data_i[15:0];
                disk_sdram_addr <= disk_adrs[21:1] + 1;
                {zpu_sdram_write,zpu_sdram_read} = 2'b10;
            end
        Z_FINISH: 
            begin
                {zpu_sdram_write,zpu_sdram_read} = 2'b00;
                if (~sdram_busy) ztate <= Z_IDLE;
            end

        Z_WRITEDW_B12:
            if (~sdram_busy) begin // wait for first write to finish
                // write the word in the middle, bytes 1,2
                // next state: write last byte, B3
                ztate_next <= Z_WRITEDW_B3;
                ztate <= zpu_e ? Z_WRITEDW_B3 : Z_WAIT;
                disk_data_to_sdram <= disk_data_i[23:8];
                disk_sdram_addr <= disk_adrs[21:1] + 1;
                {zpu_ub,zpu_lb} <= 2'b11;
            end
        Z_WRITEDW_B3:
            if (~sdram_busy) begin
                ztate_next <= Z_FINISH;
                ztate <= zpu_e ? Z_FINISH : Z_WAIT;

                disk_data_to_sdram <= {disk_data_i[7:0],8'b0};
                disk_sdram_addr <= disk_adrs[21:1] + 2;
                {zpu_ub,zpu_lb} <= 2'b10;
            end
	endcase
end	
	
endmodule
