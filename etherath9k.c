#include "u.h"
#include "../port/lib.h"
#include "mem.h"
#include "dat.h"
#include "fns.h"
#include "io.h"
#include "../port/error.h"
#include "../port/netif.h"

#include "etherif.h"
#include "wifi.h"

enum {
	Attached 	= 0x0001,	/* attach has succeeded */
	Enabled		= 1<<1,		/* chip is enabled */
	Gpio 		= 1<<2,		/* gpio device attached */
};

enum {
	Isr		= 0x008,	/* interrupt status */
	Imr		= 0x00c,	/* interrupt mask */

	FhIsr		= 0x010,	/* second interrupt status */
};

#define DBG	print
#define csr32r(c, r)	(*((c)->nic+((r)/4)))
#define csr32w(c, r, v)	(*((c)->nic+((r)/4)) = (v))
#define athisenabled(ctlr) 	(((ctlr)->flags & Enabled)==Enabled)

typedef struct Ctlr Ctlr;

struct Ctlr {
	Lock;
	QLock;

	Ctlr *link;
	Pcidev *pdev;
	Wifi *wifi;

	u32int ie;

	int active;
	int port;
	int flags;	/* ath flags */

	u32int *nic;
	int channel;
};


static Ctlr *ath9khead, *ath9ktail;

static void
ath9kdisinstr(Ctlr *ctlr)
{
	/* Disable interrupts */
	ctlr->ie = 0;
	csr32w(ctlr, Imr, 0);
	csr32w(ctlr, Isr, ~0);
	csr32w(ctlr, FhIsr, ~0);
}

static void
ath9kstop(Ether *edev)
{
	Ctlr *ctlr;

	ctlr = edev->ctlr;

	/*
	 * Shutdown the hardware and driver:
	 *    disable interrupts
	 *    turn off timers
	 *    clear transmit machinery
	 *    clear receive machinery
	 *    drain and release tx queues
	 *    reclaim beacon resources
	 *    reset 802.11 state machine
	 *    power down hardware
	 *
	 * Note that some of this work is not possible if the
	 * hardware is gone (invalid).
	 */
	ctlr->channel = 0;
	ath9kdisinstr(ctlr);

	DBG("Atheros stopped!\n");
}


static int
ath9kinit(Ether *edev)
{
	Ctlr *ctlr;
	char *err;

	ctlr = edev->ctlr;

	DBG("ath9k enabled! Resetting device...\n");

	ath9kstop(edev);

	return 0;

Err:
	DBG("ath9kinit: %s\n", err);
	//poweroff(ctlr);
	return -1;
}

static void
ath9kintr(Ureg* ureg, void *arg)
{
	DBG("ath9kintr: start\n");
}

static void
ath9kattach(Ether *edev)
{
	DBG("ath9kattach: start\n");
}

static void
ath9kshutdown(Ether *edev)
{
	DBG("ath9kshutdown: start\n");
}

static void
ath9kpromiscuous(void *arg, int on)
{
	DBG("ath9kpromiscuous: start\n");
}

static long
ath9kifstat(Ether *edev, void *buf, long n, ulong off)
{
	DBG("ath9kifstat: start\n");
	return -1;
}

static long
ath9kctl(Ether *edev, void *buf, long n)
{
	DBG("ath9kctl: start\n");
	return -1;
}

static void
ath9kmulticast(void *, uchar*, int)
{
	DBG("ath9kmulticast: start\n");
}

static void
ath9kpci(void)
{
	Pcidev *pdev;

	pdev = nil;

	while(pdev = pcimatch(pdev, 0, 0)) {
		Ctlr *ctlr;
		void *mem;

		if(pdev->ccrb != 2)
			continue;

		if(pdev->vid != 0x168c)
			continue;

		switch(pdev->did){
		default:
			continue;
		case 0x002b:	/* Atheros AR9285 */
			break;
		}

		DBG("Atheros AR9285 device found!\n");

		pcisetbme(pdev);
		pcisetpms(pdev, 0);

		ctlr = malloc(sizeof(Ctlr));
		if(ctlr == nil) {
			print("ath9k: unable to alloc Ctlr\n");
			continue;
		}
		ctlr->port = pdev->mem[0].bar & ~0x0F;
		mem = vmap(pdev->mem[0].bar & ~0x0F, pdev->mem[0].size);
		if(mem == nil) {
			print("ath9k: can't map %8.8luX\n", pdev->mem[0].bar);
			free(ctlr);
			continue;
		}

		ctlr->nic = mem;
		ctlr->pdev = pdev;

		if(ath9khead != nil)
			ath9ktail->link = ctlr;
		else
			ath9khead = ctlr;
		ath9ktail = ctlr;
	}
}

static int
ath9kpnp(Ether* edev)
{
	Ctlr *ctlr;

	if(ath9khead == nil)
		ath9kpci();

again:
	for(ctlr = ath9khead; ctlr != nil; ctlr = ctlr->link){
		if(ctlr->active)
			continue;
		if(edev->port == 0 || edev->port == ctlr->port){
			ctlr->active = 1;
			break;
		}
	}

	if(ctlr == nil)
		return -1;

	edev->ctlr = ctlr;
	edev->port = ctlr->port;
	edev->irq = ctlr->pdev->intl;
	edev->tbdf = ctlr->pdev->tbdf;
	edev->arg = edev;
	edev->interrupt = ath9kintr;
	edev->attach = ath9kattach;
	edev->ifstat = ath9kifstat;
	edev->ctl = ath9kctl;
	edev->shutdown = ath9kshutdown;
	edev->promiscuous = ath9kpromiscuous;
	edev->multicast = ath9kmulticast;
	edev->mbps = 54;

	if(ath9kinit(edev) < 0){
		edev->ctlr = nil;
		goto again;
	}
	
	return -1;
}

void
etherath9klink(void)
{
	addethercard("ath9k", ath9kpnp);
}