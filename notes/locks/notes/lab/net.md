# Registers of E1000

**IMS**(offset 0x00D0, Interrupt Mask Set)
32 bit register, every bit is corresponding to an interrupt even.

```c
regs[E1000_IMS] = (1 << 7);
```

enable receive interrupt.

**Write 1 to IMS enable corresponding interruption, but write 0 to IMS do not disable corresponding interruption. Use IMC(Interrupt Mask Clear) to disable interruption**

----

**CTL**(offset 0x0000, Device Control Register)

32 bit register.

```c
regs[E1000_CTL] |= E1000_CTL_RST;
```

set bit 26 of CTL to 1, E1000 hardware inspect that `RST == 1`, it will do Global Reset. It would be clear to 0, after reset.

----

## Complete `e1000_transmit()`

1. Transmit code must place a pointer to the packet data in a descriptor in the TX ring.

2. Need to ensure that each buffer is passed to `kfree()`, after the E1000 has finished trasmitting the packet(E1000 sets the E1000_TXD_STAT_DD bit in the descriptor to indicate this).

How to do

1. Check if the descriptor in `tx_ring[regs[E1000_TDT]]` is in used. If `tx_ring[E1000_TDT].status & E1000_TXD_STAT_DD == 0`, which means `tx_ring[E1000_TDT]` is not processed, the rx_ing is full.

2. If not full, `kfree()` original buf, write Descriptor with new buffer, `addr`, `cmd`.


## Complete `e1000_recv()`

done

```c
rx_ring[i].addr = (uint64) kalloc();
```

This is the buffer written into, when receiving a packet.

A packet can occupy more than one buffer, 

```txt
Descriptor 0: First part    DD=1, EOP=0
Descriptor 1: Middle part   DD=1, EOP=0
Descriptor 2: Last part     DD=1, EOP=1
```

RX Descriptor: 16bytes

```txt
Buffer Address(64bits, the address of buffer), Length(2bytes, bytes written into this buffer), Checksum(2bytes, ), status(1byte, describing status of written...), errors(1byte, error info of data package), special(2bytes, VLAN...) 
```

**DD-Descriptor Done, bit 0**

Hardware has finished processing of this fd, and write result into memo (Descript).

**EOP-End of Packet, bit 1**

This fd's data is corresponding last part of a packet.

## UDP Receive

### recv(short dport, int *src, short \*sport, char \*buf, int maxlen)

Return the payload of a UDP packet that arrives with destination port dprot.

1. `recv()` should return right away with the earliest waiting packet, if one or more packets arrived before the call to `rec()` 

2. If no packet are waiting, `recv()` should wait until a packet for dprot arrives. 

3. `recv()` should see arriving packets for a given prot in arrival order.

4. `recv()` copies the packet's 32-bit source IP address to *src, copies the packet's 16-bit UDP source port number to *sport, copies at most maxlen bytes of the packet's UDP payload to buf, and removes the packet from the queue
