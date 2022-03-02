/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */


#include "system.h"

#include "huffman.h"

#include "network.h"

CHuffman g_Huffman;
NETSOCKET g_Socket;
NETADDR g_ServerAddr;
unsigned char g_aRequestTokenBuf[NET_TOKENREQUEST_DATASIZE];


void init_network()
{
	NETADDR BindAddr;
	net_host_lookup("127.0.0.1", &BindAddr, NETTYPE_ALL);
	g_Socket = net_udp_create(BindAddr, 0);
	g_Huffman.Init(0);
	mem_zero(g_aRequestTokenBuf, sizeof(g_aRequestTokenBuf));
}


int net_udp_send(NETSOCKET sock, const NETADDR *addr, const void *data, int size)
{
	int d = -1;

	if(addr->type&NETTYPE_IPV4)
	{
		if(sock.ipv4sock >= 0)
		{
			struct sockaddr_in sa;
			if(addr->type&NETTYPE_LINK_BROADCAST)
			{
				mem_zero(&sa, sizeof(sa));
				sa.sin_port = htons(addr->port);
				sa.sin_family = AF_INET;
				sa.sin_addr.s_addr = INADDR_BROADCAST;
			}
			else
				netaddr_to_sockaddr_in(addr, &sa);

			d = sendto((int)sock.ipv4sock, (const char*)data, size, 0, (struct sockaddr *)&sa, sizeof(sa));
		}
		else
			dbg_msg("net", "can't sent ipv4 traffic to this socket socket=%d", sock.ipv4sock);
	}

	if(addr->type&NETTYPE_IPV6)
	{
		if(sock.ipv6sock >= 0)
		{
			struct sockaddr_in6 sa;
			if(addr->type&NETTYPE_LINK_BROADCAST)
			{
				mem_zero(&sa, sizeof(sa));
				sa.sin6_port = htons(addr->port);
				sa.sin6_family = AF_INET6;
				sa.sin6_addr.s6_addr[0] = 0xff; /* multicast */
				sa.sin6_addr.s6_addr[1] = 0x02; /* link local scope */
				sa.sin6_addr.s6_addr[15] = 1; /* all nodes */
			}
			else
				netaddr_to_sockaddr_in6(addr, &sa);

			d = sendto((int)sock.ipv6sock, (const char*)data, size, 0, (struct sockaddr *)&sa, sizeof(sa));
		}
		else
			dbg_msg("net", "can't sent ipv6 traffic to this socket");
	}
	/*
	else
		dbg_msg("net", "can't sent to network of type %d", addr->type);
		*/

	if(d < 0)
	{
		char addrstr[256];
		net_addr_str(addr, addrstr, sizeof(addrstr), true);

		dbg_msg("net", "sendto error (%d '%s')", errno, strerror(errno));
		dbg_msg("net", "\tsock = %d %x", sock, sock);
		dbg_msg("net", "\tsize = %d %x", size, size);
		dbg_msg("net", "\taddr = %s", addrstr);
	}

	return d;
}

void SendPacket(const NETADDR *pAddr, CNetPacketConstruct *pPacket)
{
	unsigned char aBuffer[NET_MAX_PACKETSIZE];
	int CompressedSize = -1;
	int FinalSize = -1;

	// compress if not ctrl msg
	if(!(pPacket->m_Flags&NET_PACKETFLAG_CONTROL))
		CompressedSize = g_Huffman.Compress(pPacket->m_aChunkData, pPacket->m_DataSize, &aBuffer[NET_PACKETHEADERSIZE], NET_MAX_PAYLOAD);

	// check if the compression was enabled, successful and good enough
	if(CompressedSize > 0 && CompressedSize < pPacket->m_DataSize)
	{
		FinalSize = CompressedSize;
		pPacket->m_Flags |= NET_PACKETFLAG_COMPRESSION;
	}
	else
	{
		// use uncompressed data
		FinalSize = pPacket->m_DataSize;
		mem_copy(&aBuffer[NET_PACKETHEADERSIZE], pPacket->m_aChunkData, pPacket->m_DataSize);
		pPacket->m_Flags &= ~NET_PACKETFLAG_COMPRESSION;
	}

	// set header and send the packet if all things are good
	if(FinalSize >= 0)
	{
		FinalSize += NET_PACKETHEADERSIZE;

		int i = 0;
		aBuffer[i++] = ((pPacket->m_Flags<<2)&0xfc) | ((pPacket->m_Ack>>8)&0x03); // flags and ack
		aBuffer[i++] = (pPacket->m_Ack)&0xff; // ack
		aBuffer[i++] = (pPacket->m_NumChunks)&0xff; // num chunks
		aBuffer[i++] = (pPacket->m_Token>>24)&0xff; // token
		aBuffer[i++] = (pPacket->m_Token>>16)&0xff;
		aBuffer[i++] = (pPacket->m_Token>>8)&0xff;
		aBuffer[i++] = (pPacket->m_Token)&0xff;

		net_udp_send(g_Socket, pAddr, aBuffer, FinalSize);
	}
	else
		dbg_msg("libtwnetwork", "Could not send packet with FinalSize=%d", FinalSize);
}

extern "C" {

void Connect(const char *pIp, int Port)
{
	init_network();
	dbg_msg("libtwnetwork", "connecting to ip=%s port=%d", pIp, Port);
	if(net_addr_from_str(&g_ServerAddr, pIp) != 0)
	{
		dbg_msg("libtwnetwork", "could not find the address of %s, connecting to localhost", pIp);
		net_host_lookup("localhost", &g_ServerAddr, g_Socket.type);
	}
	g_ServerAddr.port = Port;
}

void Send(CNetPacketConstruct *pPacket)
{
	SendPacket(&g_ServerAddr, pPacket);
}

void SendSample()
{
	int ExtraSize = sizeof(g_aRequestTokenBuf);
	CNetPacketConstruct Construct;
	Construct.m_Token = NET_TOKEN_NONE;
	Construct.m_Flags = NET_PACKETFLAG_CONTROL;
	Construct.m_Ack = 0;
	Construct.m_NumChunks = 0;
	Construct.m_DataSize = 1+ExtraSize;
	Construct.m_aChunkData[0] = NET_CTRLMSG_TOKEN;

	SendPacket(&g_ServerAddr, &Construct);
}

}
