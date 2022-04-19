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


int net_udp_recv(NETSOCKET sock, NETADDR *addr, void *data, int maxsize)
{
	char sockaddrbuf[128];
	socklen_t fromlen;// = sizeof(sockaddrbuf);
	int bytes = 0;

	if(sock.ipv4sock >= 0)
	{
		fromlen = sizeof(struct sockaddr_in);
		bytes = recvfrom(sock.ipv4sock, (char*)data, maxsize, 0, (struct sockaddr *)&sockaddrbuf, &fromlen);
	}

	if(bytes <= 0 && sock.ipv6sock >= 0)
	{
		fromlen = sizeof(struct sockaddr_in6);
		bytes = recvfrom(sock.ipv6sock, (char*)data, maxsize, 0, (struct sockaddr *)&sockaddrbuf, &fromlen);
	}

	if(bytes > 0)
	{
		sockaddr_to_netaddr((struct sockaddr *)&sockaddrbuf, addr);
		return bytes;
	}
	else if(bytes == 0)
		return 0;
	return -1; /* error */
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

int UnpackPacket(NETADDR *pAddr, unsigned char *pBuffer, CNetPacketConstruct *pPacket)
{
	int Size = net_udp_recv(g_Socket, pAddr, pBuffer, NET_MAX_PACKETSIZE);
	if(Size <= 0)
		return 1;

	if(Size < NET_PACKETHEADERSIZE || Size > NET_MAX_PACKETSIZE)
	{
		dbg_msg("network", "packet too small, size=%d", Size);
		return -1;
	}

	pPacket->m_Flags = (pBuffer[0]&0xfc)>>2;

	// FFFFFFxx
	if(pPacket->m_Flags&NET_PACKETFLAG_CONNLESS)
	{
		if(Size < NET_PACKETHEADERSIZE_CONNLESS)
		{
			dbg_msg("net", "connless packet too small, size=%d", Size);
			return -1;
		}

		pPacket->m_Flags = NET_PACKETFLAG_CONNLESS;
		pPacket->m_Ack = 0;
		pPacket->m_NumChunks = 0;
		int Version = pBuffer[0]&0x3;
		// xxxxxxVV

		if(Version != NET_PACKETVERSION)
			return -1;

		pPacket->m_DataSize = Size - NET_PACKETHEADERSIZE_CONNLESS;
		pPacket->m_Token = (pBuffer[1] << 24) | (pBuffer[2] << 16) | (pBuffer[3] << 8) | pBuffer[4];
		// TTTTTTTT TTTTTTTT TTTTTTTT TTTTTTTT
		pPacket->m_ResponseToken = (pBuffer[5]<<24) | (pBuffer[6]<<16) | (pBuffer[7]<<8) | pBuffer[8];
		// RRRRRRRR RRRRRRRR RRRRRRRR RRRRRRRR
		mem_copy(pPacket->m_aChunkData, &pBuffer[NET_PACKETHEADERSIZE_CONNLESS], pPacket->m_DataSize);
	}
	else
	{
		if(Size - NET_PACKETHEADERSIZE > NET_MAX_PAYLOAD)
		{
			dbg_msg("network", "packet payload too big, size=%d", Size);
			return -1;
		}

		pPacket->m_Ack = ((pBuffer[0]&0x3)<<8) | pBuffer[1];
			// xxxxxxAA AAAAAAAA
		pPacket->m_NumChunks = pBuffer[2];
			// NNNNNNNN

		pPacket->m_DataSize = Size - NET_PACKETHEADERSIZE;
		pPacket->m_Token = (pBuffer[3] << 24) | (pBuffer[4] << 16) | (pBuffer[5] << 8) | pBuffer[6];
			// TTTTTTTT TTTTTTTT TTTTTTTT TTTTTTTT
		pPacket->m_ResponseToken = NET_TOKEN_NONE;

		if(pPacket->m_Flags&NET_PACKETFLAG_COMPRESSION)
			pPacket->m_DataSize = g_Huffman.Decompress(&pBuffer[NET_PACKETHEADERSIZE], pPacket->m_DataSize, pPacket->m_aChunkData, sizeof(pPacket->m_aChunkData));
		else
			mem_copy(pPacket->m_aChunkData, &pBuffer[NET_PACKETHEADERSIZE], pPacket->m_DataSize);
	}

	// check for errors
	if(pPacket->m_DataSize < 0)
	{
		dbg_msg("network", "error during packet decoding");
		return -1;
	}

	// set the response token (a bit hacky because this function shouldn't know about control packets)
	if(pPacket->m_Flags&NET_PACKETFLAG_CONTROL)
	{
		if(pPacket->m_DataSize >= 5) // control byte + token
		{
			if(pPacket->m_aChunkData[0] == NET_CTRLMSG_CONNECT
				|| pPacket->m_aChunkData[0] == NET_CTRLMSG_TOKEN)
			{
				pPacket->m_ResponseToken = (pPacket->m_aChunkData[1]<<24) | (pPacket->m_aChunkData[2]<<16)
					| (pPacket->m_aChunkData[3]<<8) | pPacket->m_aChunkData[4];
			}
		}
	}

	// chiller debug start
	char aAddrStr[NETADDR_MAXSTRSIZE];
	net_addr_str(pAddr, aAddrStr, sizeof(aAddrStr), true);
	if(str_startswith(aAddrStr, "[0:0:0:0:0:0:0:1]:") || str_startswith(aAddrStr, "127.0.0.1:"))
	{
		char aFlags[512];
		aFlags[0] = '\0';
		if(pPacket->m_Flags&NET_PACKETFLAG_CONTROL)
			str_append(aFlags, "CONTROL", sizeof(aFlags));
		if(pPacket->m_Flags&NET_PACKETFLAG_RESEND)
			str_append(aFlags, aFlags[0] ? "|RESEND" : "RESEND", sizeof(aFlags));
		if(pPacket->m_Flags&NET_PACKETFLAG_COMPRESSION)
			str_append(aFlags, aFlags[0] ? "|COMPRESSION" : "COMPRESSION", sizeof(aFlags));
		if(pPacket->m_Flags&NET_PACKETFLAG_CONNLESS)
			str_append(aFlags, aFlags[0] ? "|CONNLESS" : "CONNLESS", sizeof(aFlags));
		char aBuf[512];
		aBuf[0] = '\0';
		if(aFlags[0])
			str_format(aBuf, sizeof(aBuf), " (%s)", aFlags);
		char aHexData[1024];
		str_hex(aHexData, sizeof(aHexData), pPacket->m_aChunkData, pPacket->m_DataSize);
		char aRawData[1024];
		for(int i = 0; i < pPacket->m_DataSize; i++)
			aRawData[i] = pPacket->m_aChunkData[i] < 32 ? '.' : pPacket->m_aChunkData[i];
		dbg_msg("network", "%s size=%d flags=%d%s", aAddrStr, Size, pPacket->m_Flags, aBuf);
		dbg_msg("network", "  data: %s", aHexData);
		dbg_msg("network", "  data_raw: %s", aRawData);
	}
	// chiller debug end
	return 0;
}

// CNetRecvUnpacker::m_Data
CNetPacketConstruct g_Data;
// CNetRecvUnpacker::m_aBuffer
unsigned char g_aBuffer[NET_MAX_PACKETSIZE];


int FetchChunk(CNetChunk *pChunk)
{
	CNetChunkHeader Header;
	unsigned char *pEnd = g_Data.m_aChunkData + g_Data.m_DataSize;
	while(1)
	{
		unsigned char *pData = g_Data.m_aChunkData;

		// TODO: this is incomplete
		pData = Header.Unpack(pData);
		return 0;
	}
	return 0;
}

int Recv(CNetChunk *pChunk, TOKEN *pResponseToken)
{
	while(1)
	{
		// check for a chunk
		if(FetchChunk(pChunk))
			return 1;

		NETADDR Addr;
		unsigned char aBuffer[NET_MAX_PACKETSIZE];
		int Result = UnpackPacket(&Addr, g_aBuffer, &g_Data);
		// no more packets for now
		if(Result > 0)
			break;

		if(!Result)
		{
			return 0; // TODO: implement this
		}
	}
	return 0;
}

void PumpNetwork()
{
	CNetChunk Packet;
	while(Recv(&Packet, 0))
	{
		// if(!(Packet.m_Flags&NETSENDFLAG_CONNLESS))
		// 	ProcessServerPacket(&Packet);
	}
}

}
