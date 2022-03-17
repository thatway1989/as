/**
 * AS - the open source Automotive Software on https://github.com/parai
 *
 * Copyright (C) 2017  AS <parai@foxmail.com>
 *
 * This source code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by the
 * Free Software Foundation; See <http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt>.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 */
#ifdef USE_PCI
/* ============================ [ INCLUDES  ] ====================================================== */
#include "Os.h"
#include "pci_core.h"
#include "asdebug.h"
#include "Bsd.h"
#ifdef USE_UIP
#include "contiki-net.h"
#include "net/ip/uip.h"
#include "net/ip/uipopt.h"
#endif
#ifdef USE_LWIP
#include "lwip/opt.h"
#include "lwip/arch.h"
#include "lwip/stats.h"
#include "lwip/debug.h"
#include "lwip/sys.h"
#include "lwip/tcp.h"
#include "lwip/udp.h"
#include "lwip/mem.h"
#include "lwip/memp.h"
#include "lwip/tcpip.h"
#include "netif/etharp.h"
#include "lwip/dhcp.h"
#include "lwip/inet.h"
#include "lwip_handler.h"
#ifdef USE_STDRT
#include "netif/ethernetif.h"
#else
#include "ethernetif.h"
#endif
#endif
/* ============================ [ MACROS    ] ====================================================== */
#define AS_LOG_ETH 1
#define AS_LOG_ETHE 1

#define IFNAME0 't'
#define IFNAME1 'p'
#ifndef PACKET_LIB_ADAPTER_NR
#define PACKET_LIB_ADAPTER_NR 0
#endif
enum {
	FLG_RX = 0x01,
	FLG_TX = 0x02,
};

enum{
	REG_MACL      = 0x00,
	REG_MACH      = 0x04,
	REG_MTU       = 0x08,
	REG_DATA      = 0x0C,
	REG_LENGTH    = 0x10,
	REG_NETSTATUS = 0x14,
	REG_GW        = 0x18,
	REG_NETMASK   = 0x1C,
	REG_CMD       = 0x20,
	REG_ADAPTERID = 0x24,
};

#define MAX_ADDR_LEN 6

#ifdef USE_UIP
#define BUF ((struct uip_eth_hdr *)&uip_buf[0])
#define IPBUF ((struct uip_tcpip_hdr *)&uip_buf[UIP_LLH_LEN])
#endif
/* ============================ [ TYPES     ] ====================================================== */
#if defined(USE_STDRT) && defined(RT_USING_LWIP)
struct tap_netif
{
	/* inherit from ethernet device */
	struct eth_device parent;

	/* interface address info. */
	rt_uint8_t  dev_addr[MAX_ADDR_LEN];		/* hw address	*/
};
#endif
/* ============================ [ DECLARES  ] ====================================================== */
#if defined(USE_STDRT) && defined(RT_USING_LWIP)
extern void rt_thread_exit(void);
#endif
#ifdef USE_LWIP
struct pbuf * low_level_input(void);
err_t low_level_output(struct netif *netif, struct pbuf *p);
#endif
/* ============================ [ DATAS     ] ====================================================== */
static pci_dev *pdev = NULL;
static void* __iobase= NULL;
static char pkbuf[1514];
#if defined(USE_STDRT) && defined(RT_USING_LWIP)
static struct tap_netif tap_netif_device;
static struct rt_semaphore sem_lock;
#endif

#ifdef USE_UIP
PROCESS(ethernet_process, "uip driver");
#endif
/* ============================ [ LOCALS    ] ====================================================== */
#if defined(USE_STDRT) && defined(RT_USING_LWIP)
static struct netif* sys_get_netif(void)
{
	return tap_netif_device.parent.netif;
}
#endif
#ifdef USE_LWIP
void Eth_Isr(void)
{
	if(__iobase != NULL)
	{
		uint32 flag;
		flag = readl(__iobase+REG_NETSTATUS);
		if(flag&FLG_RX)
		{
			#ifdef USE_LWIP
			extern struct netif* sys_get_netif(void);
			struct eth_hdr *ethhdr;
			/* move received packet into a new pbuf */
			struct pbuf *p = low_level_input();
			struct netif* netif = sys_get_netif();
			if(p!=NULL){
				ethhdr = (struct eth_hdr *)p->payload;

				switch(htons(ethhdr->type)) {
					/* IP or ARP packet? */
					case ETHTYPE_IP:
					case ETHTYPE_ARP:
					#if PPPOE_SUPPORT
						/* PPPoE packet? */
					case ETHTYPE_PPPOEDISC:
					case ETHTYPE_PPPOE:
					#endif /* PPPOE_SUPPORT */
						#ifdef USE_PCAPIF
						if (netif->input(p, netif) != ERR_OK) {
						#else
						/* full packet send to tcpip_thread to process */
						if (ethernet_input(p, netif) != ERR_OK) {
						#endif
							LWIP_DEBUGF(NETIF_DEBUG, ("ethernetif_input: IP input error\n"));
							pbuf_free(p);
							p = NULL;
						}
						break;
					default:
						pbuf_free(p);
						break;
				}
			}
			#endif
		}
	}
}
#else
void __weak Eth_Isr(void) {}
#endif /* USE_LWIP */
#if defined(USE_STDRT) && defined(RT_USING_LWIP)
static void tap_asnet_thread_entry(void* param)
{
	struct eth_device* eth;
	uint32 flag;
	eth = (struct eth_device*) &tap_netif_device;

	rt_kprintf("tap eth server on-line\n");

	for(;;)
	{
		if (RT_NULL == __iobase) {
			rt_kprintf("tap eth server exit\n");
			rt_thread_exit();
		}

#if 1
		flag = readl(__iobase+REG_NETSTATUS);
		if(flag&FLG_RX)
		{
			/* notify eth rx thread to receive packet */
			eth_device_ready(eth);
		}
		else
		{
			rt_thread_delay(1);
		}
#else
		Eth_Isr();
		rt_thread_delay(1);
#endif
	}
}
static rt_err_t tap_netif_init(rt_device_t dev)
{
	rt_thread_t tid;
	uint32 val;
	uint32 mtu;
	pdev = find_pci_dev_from_id(0xcaac,0x0002);
	if(RT_NULL == pdev)
	{
		rt_kprintf("tap: Could not open 'asnet'\n");
		return -RT_ERROR;
	}
	
	__iobase = pci_get_memio(pdev, 1);
	enable_pci_resource(pdev);
#ifdef __X86__
	pci_register_irq(pdev->irq_num,Eth_Isr);
#else
	pci_bus_write_config_byte(pdev,0x3c,0x44);
	pci_register_irq(32+31,Eth_Isr);
#endif
	enable_pci_interrupt(pdev);

#ifdef USE_PCAPIF
	ASLOG(ETH, ("using adapter %d\n", PACKET_LIB_ADAPTER_NR));
	writel(__iobase+REG_ADAPTERID, PACKET_LIB_ADAPTER_NR);
	writel(__iobase+REG_MACH,0xdeadbeef);
	writel(__iobase+REG_MACL,0xdeadbeef);
#endif
	writel(__iobase+REG_GW, inet_addr(RT_LWIP_GWADDR));
	writel(__iobase+REG_NETMASK, inet_addr(RT_LWIP_MSKADDR));
	writel(__iobase+REG_CMD, 0);

	mtu = readl(__iobase+REG_MTU);
	val = readl(__iobase+REG_MACL);
	tap_netif_device.dev_addr[0] = (val>>0)&0xFF;
	tap_netif_device.dev_addr[1] = (val>>8)&0xFF;
	tap_netif_device.dev_addr[2] = (val>>16)&0xFF;
	tap_netif_device.dev_addr[3] = (val>>24)&0xFF;
	val = readl(__iobase+REG_MACH);
	tap_netif_device.dev_addr[4] = (val>>0)&0xFF;
	tap_netif_device.dev_addr[5] = (val>>8)&0xFF;

	RT_ASSERT(tap_netif_device.parent.netif != RT_NULL);
	memcpy(tap_netif_device.parent.netif->hwaddr,
		   tap_netif_device.dev_addr, 6);

	/* create recv thread */
	tid = rt_thread_create("tap", tap_asnet_thread_entry, RT_NULL, 
		2048, RT_THREAD_PRIORITY_MAX - 1, 10);
	if (tid != RT_NULL)
	{
		rt_thread_startup(tid);
	}

	rt_kprintf("hwaddr is %02X:%02X:%02X:%02X:%02X:%02X, mtu=%d\n",
		tap_netif_device.dev_addr[0],tap_netif_device.dev_addr[1],
		tap_netif_device.dev_addr[2],tap_netif_device.dev_addr[3],
		tap_netif_device.dev_addr[4],tap_netif_device.dev_addr[4],
		mtu);
	return RT_EOK;
}

static rt_err_t tap_netif_open(rt_device_t dev, rt_uint16_t oflag)
{
	return RT_EOK;
}

static rt_err_t tap_netif_close(rt_device_t dev)
{
	return RT_EOK;
}

static rt_size_t tap_netif_read(rt_device_t dev, rt_off_t pos, void* buffer, rt_size_t size)
{
	rt_set_errno(-RT_ENOSYS);
	return 0;
}

static rt_size_t tap_netif_write (rt_device_t dev, rt_off_t pos, const void* buffer, rt_size_t size)
{
	rt_set_errno(-RT_ENOSYS);
	return 0;
}

static rt_err_t tap_netif_control(rt_device_t dev, int cmd, void *args)
{
	switch (cmd)
	{
	case NIOCTL_GADDR:
		/* get mac address */
		if (args) rt_memcpy(args, tap_netif_device.dev_addr, 6);
		else return -RT_ERROR;
		break;

	default :
		break;
	}

	return RT_EOK;
}
static rt_err_t tap_netif_tx( rt_device_t dev, struct pbuf* p)
{
	struct pbuf *q;
	static char buffer[2048];
	int length;
	int pos = 0;
	unsigned char* ptr;

	/* lock EMAC device */
	rt_sem_take(&sem_lock, RT_WAITING_FOREVER);

	/* copy data to tx buffer */
	q = p;
	ptr = (rt_uint8_t*)buffer;
	while (q)
	{
		memcpy(ptr, q->payload, q->len);
		ptr += q->len;
		q = q->next;
	}
	length = p->tot_len;

	/* signal that packet should be sent(); */
	writel(__iobase+REG_LENGTH,p->tot_len);
	while(length > 0)
	{
		writel(__iobase+REG_DATA,*((uint8*)&buffer[pos]));
		pos += 1;
		length -= 1;
	}
	writel(__iobase+REG_CMD, 1);

	/* unlock EMAC device */
	rt_sem_release(&sem_lock);

	return RT_EOK;
}

struct pbuf *tap_netif_rx(rt_device_t dev)
{
	struct pbuf* p = RT_NULL;
	int size;
	u16_t len,pos;

	if(RT_NULL == __iobase) return RT_NULL;

	size = len = readl(__iobase+REG_LENGTH);
	if(size > 0)
	{
		pos = 0;
		while(len > 0)
		{
			pkbuf[pos] = readl(__iobase+REG_DATA);
			pos ++;
			len --;
		}
	}

	if (size > 0) {
		p = pbuf_alloc(PBUF_LINK, size, PBUF_RAM);
		pbuf_take(p, pkbuf, size);
	}

	return p;
}

#endif  /* USE_STDRT */
/* ============================ [ FUNCTIONS ] ====================================================== */
void PciNet_Init(uint32 gw, uint32 netmask, uint8* hwaddr, uint32* mtu)
{
	imask_t imask;
	ASLOG(ETH, ("!!!%s: begin\n",__FUNCTION__));
	pdev = find_pci_dev_from_id(0xcaac,0x0002);
	if(NULL != pdev)
	{
		uint32 val;
		__iobase = pci_get_memio(pdev, 1);

		Irq_Save(imask);
		enable_pci_resource(pdev);
		#ifdef __X86__
		pci_register_irq(pdev->irq_num,Eth_Isr);
		#else
		pci_bus_write_config_byte(pdev,0x3c,0x44);
		pci_register_irq(32+31,Eth_Isr);
		#endif
		enable_pci_interrupt(pdev);

#ifdef USE_PCAPIF
		ASLOG(ETH, ("using adapter %d,  gw/netmask=%08X/%08X\n",  PACKET_LIB_ADAPTER_NR,  gw, netmask));
		writel(__iobase+REG_ADAPTERID, PACKET_LIB_ADAPTER_NR);
		writel(__iobase+REG_MACH,0xdeadbeef);
		writel(__iobase+REG_MACL,0xdeadbeef);
#endif
		writel(__iobase+REG_GW, gw);
		writel(__iobase+REG_NETMASK, netmask);
		writel(__iobase+REG_CMD, 0);

		*mtu = readl(__iobase+REG_MTU);
		val = readl(__iobase+REG_MACL);
		hwaddr[0] = (val>>0)&0xFF;
		hwaddr[1] = (val>>8)&0xFF;
		hwaddr[2] = (val>>16)&0xFF;
		hwaddr[3] = (val>>24)&0xFF;
		val = readl(__iobase+REG_MACH);
		hwaddr[4] = (val>>0)&0xFF;
		hwaddr[5] = (val>>8)&0xFF;
		Irq_Restore(imask);
	}
	else
	{
		ASLOG(ERROR, ("No pci-asnet device found, specify '-device pci-asnet' to qemu\n"));
	}
}
#ifdef USE_LWIP
struct pbuf * low_level_input(void)
{
	//ASLOG(ETH, ("!!!%s begin\n",__FUNCTION__));
	imask_t irq_state;
	struct pbuf *p, *q;
	u16_t len,len2,pos;
	char *bufptr;

	if(NULL == __iobase) return NULL;

	Irq_Save(irq_state);
	/* Obtain the size of the packet and put it into the "len"
	variable. */
	len = len2 = readl(__iobase+REG_LENGTH);
	printf("\n-------------have input data--------------------\n");
	ASLOG(ETH, ("!!!%s len=%d\n",__FUNCTION__, len));

	pos = 0;
	while(len2 > 0)
	{
		pkbuf[pos] = readl(__iobase+REG_DATA);
		printf("%02x ",(uint8)(pkbuf[pos]));
		pos ++;
		len2 --;
	}
	printf("\n");

	if(0 == len)
	{
		Irq_Restore(irq_state);
		return NULL;
	}

	#if ETH_PAD_SIZE
	len += ETH_PAD_SIZE; /* allow room for Ethernet padding */
	#endif
	/* We allocate a pbuf chain of pbufs from the pool. */
	p = pbuf_alloc(PBUF_RAW, len, PBUF_POOL);

	if(p != NULL) {
		#if ETH_PAD_SIZE
		pbuf_header(p, -ETH_PAD_SIZE); /* drop the padding word */
		#endif
		/* We iterate over the pbuf chain until we have read the entire
			packet into the pbuf. */
		bufptr = &pkbuf[0];
		for(q = p; q != NULL; q = q->next) {
			/* Read enough bytes to fill this pbuf in the chain. The
			   available data in the pbuf is given by the q->len
			   variable. */
			/* read data into(q->payload, q->len); */
			memcpy(q->payload, bufptr, q->len);
			bufptr += q->len;
		}
		#if ETH_PAD_SIZE
		pbuf_header(p, ETH_PAD_SIZE); /* reclaim the padding word */
		#endif
	/* acknowledge that packet has been read(); */
	} else {
		/* drop packet(); */
	}
	Irq_Restore(irq_state);
	return p;
}

err_t low_level_output(struct netif *netif, struct pbuf *p)
{
	struct pbuf *q;
	char *bufptr;
	uint32 pos = 0;
	imask_t irq_state;

	(void)netif;

	if(NULL == __iobase) return ERR_ABRT;

	Irq_Save(irq_state);
	/* initiate transfer(); */
	#if ETH_PAD_SIZE
	pbuf_header(p, -ETH_PAD_SIZE); /* drop the padding word */
	#endif
	bufptr = &pkbuf[0];

	for(q = p; q != NULL; q = q->next) {
		/* Send the data from the pbuf to the interface, one pbuf at a
		time. The size of the data in each pbuf is kept in the ->len
		variable. */
		/* send data from(q->payload, q->len); */
		ASLOG(ETH, ("\n!!!%s len=%d\n",__FUNCTION__ ,q->len));
		memcpy(bufptr, q->payload, q->len);
		for(int i=0;i<q->len;i++){
			printf("%02x ",(uint8)(bufptr[i]));
		}
		printf("\n");
		bufptr += q->len;
	}
	#if ETH_PAD_SIZE
	pbuf_header(p, ETH_PAD_SIZE); /* reclaim the padding word */
	#endif

	/* signal that packet should be sent(); */
	writel(__iobase+REG_LENGTH,p->tot_len);
	while(p->tot_len > 0)
	{
		writel(__iobase+REG_DATA,*((uint8*)&pkbuf[pos]));
		pos += 1;
		p->tot_len -= 1;
	}
	writel(__iobase+REG_CMD, 1);
	Irq_Restore(irq_state);
	return ERR_OK;
}

err_t ethernetif_init(struct netif *netif)
{
	uint32 mtu;
	PciNet_Init(netif->gw.addr, netif->netmask.addr, netif->hwaddr,&mtu);
	netif->hwaddr_len = 6;
	netif->mtu = mtu;

	ASLOG(ETH, ("!!!%s begin\n",__FUNCTION__));
	ASLOG(ETH, ("hwaddr is %02X:%02X:%02X:%02X:%02X:%02X,  mtu=%d\n", 
			netif->hwaddr[0],netif->hwaddr[1],netif->hwaddr[2],
			netif->hwaddr[3],netif->hwaddr[4],netif->hwaddr[5],
			mtu));
	netif->name[0] = IFNAME0;
	netif->name[1] = IFNAME1;
	netif->output = etharp_output;
	netif->linkoutput = low_level_output;

	netif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_IGMP;
	return ERR_OK;
}
#endif

#if defined(USE_STDRT) && defined(RT_USING_LWIP)
void tap_netif_hw_init(void)
{
	rt_sem_init(&sem_lock, "eth_lock", 1, RT_IPC_FLAG_FIFO);

	tap_netif_device.parent.parent.init		= tap_netif_init;
	tap_netif_device.parent.parent.open		= tap_netif_open;
	tap_netif_device.parent.parent.close	= tap_netif_close;
	tap_netif_device.parent.parent.read		= tap_netif_read;
	tap_netif_device.parent.parent.write	= tap_netif_write;
	tap_netif_device.parent.parent.control	= tap_netif_control;
	tap_netif_device.parent.parent.user_data= RT_NULL;

	tap_netif_device.parent.eth_rx			= tap_netif_rx;
	tap_netif_device.parent.eth_tx			= tap_netif_tx;

	eth_device_init(&(tap_netif_device.parent), "e0");
}
#endif /* USE_STDRT */

#ifdef USE_UIP
static void ethernet_send(void)
{
	char *bufptr;
	uint32 pos = 0;
	imask_t irq_state;
	uint32 tot_len;

	if(NULL == __iobase) return;

	//ASMEM(ETH, "TX", uip_buf, uip_len);

	Irq_Save(irq_state);

	tot_len = uip_len;
	/* signal that packet should be sent(); */
	writel(__iobase+REG_LENGTH, uip_len);
	while(tot_len > 0)
	{
		writel(__iobase+REG_DATA,*((uint8*)&uip_buf[pos]));
		pos += 1;
		tot_len -= 1;
	}
	writel(__iobase+REG_CMD, 1);
	Irq_Restore(irq_state);
}

#if !NETSTACK_CONF_WITH_IPV6
static uint8_t ethernet_output(void)
{
   uip_arp_out();
   ethernet_send();  
   return 0;
}
#endif

static uint16_t ethernet_poll(void)
{
	int size = 0;
	uint32 flag;
	imask_t irq_state;
	uint16 len,pos;
	Irq_Save(irq_state);
	if(__iobase != NULL)
	{
		flag = readl(__iobase+REG_NETSTATUS);
		if(flag&FLG_RX)
		{
			size = len = readl(__iobase+REG_LENGTH);
			if(size > UIP_BUFSIZE)
			{
				ASLOG(ETHE, ("uip input buffer(%d bytes) is not enough for packet with size %d bytes.\n",  UIP_BUFSIZE, size));
				size = len = UIP_BUFSIZE;
			}
			if(size > 0)
			{
				pos = 0;
				while(len > 0)
				{
					uip_buf[pos] = readl(__iobase+REG_DATA);
					pos ++;
					len --;
				}
			}
		}
	}
	Irq_Restore(irq_state);
	return size;
}

static void ethernet_exit(void)
{
	ASLOG(ETHE, ("ethernet exit is not supported\n"));
}

static void ethernet_init(void)
{
	uint32 mtu;
	ASLOG(ETH, ("!!!%s: begin\n",__FUNCTION__));
	PciNet_Init(inet_addr("172.18.0.1"), inet_addr("255.255.255.0"), uip_lladdr.addr, &mtu);

	ASLOG(ETH, ("hwaddr is %02X:%02X:%02X:%02X:%02X:%02X,  mtu=%d\n", 
		  uip_lladdr.addr[0],uip_lladdr.addr[1],uip_lladdr.addr[2],
		  uip_lladdr.addr[3],uip_lladdr.addr[4],uip_lladdr.addr[5],
		  mtu));
}

static void pollhandler(void)
{
  uip_len = ethernet_poll();

  if(uip_len > 0) {
	  //ASMEM(ETH, "RX", uip_buf, uip_len);
#if NETSTACK_CONF_WITH_IPV6
    if(BUF->type == uip_htons(UIP_ETHTYPE_IPV6)) {
      tcpip_input();
    } else
#endif /* NETSTACK_CONF_WITH_IPV6 */
    if(BUF->type == uip_htons(UIP_ETHTYPE_IP)) {
      uip_len -= sizeof(struct uip_eth_hdr);
      tcpip_input();
    } else if(BUF->type == uip_htons(UIP_ETHTYPE_ARP)) {
#if !NETSTACK_CONF_WITH_IPV6 //math
       uip_arp_arpin();
       /* If the above function invocation resulted in data that
	  should be sent out on the network, the global variable
	  uip_len is set to a value > 0. */
       if(uip_len > 0) {
	  ethernet_send();
       }
#endif              
    } else {
      uip_clear_buf();
    }
  }
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(ethernet_process, ev, data)
{
  PROCESS_POLLHANDLER(pollhandler());

  PROCESS_BEGIN();

  ethernet_init();
#if !NETSTACK_CONF_WITH_IPV6
  tcpip_set_outputfunc(ethernet_output);
#else
  tcpip_set_outputfunc(ethernet_send);
#endif
  process_poll(&ethernet_process);

  PROCESS_WAIT_UNTIL(ev == PROCESS_EVENT_EXIT);

  ethernet_exit();

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
#endif /* USE_UIP */
#endif /* USE_PCI */
