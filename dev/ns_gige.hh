/*
 * Copyright (c) 2004 The Regents of The University of Michigan
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met: redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer;
 * redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution;
 * neither the name of the copyright holders nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* @file
 * Device module for modelling the National Semiconductor
 * DP83820 ethernet controller
 */

#ifndef __DEV_NS_GIGE_HH__
#define __DEV_NS_GIGE_HH__

#include "base/inet.hh"
#include "base/statistics.hh"
#include "dev/etherint.hh"
#include "dev/etherpkt.hh"
#include "dev/io_device.hh"
#include "dev/ns_gige_reg.h"
#include "dev/pcidev.hh"
#include "mem/bus/bus.hh"
#include "sim/eventq.hh"

/**
 * Ethernet device registers
 */
struct dp_regs {
    uint32_t	command;
    uint32_t	config;
    uint32_t	mear;
    uint32_t	ptscr;
    uint32_t    isr;
    uint32_t    imr;
    uint32_t    ier;
    uint32_t    ihr;
    uint32_t    txdp;
    uint32_t    txdp_hi;
    uint32_t    txcfg;
    uint32_t    gpior;
    uint32_t    rxdp;
    uint32_t    rxdp_hi;
    uint32_t    rxcfg;
    uint32_t    pqcr;
    uint32_t    wcsr;
    uint32_t    pcr;
    uint32_t    rfcr;
    uint32_t    rfdr;
    uint32_t    srr;
    uint32_t    mibc;
    uint32_t    vrcr;
    uint32_t    vtcr;
    uint32_t    vdr;
    uint32_t    ccsr;
    uint32_t    tbicr;
    uint32_t    tbisr;
    uint32_t    tanar;
    uint32_t    tanlpar;
    uint32_t    taner;
    uint32_t    tesr;
};

struct dp_rom {
    /**
     * for perfect match memory.
     * the linux driver doesn't use any other ROM
     */
    uint8_t perfectMatch[ETH_ADDR_LEN];
};

class IntrControl;
class NSGigEInt;
class PhysicalMemory;
class BaseInterface;
class HierParams;
class Bus;
class PciConfigAll;

/**
 * NS DP82830 Ethernet device model
 */
class NSGigE : public PciDev
{
  public:
    /** Transmit State Machine states */
    enum TxState
    {
        txIdle,
        txDescRefr,
        txDescRead,
        txFifoBlock,
        txFragRead,
        txDescWrite,
        txAdvance
    };

    /** Receive State Machine States */
    enum RxState
    {
        rxIdle,
        rxDescRefr,
        rxDescRead,
        rxFifoBlock,
        rxFragWrite,
        rxDescWrite,
        rxAdvance
    };

    enum DmaState
    {
        dmaIdle,
        dmaReading,
        dmaWriting,
        dmaReadWaiting,
        dmaWriteWaiting
    };

  private:
    Addr addr;
    static const Addr size = sizeof(dp_regs);

  protected:
    typedef std::deque<PacketPtr> pktbuf_t;
    typedef pktbuf_t::iterator pktiter_t;

    /** device register file */
    dp_regs regs;
    dp_rom rom;

    /** pci settings */
    bool ioEnable;
#if 0
    bool memEnable;
    bool bmEnable;
#endif

    /*** BASIC STRUCTURES FOR TX/RX ***/
    /* Data FIFOs */
    pktbuf_t txFifo;
    uint32_t maxTxFifoSize;
    pktbuf_t rxFifo;
    uint32_t maxRxFifoSize;

    /** various helper vars */
    PacketPtr txPacket;
    PacketPtr rxPacket;
    uint8_t *txPacketBufPtr;
    uint8_t *rxPacketBufPtr;
    uint32_t txXferLen;
    uint32_t rxXferLen;
    bool rxDmaFree;
    bool txDmaFree;

    /** DescCaches */
    ns_desc txDescCache;
    ns_desc rxDescCache;

    /* tx State Machine */
    TxState txState;
    bool txEnable;

    /** Current Transmit Descriptor Done */
    bool CTDD;
    /** current amt of free space in txDataFifo in bytes */
    uint32_t txFifoAvail;
    /** halt the tx state machine after next packet */
    bool txHalt;
    /** ptr to the next byte in the current fragment */
    Addr txFragPtr;
    /** count of bytes remaining in the current descriptor */
    uint32_t txDescCnt;
    DmaState txDmaState;

    /** rx State Machine */
    RxState rxState;
    bool rxEnable;

    /** Current Receive Descriptor Done */
    bool CRDD;
    /** num of bytes in the current packet being drained from rxDataFifo */
    uint32_t rxPktBytes;
    /** number of bytes in the rxFifo */
    uint32_t rxFifoCnt;
    /** halt the rx state machine after current packet */
    bool rxHalt;
    /** ptr to the next byte in current fragment */
    Addr rxFragPtr;
    /** count of bytes remaining in the current descriptor */
    uint32_t rxDescCnt;
    DmaState rxDmaState;

    bool extstsEnable;

  protected:
    Tick dmaReadDelay;
    Tick dmaWriteDelay;

    Tick dmaReadFactor;
    Tick dmaWriteFactor;

    void *rxDmaData;
    Addr  rxDmaAddr;
    int   rxDmaLen;
    bool  doRxDmaRead();
    bool  doRxDmaWrite();
    void  rxDmaReadCopy();
    void  rxDmaWriteCopy();

    void *txDmaData;
    Addr  txDmaAddr;
    int   txDmaLen;
    bool  doTxDmaRead();
    bool  doTxDmaWrite();
    void  txDmaReadCopy();
    void  txDmaWriteCopy();

    void rxDmaReadDone();
    friend class EventWrapper<NSGigE, &NSGigE::rxDmaReadDone>;
    EventWrapper<NSGigE, &NSGigE::rxDmaReadDone> rxDmaReadEvent;

    void rxDmaWriteDone();
    friend class EventWrapper<NSGigE, &NSGigE::rxDmaWriteDone>;
    EventWrapper<NSGigE, &NSGigE::rxDmaWriteDone> rxDmaWriteEvent;

    void txDmaReadDone();
    friend class EventWrapper<NSGigE, &NSGigE::txDmaReadDone>;
    EventWrapper<NSGigE, &NSGigE::txDmaReadDone> txDmaReadEvent;

    void txDmaWriteDone();
    friend class EventWrapper<NSGigE, &NSGigE::txDmaWriteDone>;
    EventWrapper<NSGigE, &NSGigE::txDmaWriteDone> txDmaWriteEvent;

    bool dmaDescFree;
    bool dmaDataFree;


  protected:
    Tick txDelay;
    Tick rxDelay;

    void txReset();
    void rxReset();
    void regsReset();

    void rxKick();
    Tick rxKickTick;
    typedef EventWrapper<NSGigE, &NSGigE::rxKick> RxKickEvent;
    friend class RxKickEvent;

    void txKick();
    Tick txKickTick;
    typedef EventWrapper<NSGigE, &NSGigE::txKick> TxKickEvent;
    friend class TxKickEvent;

    /**
     * Retransmit event
     */
    void transmit();
    void txEventTransmit()
    {
        transmit();
        if (txState == txFifoBlock)
            txKick();
    }
    typedef EventWrapper<NSGigE, &NSGigE::txEventTransmit> TxEvent;
    friend class TxEvent;
    TxEvent txEvent;

    void txDump() const;
    void rxDump() const;

    /**
     * receive address filter
     */
    bool rxFilterEnable;
    bool rxFilter(PacketPtr &packet);
    bool acceptBroadcast;
    bool acceptMulticast;
    bool acceptUnicast;
    bool acceptPerfect;
    bool acceptArp;

    PhysicalMemory *physmem;

    /**
     * Interrupt management
     */
    IntrControl *intctrl;
    void devIntrPost(uint32_t interrupts);
    void devIntrClear(uint32_t interrupts);
    void devIntrChangeMask();

    Tick intrDelay;
    Tick intrTick;
    bool cpuPendingIntr;
    void cpuIntrPost(Tick when);
    void cpuInterrupt();
    void cpuIntrClear();

    typedef EventWrapper<NSGigE, &NSGigE::cpuInterrupt> IntrEvent;
    friend class IntrEvent;
    IntrEvent *intrEvent;
    NSGigEInt *interface;

  public:
    struct Params : public PciDev::Params
    {
        PhysicalMemory *pmem;
        HierParams *hier;
        Bus *header_bus;
        Bus *payload_bus;
        Tick intr_delay;
        Tick tx_delay;
        Tick rx_delay;
        Tick pio_latency;
        bool dma_desc_free;
        bool dma_data_free;
        Tick dma_read_delay;
        Tick dma_write_delay;
        Tick dma_read_factor;
        Tick dma_write_factor;
        bool rx_filter;
        Net::EthAddr eaddr;
        uint32_t tx_fifo_size;
        uint32_t rx_fifo_size;
    };

    NSGigE(Params *params);
    ~NSGigE();
    const Params *params() const { return (const Params *)_params; }

    virtual void WriteConfig(int offset, int size, uint32_t data);
    virtual void ReadConfig(int offset, int size, uint8_t *data);

    virtual Fault read(MemReqPtr &req, uint8_t *data);
    virtual Fault write(MemReqPtr &req, const uint8_t *data);

    bool cpuIntrPending() const;
    void cpuIntrAck() { cpuIntrClear(); }

    bool recvPacket(PacketPtr &packet);
    void transferDone();

    void setInterface(NSGigEInt *i) { assert(!interface); interface = i; }

    virtual void serialize(std::ostream &os);
    virtual void unserialize(Checkpoint *cp, const std::string &section);

  public:
    void regStats();

  private:
    Stats::Scalar<> txBytes;
    Stats::Scalar<> rxBytes;
    Stats::Scalar<> txPackets;
    Stats::Scalar<> rxPackets;
    Stats::Scalar<> txIpChecksums;
    Stats::Scalar<> rxIpChecksums;
    Stats::Scalar<> txTcpChecksums;
    Stats::Scalar<> rxTcpChecksums;
    Stats::Scalar<> txUdpChecksums;
    Stats::Scalar<> rxUdpChecksums;
    Stats::Scalar<> descDmaReads;
    Stats::Scalar<> descDmaWrites;
    Stats::Scalar<> descDmaRdBytes;
    Stats::Scalar<> descDmaWrBytes;
    Stats::Formula txBandwidth;
    Stats::Formula rxBandwidth;
    Stats::Formula txPacketRate;
    Stats::Formula rxPacketRate;

  public:
    Tick cacheAccess(MemReqPtr &req);
};

/*
 * Ethernet Interface for an Ethernet Device
 */
class NSGigEInt : public EtherInt
{
  private:
    NSGigE *dev;

  public:
    NSGigEInt(const std::string &name, NSGigE *d)
        : EtherInt(name), dev(d) { dev->setInterface(this); }

    virtual bool recvPacket(PacketPtr &pkt) { return dev->recvPacket(pkt); }
    virtual void sendDone() { dev->transferDone(); }
};

#endif // __DEV_NS_GIGE_HH__
