TARGET=floppy_testbench

BASE_DIR=../../src
FLOPPY_DIR=$(BASE_DIR)/floppy $(BASE_DIR)/floppy/verilog-6502 
TOP=$(TARGET)

IVINC=$(addprefix -I,$(FLOPPY_DIR))

IVERILOG=iverilog $(IVINC) -DSIMULATION #-DSIM
VVP=vvp
GTKWAVE=gtkwave

FLOPPY_SRC=$(foreach sdir,$(FLOPPY_DIR),$(wildcard $(sdir)/*.v))

OTHER_SRC=$(BASE_DIR)/uart.v $(BASE_DIR)/ram.v \
	  $(BASE_DIR)/sdram/SDRAM_Controller.v \
	  $(BASE_DIR)/sdram_arbitre.v

SRC=$(TARGET).v $(FLOPPY_SRC) $(SDCARD_SRC) $(OTHER_SRC)

all:	$(TARGET).vcd

wave:	$(TARGET).vcd
	$(GTKWAVE) $^ &

$(TARGET).vcd:	$(TARGET).vvp
	$(VVP) $^

$(TARGET).vvp:	$(SRC)
	$(IVERILOG) -o $@ -s $(TOP) $^
