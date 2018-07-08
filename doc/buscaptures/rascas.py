# Test v06c bus multiplexing into ras/cas:
# forward 
MAP_RAS = [9,10,11,12,13,1,2,22]
MAP_CAS = [3,4,5,6,7,8,16,23]

# busses start with 1, so invalid value at [0]
def abus(acpu): return ['#'] + acpu[:16] + [int(not(x)) for x in acpu[8:]]

# hex value to list of bus wires, right-to-left, msb right
# so that hex2bus(addr) -> a[0], a[1]... etc
def hex2bus(n): return [int(x) for x in ('0'*16 + bin(n)[2:])[-16:]][::-1]


# (d2,d6) of muxes d11,d12,d13,d14
def m1(b): return [b[n] for n in MAP_RAS]

# (d3,d7) of muxed d11,d12,d13,d14
def m2(b): return [b[n] for n in MAP_CAS]

def m2hex(m): return hex(eval('0b' + "".join([str(c) for c in reversed(m)])))

# Bus 2: wires 1..16 = cpu_abus, wires 17..24 = ~cpu_abus[8..16]
def printrascas(address):
    b2=abus(hex2bus(address))
    #print 'b2 looks like this:', b2
    #print 'm1 is ', m1(b2), 'm2 is ', m2(b2)
    return m2hex(m1(b2)), m2hex(m2(b2))
    
print 'Expect to see these RAS/CAS values on SCHAPP for A=0x1234:'
ras,cas = printrascas(0xa000)
print ras,cas

# doing the inverse
def rascas2adrs(ras,cas):
    # get the bits for easy shuffling
    m = hex2bus((ras << 8) | cas)
    # forward transform: m1[0] = b[9]
    # inverse transform: b[9] = m1[0]
    m2,m1 = m[:8],m[8:]
    #print 'reverse m1: ', m1, 'reverse m2: ', m2

    b = ['#']*24
    b[9] = m1[0]
    for i,busn in enumerate(MAP_RAS):
        if busn > 16:
            b[busn-8] = int(not(m1[i]))
            print "b[%d]=~ra[%d]" % (busn-8-1,i)
        else:
            b[busn] = m1[i]
            print "b[%d]=ra[%d]" % (busn-1,i)

    for i,busn in enumerate(MAP_CAS):
        if busn > 16:
            b[busn-8] = int(not(m2[i]))
            print "b[%d]=~ca[%d]" % (busn-8-1,i)
        else:
            print "b[%d]=ca[%d]" % (busn-1,i)
            b[busn] = m2[i]

    return m2hex(b[9:17]),m2hex(b[1:9])

ras=eval(ras)
cas=eval(cas)

rev_ah,rev_al = rascas2adrs(ras,cas)
print 'Reverse ah,al for same ras,cas=', rev_ah, rev_al
