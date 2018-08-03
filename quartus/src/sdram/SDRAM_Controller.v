// Dmitry Tselikov (b2m) http://bashkiria-2m.narod.ru/
// Modified by Ivan Gorodetsky 2014-2015 http://sensi.org/~retrocomp
// Modified by Viacheslav Slavinsky 2018 http://sensi.org/~svo

// Example timings
// refresh: 4096 times every Tref=64ms (or once every 64ms/4096 = 15us)
// refresh state holds for: Trc=60ns

// 90MHz (tc=11ns): 15us = 15000/11ns 1363 cycles min between refresh 
// states = 60/11 = 5.45 refresh states
// 114MHz (tc=9ns): 60/9 = 6.66 refresh states
// 120MHz (tc=8ns): 60/8 = 7.5 refresh states
// 72MHz (tc=13ns): 60/13 = 4.6 refresh states

module SDRAM_Controller(
	input			clk,
	input			reset,
	inout	[15:0]		DRAM_DQ,
	output	reg[11:0]	DRAM_ADDR,
	output	reg		DRAM_LDQM,
	output	reg		DRAM_UDQM,
	output	reg		DRAM_WE_N,
	output	reg		DRAM_CAS_N,
	output	reg		DRAM_RAS_N,
	output			DRAM_CS_N,
	output			DRAM_BA_0,
	output			DRAM_BA_1,
	input	[21:0]		iaddr,
	input	[15:0]		dataw,
	input			rd,
	input			we_n,
	input ilb_n,
	input iub_n,
	output reg [15:0]	datar,
	output reg membusy,
	input			refresh
);

parameter ST_RESET0 = 0,
	ST_RESET1 = 1,
	ST_IDLE   = 2,
	ST_RAS0   = 3,
	ST_RAS1   = 4,
	ST_READ0  = 5,
	ST_READ1  = 6,
	ST_READ2  = 7,
	ST_WRITE0 = 8,
	ST_WRITE1 = 9,
	ST_WRITE2 = 10,
	ST_REFRESH0 = 11,
	ST_REFRESH1 = 12,
	ST_REFRESH2 = 13,
	ST_REFRESH3 = 14,
	ST_REFRESH4 = 17,
	ST_REFRESH5 = 18,
	ST_REFRESH6 = 19,
	ST_REFRESH7 = 20;

reg[4:0] state;
reg[9:0] refreshcnt;
reg[21:0] addr;
reg[15:0] odata;
reg refreshflg;
reg lb_n,ub_n;

assign DRAM_DQ = state == ST_WRITE0 ? odata : 16'bZZZZZZZZZZZZZZZZ;

assign DRAM_CS_N = reset;
assign DRAM_BA_0 = addr[20];
assign DRAM_BA_1 = addr[21];

always @(*) begin
	case (state)
	ST_RESET0:   DRAM_ADDR = 12'b100000;				// CAS latency 2
	ST_RAS0:     DRAM_ADDR = addr[19:8];
	ST_READ0:    DRAM_ADDR =  {4'b0100,addr[7:0]};			// A10=1 AUTO PRECHARGE ENABLE
	ST_WRITE0:   DRAM_ADDR = {4'b0100,addr[7:0]};			// A10=1 AUTO PRECHARGE ENABLE
	endcase
	case (state)
        ST_RESET0:begin
            {DRAM_RAS_N,DRAM_CAS_N,DRAM_WE_N} = 3'b000;	// CMD_LOAD_MODE_REGISTER
            //{DRAM_RAS_N,DRAM_CAS_N,DRAM_WE_N,DRAM_UDQM,DRAM_LDQM} = 5'b11111;
            end
	ST_RAS0:     {DRAM_RAS_N,DRAM_CAS_N,DRAM_WE_N} = 3'b011;	// CMD_ACTIVE
	ST_READ0:    {DRAM_RAS_N,DRAM_CAS_N,DRAM_WE_N,DRAM_UDQM,DRAM_LDQM} = 5'b10100;	// CMD_READ
	ST_WRITE0:   {DRAM_RAS_N,DRAM_CAS_N,DRAM_WE_N,DRAM_UDQM,DRAM_LDQM} = {3'b100,ub_n,lb_n}; // CMD_WRITE
	ST_WRITE2:   {DRAM_UDQM,DRAM_LDQM} = 2'b00;
	ST_REFRESH0: {DRAM_RAS_N,DRAM_CAS_N,DRAM_WE_N} = 3'b001;	// CMD_REFRESH
	default:     {DRAM_RAS_N,DRAM_CAS_N,DRAM_WE_N} = 3'b111;	// CMD_NOP
	endcase
end

//wire refresh_cond = {refreshcnt[9]!=refreshflg;

reg refresh_sync;
wire refresh_cond = refresh & ~refresh_sync;
wire membusy_idle = refresh_cond | rd | ~we_n;


always @(posedge clk) begin
	if (reset) begin
		membusy <= 1'b0;
		refresh_sync <= 1'b0;
                datar <= 0;
                {rd_r, we_n_r} = 2'b01;		
	end 
	else 	
	begin
		refresh_sync <= refresh;
		
		case(state) 
		ST_IDLE:begin
			membusy <= membusy_idle;
			{addr[21:0],odata,ub_n,lb_n}<={iaddr[21:0],dataw,iub_n,ilb_n};
                        {rd_r, we_n_r} <= {rd, we_n};
			end
		ST_READ2:
			begin
			if (lb_n==1'b0) datar[7:0] <= DRAM_DQ[7:0];
			if (ub_n==1'b0) datar[15:8] <= DRAM_DQ[15:8];
			end
	//	ST_REFRESH0:
	//	ST_REFRESH6:
	//		/* Clear membusy early so that the next ST_IDLE could accept commands */
	//		membusy <= 1'b0;
		endcase
	end
end

reg rd_r, we_n_r;

always @(posedge clk) begin
        if (reset) begin
		state <= ST_RESET0; 
        end
	else 
	case (state)
	ST_RESET0: state <= ST_RESET1;
	ST_RESET1: state <= ST_IDLE;
	ST_IDLE: state <= (rd | ~we_n) ? ST_RAS0 : refresh_cond ? ST_REFRESH0 : ST_IDLE;
	ST_RAS0: state <= ST_RAS1;
	ST_RAS1:casex ({rd_r,~we_n_r})
		2'b10: state <= ST_READ0; 
		2'b01: state <= ST_WRITE0;
		default: state <= ST_IDLE;
		endcase
	ST_READ0: state <= ST_READ1;
	ST_READ1: state <= ST_READ2;
	ST_READ2: state<=ST_IDLE;
	ST_WRITE0: state <= ST_WRITE1;
	ST_WRITE1: state <= ST_WRITE2;
	ST_WRITE2: state <= ST_IDLE;
	ST_REFRESH0: state <= ST_REFRESH1;
	ST_REFRESH1: state <= ST_REFRESH2;
	ST_REFRESH2: state <= ST_REFRESH3;
	ST_REFRESH3: state <= ST_REFRESH4;
	ST_REFRESH4: state <= ST_REFRESH5;
	ST_REFRESH5: state <= ST_REFRESH6;		
	ST_REFRESH6: state <= ST_REFRESH7;	// for 96,100,105MHz 6->IDLE
	ST_REFRESH7: state <= ST_IDLE;  	// for 114,120 MHz
	default: state <= ST_IDLE;
	endcase
end
	
//reg		wr_req_flag;		/* write request posted flag */
//reg		rd_req_flag;		/* read request posted flag */
//reg		wr_done_flag;		/* write request complete flag */
//reg		rd_done_flag;		/* read request complete flag */
//
//always @(posedge clk) begin
//	if (reset)
//		{wr_req_flag,rd_req_flag,wr_done_flag,rd_done_flag} = 4'b0000;
//	else begin
//		case (state)
//		ST_RAS1:	{wr_req_flag,rd_req_flag,wr_done_flag,rd_done_flag} <= 4'b0000;	// a request is being served, drop all flags
//		ST_WRITE2:	wr_done_flag <= 1'b1;
//		ST_READ2:	rd_done_flag <= 1'b1;
//		default:	begin
//				/* register a new request */
//				wr_req_flag <= wr_req_flag | ~we_n;
//				rd_req_flag <= rd_req_flag | rd;
//				end				
//		endcase
//	end
//end
	
	
	
endmodule
