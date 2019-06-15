#ifndef _MCTCP_H
#define _MCTCP_H

#include <linux/skbuff.h>

extern int mcps_try_skb(struct sk_buff *skb);

#ifdef CONFIG_MODEM_IF_NET_GRO
extern int mcps_try_gro(struct sk_buff *skb);
#endif

#endif
