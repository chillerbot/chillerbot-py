/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

/*

CURRENT:
	packet header: 7 bytes (9 bytes for connless)
		unsigned char flags_ack;    // 6bit flags, 2bit ack
		unsigned char ack;          // 8bit ack
		unsigned char numchunks;    // 8bit chunks
		unsigned char token[4];     // 32bit token
		// ffffffaa
		// aaaaaaaa
		// NNNNNNNN
		// TTTTTTTT
		// TTTTTTTT
		// TTTTTTTT
		// TTTTTTTT

	packet header (CONNLESS):
		unsigned char flag_version;				// 6bit flags, 2bits version
		unsigned char token[4];					// 32bit token
		unsigned char responsetoken[4];			// 32bit response token

		// ffffffvv
		// TTTTTTTT
		// TTTTTTTT
		// TTTTTTTT
		// TTTTTTTT
		// RRRRRRRR
		// RRRRRRRR
		// RRRRRRRR
		// RRRRRRRR

	if the token isn't explicitely set by any means, it must be set to
	0xffffffff

	chunk header: 2-3 bytes
		unsigned char flags_size; // 2bit flags, 6 bit size
		unsigned char size_seq; // 6bit size, 2bit seq
		(unsigned char seq;) // 8bit seq, if vital flag is set
*/

enum
{
	NETFLAG_ALLOWSTATELESS=1,
	NETSENDFLAG_VITAL=1,
	NETSENDFLAG_CONNLESS=2,
	NETSENDFLAG_FLUSH=4,

	NETSTATE_OFFLINE=0,
	NETSTATE_CONNECTING,
	NETSTATE_ONLINE,

	NETBANTYPE_SOFT=1,
	NETBANTYPE_DROP=2,

	NETCREATE_FLAG_RANDOMPORT=1,
};


enum
{
	NET_MAX_CHUNKHEADERSIZE = 3,
	
	// packets
	NET_PACKETHEADERSIZE = 7,
	NET_PACKETHEADERSIZE_CONNLESS = NET_PACKETHEADERSIZE + 2,
	NET_MAX_PACKETHEADERSIZE = NET_PACKETHEADERSIZE_CONNLESS,

	NET_MAX_PACKETSIZE = 1400,
	NET_MAX_PAYLOAD = NET_MAX_PACKETSIZE-NET_MAX_PACKETHEADERSIZE,

	NET_PACKETVERSION=1,

	NET_PACKETFLAG_CONTROL=1,
	NET_PACKETFLAG_RESEND=2,
	NET_PACKETFLAG_COMPRESSION=4,
	NET_PACKETFLAG_CONNLESS=8,

	NET_MAX_PACKET_CHUNKS=256,

	// token
	NET_SEEDTIME = 16,

	NET_TOKENCACHE_SIZE = 64,
	NET_TOKENCACHE_ADDRESSEXPIRY = NET_SEEDTIME,
	NET_TOKENCACHE_PACKETEXPIRY = 5,
};
enum
{
	NET_TOKEN_MAX = 0xffffffff,
	NET_TOKEN_NONE = NET_TOKEN_MAX,
	NET_TOKEN_MASK = NET_TOKEN_MAX,
};
enum
{
	NET_TOKENFLAG_ALLOWBROADCAST = 1,
	NET_TOKENFLAG_RESPONSEONLY = 2,

	NET_TOKENREQUEST_DATASIZE = 512,

	//
	NET_MAX_CLIENTS = 64,
	NET_MAX_CONSOLE_CLIENTS = 4,
	
	NET_MAX_SEQUENCE = 1<<10,
	NET_SEQUENCE_MASK = NET_MAX_SEQUENCE-1,

	NET_CONNSTATE_OFFLINE=0,
	NET_CONNSTATE_TOKEN=1,
	NET_CONNSTATE_CONNECT=2,
	NET_CONNSTATE_PENDING=3,
	NET_CONNSTATE_ONLINE=4,
	NET_CONNSTATE_ERROR=5,

	NET_CHUNKFLAG_VITAL=1,
	NET_CHUNKFLAG_RESEND=2,

	NET_CTRLMSG_KEEPALIVE=0,
	NET_CTRLMSG_CONNECT=1,
	NET_CTRLMSG_ACCEPT=2,
	NET_CTRLMSG_CLOSE=4,
	NET_CTRLMSG_TOKEN=5,

	NET_CONN_BUFFERSIZE=1024*32,

	NET_ENUM_TERMINATOR
};

typedef unsigned int TOKEN;

struct CNetChunk
{
	// -1 means that it's a connless packet
	// 0 on the client means the server
	int m_ClientID;
	NETADDR m_Address; // only used when cid == -1
	int m_Flags;
	int m_DataSize;
	const void *m_pData;
};

class CNetChunkHeader
{
public:
	int m_Flags;
	int m_Size;
	int m_Sequence;

	unsigned char *Pack(unsigned char *pData)
	{
		pData[0] = ((m_Flags&0x03)<<6) | ((m_Size>>6)&0x3F);
		pData[1] = (m_Size&0x3F);
		if(m_Flags&NET_CHUNKFLAG_VITAL)
		{
			pData[1] |= (m_Sequence>>2)&0xC0;
			pData[2] = m_Sequence&0xFF;
			return pData + 3;
		}
		return pData + 2;
	}
	unsigned char *Unpack(unsigned char *pData)
	{
		m_Flags = (pData[0]>>6)&0x03;
		m_Size = ((pData[0]&0x3F)<<6) | (pData[1]&0x3F);
		m_Sequence = -1;
		if(m_Flags&NET_CHUNKFLAG_VITAL)
		{
			m_Sequence = ((pData[1]&0xC0)<<2) | pData[2];
			return pData + 3;
		}
		return pData + 2;
	}
};

class CNetChunkResend
{
public:
	int m_Flags;
	int m_DataSize;
	unsigned char *m_pData;

	int m_Sequence;
	int64_t m_LastSendTime;
	int64_t m_FirstSendTime;
};

class CNetPacketConstruct
{
public:
	TOKEN m_Token;
	TOKEN m_ResponseToken; // only used in connless packets
	int m_Flags;
	int m_Ack;
	int m_NumChunks;
	int m_DataSize;
	unsigned char m_aChunkData[NET_MAX_PAYLOAD];
};
