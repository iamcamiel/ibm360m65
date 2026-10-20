// Full generated CPU regression. Storage is a private, big-endian 8 MiB adapter.
// The runner extracts the production instruction-exit predicate into exit.inc.
#include "360_struc.h"
#include <cstdio>
#include <cstdlib>
#include <cstdint>
extern "C" { double runtime=0; void logmsg(char*,...) {} }
static unsigned char memory[8388608], keys[4096];
static unsigned cycle=0, seq=0, reads=0, writes=0;
static unsigned exit_cc=99, exit_ros=0, exit_ic=0, exit_count=0;
static uint64_t exit_data=0;
static bool skip_exit=false, can_exit=false, delayed_exit=false;
static uint32_t read32(unsigned a) {
    return (uint32_t(memory[a])<<24)|(uint32_t(memory[a+1])<<16)|
           (uint32_t(memory[a+2])<<8)|memory[a+3];
}
static void put32(unsigned a, uint32_t v) {
    for(int i=0;i<4;++i) memory[a+i]=(v>>(24-8*i))&255;
}
static void storage() {
    unsigned cmd=oldstate.WA.reg_se_cmd.F, a=oldstate.WA.reg_se_addr.F;
    unsigned base=a&0x7ffff8u;
    if(!((cmd^seq)&0xc0000000u)) return;
    seq=cmd&0xc0000000u; unsigned response=seq;
    if(!(cmd&8)) {
        if(cmd&0x10) {
            newstate.EXTERNAL_.reg_se_rdata_hi.F=read32(base);
            newstate.EXTERNAL_.reg_se_rdata_lo.F=read32(base+4);
            for(int i=0;i<8;++i) if(a&(0x80000000u>>i)) memory[base+i]=255;
        } else if(cmd&1) {
            unsigned hi=oldstate.WA.reg_se_wdata_hi.F, lo=oldstate.WA.reg_se_wdata_lo.F;
            for(int i=0;i<4;++i) {
                if(a&(0x80000000u>>i)) memory[base+i]=(hi>>(24-8*i))&255;
                if(a&(0x08000000u>>i)) memory[base+4+i]=(lo>>(24-8*i))&255;
            }
            ++writes;
        } else if(cmd&2) keys[(a&0x7ff800u)>>11]=(cmd>>22)&0xf8;
        else if(cmd&4) response|=(unsigned(keys[(a&0x7ff800u)>>11])<<22)|0x01000000u;
        else {
            newstate.EXTERNAL_.reg_se_rdata_hi.F=read32(base);
            newstate.EXTERNAL_.reg_se_rdata_lo.F=read32(base+4); ++reads;
        }
    }
    newstate.EXTERNAL_.reg_se_resp.F=response;
}
static void tick() { ++cycle; single_cycle(); }
static void twenty() {
    for(int i=0;i<20&&!m65_fault_pending();++i) { tick(); if(i>=17) storage(); }
    for(int i=0;i<6&&newstate.RX.rosar.F==0&&!m65_fault_pending();++i) tick();
    newstate.EXTERNAL_.reg_io_resp.F=oldstate.WA.reg_io_cmd.F&0xc0000000u;
}
static bool instruction_exit() {
#include "exit.inc"
    can_exit=true;
    return false;
}
int main(int argc,char**argv) {
    if(argc!=6) return 3;
    unsigned count=strtoul(argv[1],0,10), hi=strtoul(argv[2],0,16);
    unsigned lo=strtoul(argv[3],0,16), context=strtoul(argv[4],0,10);
    unsigned initialcc=strtoul(argv[5],0,10), base=context?0x7553d8:0x100;
    _putenv_s("M65_INTERVAL_TIMER","0"); lf=fopen("m65.log","w");
    put32(0,0x00040000|(initialcc<<12)); put32(4,base);
    unsigned pc=base;
    if(context) {
        unsigned regs[16]={0x01000c30,0x007803d0,0x00751bb0,0x0075551c,
            hi,0xc30,0x007827e0,0x007803d0,0xf,0x00783d38,0x0078165e,
            0x4075492e,0x00781580,0x007816e8,0x80754ce2,lo};
        for(unsigned i=0;i<16;++i) put32(0x300+i*4,regs[i]);
        put32(pc,0x980f0300); pc+=4; // LM R0,R15,300
        put32(pc,0x5870c0d4); pc+=4; // L R7,0D4(R12)
        memory[pc++]=0x18; memory[pc++]=0xe4; // LR R14,R4
        put32(0x781654,0x007803d0);
    } else {
        put32(pc,0x98ef0300); pc+=4; // LM R14,R15,300
        put32(0x300,hi); put32(0x304,lo);
    }
    put32(pc,0x8ee00000|count); pc+=4; // SRDA R14,count
    put32(pc,0x90ef0308); pc+=4; // STM R14,R15,308
    put32(pc,0x82000180); put32(0x180,0x00060000); put32(0x184,pc);
    full_init(); newstate.EXTERNAL_.reg_se_size.F=8388607;
    for(unsigned i=0;i<M65_NATIVE_STARTUP_CYCLES/20;++i) twenty();
    newstate.EXTERNAL_.switches_0.F|=0xffe000u;
    for(int i=0;i<12;++i) newstate.EXTERNAL_.switches_0.F&=~(((0x350u>>i)&1u)<<(23-i));
    newstate.EXTERNAL_.switches_0.B14=true;
    for(int i=0;i<50;++i) twenty();
    newstate.EXTERNAL_.switches_0.B14=false;
    unsigned waits=0;
    for(unsigned i=0;i<35000&&!m65_fault_pending();++i) {
        twenty();
        if(instruction_exit()) {
            if(newstate.RE.e_bit.F==0x8ee0) {
                ++exit_count;
                exit_cc=(newstate.RW.psw_bit.B34<<1)|newstate.RW.psw_bit.B35;
                exit_data=(uint64_t(newstate.ls_mem[14].F)<<32)|newstate.ls_mem[15].F;
                exit_ros=newstate.RX.rosar.F; exit_ic=newstate.CA.ic.F;
            }
            skip_exit=can_exit=delayed_exit=false;
        }
        if(newstate.RW.psw_bit.B14&&newstate.RX.rosar.F==0x784) {
            if(++waits==32) break;
        } else waits=0;
    }
    printf("{\"exit_cc\":%u,\"exit_data\":\"%016llx\",\"exit_count\":%u,"
           "\"exit_ros\":%u,\"exit_ic\":%u,\"stored\":\"%08x%08x\","
           "\"wait\":%s,\"fault\":%d,\"reads\":%u,\"writes\":%u,\"cycles\":%u,"
           "\"checks\":[%u,%u,%u,%u]}\n",exit_cc,exit_data,exit_count,exit_ros,exit_ic,
           read32(0x308),read32(0x30c),waits==32?"true":"false",m65_fault_pending(),
           reads,writes,cycle,unsigned(newstate.MB._sdbi_parity_check),
           unsigned(newstate.BF._local_store_bus_parity_check),
           unsigned(newstate.AS._sadder_full_sum_error),unsigned(newstate.AS._sadder_half_sum_error));
    fclose(lf); return m65_fault_pending()?2:0;
}
