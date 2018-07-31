TARGET=purifier_testbench

BASE_DIR=../../src
TOP=$(TARGET)

IVERILOG=iverilog $(IVINC) -DSIMULATION #-DSIM
VVP=vvp
GTKWAVE=gtkwave

OTHER_SRC=$(BASE_DIR)/vu_bus.v

SRC=$(TARGET).v $(OTHER_SRC)

all:	$(TARGET).vcd

wave:	$(TARGET).vcd
	$(GTKWAVE) $^ &

$(TARGET).vcd:	$(TARGET).vvp
	$(VVP) $^

$(TARGET).vvp:	$(SRC)
	$(IVERILOG) -o $@ -s $(TOP) $^
