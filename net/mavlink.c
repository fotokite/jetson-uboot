/*
 * Mavlink is a protocol used in internal communication on UAVs.
 *
 * Uses parts from the autogenerated code from our own message definitions.
 *
 * Copyright (C) 2020 by Alexander Hedges (alexander.hedges@fotokite.com)
 *
 * Licensed under the GPL v2 or later
 */

#include <common.h>
#include <net.h>

// Contains message definition specific data. Needs to define:
// MAVLINK_MSG_ID_GS_BUTTON_LONG_PRESS
// MAVLINK_MESSAGE_CRCS
// MAVLINK_STX
// DEFAULT_MAVLINK_SRC_IP
// DEFAULT_MAVLINK_PORT
#include "mavlink_messages.h"

// Macro to define packed structures
#ifdef __GNUC__
  #define MAVPACKED( __Declaration__ ) __Declaration__ __attribute__((packed))
#else
  #define MAVPACKED( __Declaration__ ) __pragma( pack(push, 1) ) __Declaration__ __pragma( pack(pop) )
#endif

#ifndef MAVLINK_MAX_PAYLOAD_LEN
// it is possible to override this, but be careful!
#define MAVLINK_MAX_PAYLOAD_LEN 255 ///< Maximum payload length
#endif

#define MAVLINK_NUM_CHECKSUM_BYTES 2
#define MAVLINK_SIGNATURE_BLOCK_LEN 13

MAVPACKED(
typedef struct __mavlink_message {
	uint16_t checksum;      ///< sent at end of packet
	uint8_t magic;          ///< protocol magic marker
	uint8_t len;            ///< Length of payload
	uint8_t incompat_flags; ///< flags that must be understood
	uint8_t compat_flags;   ///< flags that can be ignored if not understood
	uint8_t seq;            ///< Sequence of packet
	uint8_t sysid;          ///< ID of message sender system/aircraft
	uint8_t compid;         ///< ID of the message sender component
	uint32_t msgid:24;      ///< ID of message in payload
	uint64_t payload64[(MAVLINK_MAX_PAYLOAD_LEN+MAVLINK_NUM_CHECKSUM_BYTES+7)/8];
	uint8_t ck[2];          ///< incoming checksum bytes
	uint8_t signature[MAVLINK_SIGNATURE_BLOCK_LEN];
}) mavlink_message_t;

typedef enum {
    MAVLINK_PARSE_STATE_UNINIT=0,
    MAVLINK_PARSE_STATE_IDLE,
    MAVLINK_PARSE_STATE_GOT_STX,
    MAVLINK_PARSE_STATE_GOT_LENGTH,
    MAVLINK_PARSE_STATE_GOT_INCOMPAT_FLAGS,
    MAVLINK_PARSE_STATE_GOT_COMPAT_FLAGS,
    MAVLINK_PARSE_STATE_GOT_SEQ,
    MAVLINK_PARSE_STATE_GOT_SYSID,
    MAVLINK_PARSE_STATE_GOT_COMPID,
    MAVLINK_PARSE_STATE_GOT_MSGID1,
    MAVLINK_PARSE_STATE_GOT_MSGID2,
    MAVLINK_PARSE_STATE_GOT_MSGID3,
    MAVLINK_PARSE_STATE_GOT_PAYLOAD,
    MAVLINK_PARSE_STATE_GOT_CRC1,
    MAVLINK_PARSE_STATE_GOT_BAD_CRC1,
    MAVLINK_PARSE_STATE_SIGNATURE_WAIT
} mavlink_parse_state_t; ///< The state machine for the comm parser

typedef enum {
    MAVLINK_FRAMING_INCOMPLETE=0,
    MAVLINK_FRAMING_OK=1,
    MAVLINK_FRAMING_BAD_CRC=2,
    MAVLINK_FRAMING_BAD_SIGNATURE=3
} mavlink_framing_t;

typedef struct __mavlink_status {
    mavlink_framing_t msg_received;     ///< Number of received messages
    uint8_t buffer_overrun;             ///< Number of buffer overruns
    uint8_t parse_error;                ///< Number of parse errors
    mavlink_parse_state_t parse_state;  ///< Parsing state machine
    uint8_t packet_idx;                 ///< Index in current packet
    uint8_t current_rx_seq;             ///< Sequence number of last packet received
    uint8_t current_tx_seq;             ///< Sequence number of last packet sent
    uint16_t packet_rx_success_count;   ///< Received packets
    uint16_t packet_rx_drop_count;      ///< Number of packet drops
    uint8_t flags;                      ///< MAVLINK_STATUS_FLAG_*
    uint8_t signature_wait;             ///< number of signature bytes left to receive
    struct __mavlink_signing *signing;  ///< optional signing state
} mavlink_status_t;

static mavlink_status_t parsing_status;
static mavlink_message_t parsing_msg;
static struct in_addr mavlink_src_ip;
static unsigned int mavlink_port;

int parse_mavlink_msg(uchar *pkt, unsigned int len, unsigned int *start, mavlink_message_t *out_msg);

static void mavlink_timeout(void)
{
	net_set_state(NETLOOP_FAIL);
}

static void mavlink_msg_handler(uchar *pkt, unsigned dport,
		struct in_addr sip, unsigned sport, unsigned len)
{
	int i, err;
	unsigned int start;
	mavlink_message_t msg;

	debug("mavlink handler: got packet: (src=%d.%d.%d.%d (0x%.8x), src port=%d, dst port=%d, len=%d)\n",
		((uchar *) &sip.s_addr)[0], ((uchar *) &sip.s_addr)[1], ((uchar *) &sip.s_addr)[2], ((uchar *) &sip.s_addr)[3], sip.s_addr, sport, dport, len);

	if (sport != mavlink_port || sip.s_addr != mavlink_src_ip.s_addr) {
		return;
	}

	for (i = 0; i < len; i++) {
		debug("%.2x", pkt[i]);
	}
	debug("\n");

	start = 0;
	while (start < len) {
		err = parse_mavlink_msg(pkt, len, &start, &msg);
		if (err == 0 && msg.msgid == MAVLINK_MSG_ID_GS_BUTTON_LONG_PRESS) {
			net_set_state(NETLOOP_SUCCESS);
			return;
		}
	}
}

void mavlink_start(void)
{
	net_set_timeout_handler(5 * 1000, mavlink_timeout);
	net_set_udp_handler(mavlink_msg_handler);
	parsing_status.parse_state = MAVLINK_PARSE_STATE_UNINIT;
	parsing_status.flags = 0;

	if (net_boot_file_name[0] == '\0') {
		mavlink_src_ip.s_addr = DEFAULT_MAVLINK_SRC_IP;
		mavlink_port = DEFAULT_MAVLINK_PORT;
	} else {
		char *p = net_boot_file_name;

		p = strchr(p, ':');

		if (p != NULL) {
			mavlink_src_ip = string_to_ip(net_boot_file_name);
			++p;
			mavlink_port = simple_strtoul(p, NULL, 10);
		} else {
			mavlink_src_ip.s_addr = DEFAULT_MAVLINK_SRC_IP;
			mavlink_port = simple_strtoul(net_boot_file_name, NULL, 10);
		}
	}
}

/*************************************************************
 * MAVLINK PARSING IMPORTED FROM MAVLINK_HELPER, DO NOT EDIT *
 *************************************************************/

#define MAVLINK_HELPER static inline

#define X25_INIT_CRC 0xffff

#define _MAV_PAYLOAD_NON_CONST(msg) ((char *)(&((msg)->payload64[0])))
/*
  incompat_flags bits
 */
#define MAVLINK_IFLAG_SIGNED  0x01
#define MAVLINK_IFLAG_MASK    0x01 // mask of all understood bits

#define MAVLINK_STATUS_FLAG_IN_MAVLINK1  1 // last incoming packet was MAVLink1
#define MAVLINK_STATUS_FLAG_OUT_MAVLINK1 2 // generate MAVLink1 by default
#define MAVLINK_STATUS_FLAG_IN_SIGNED    4 // last incoming packet was signed and validated
#define MAVLINK_STATUS_FLAG_IN_BADSIG    8 // last incoming packet had a bad signature

#define MAVLINK_STX_MAVLINK1 0xFE          // marker for old protocol

/*
  entry in table of information about each message type
 */
typedef struct __mavlink_msg_entry {
	uint32_t msgid;
	uint8_t crc_extra;
        uint8_t min_msg_len;       // minimum message length
        uint8_t max_msg_len;       // maximum message length (e.g. including mavlink2 extensions)
        uint8_t flags;             // MAV_MSG_ENTRY_FLAG_*
	uint8_t target_system_ofs; // payload offset to target_system, or 0
	uint8_t target_component_ofs; // payload offset to target_component, or 0
} mavlink_msg_entry_t;

MAVLINK_HELPER mavlink_framing_t mavlink_frame_char_buffer(mavlink_message_t* rxmsg,
                                                 mavlink_status_t* status,
                                                 uint8_t c,
                                                 mavlink_message_t* r_message);

/* Returns 0 on successful parse, 1 otherwise*/
int parse_mavlink_msg(uchar *pkt, unsigned int len, unsigned int *start, mavlink_message_t *out_msg)
{
	mavlink_framing_t result;

	while (*start < len) {
		result = mavlink_frame_char_buffer(&parsing_msg, &parsing_status, pkt[(*start)++], out_msg);
		if (result != MAVLINK_FRAMING_INCOMPLETE) {
			if (result == MAVLINK_FRAMING_OK) {
				debug("Received message with ID %d, sequence: %d from component %d of system %d\n", out_msg->msgid, out_msg->seq, out_msg->compid, out_msg->sysid);
				return 0;
			} else if (result == MAVLINK_FRAMING_BAD_CRC) {
				debug("Received message with bad CRC\n");
				return 1;
			} else if (result == MAVLINK_FRAMING_BAD_SIGNATURE) {
				debug("Received message with bad signature\n");
				return 1;
			}
			parsing_status.parse_state = MAVLINK_PARSE_STATE_UNINIT;
			parsing_status.flags = 0;
		}
	}

	return 1;
}

/**
 * @brief Initiliaze the buffer for the X.25 CRC
 *
 * @param crcAccum the 16 bit X.25 CRC
 */
static inline void crc_init(uint16_t* crcAccum)
{
        *crcAccum = X25_INIT_CRC;
}

/**
 * @brief Accumulate the X.25 CRC by adding one char at a time.
 *
 * The checksum function adds the hash of one char at a time to the
 * 16 bit checksum (uint16_t).
 *
 * @param data new char to hash
 * @param crcAccum the already accumulated checksum
 **/
static inline void crc_accumulate(uint8_t data, uint16_t *crcAccum)
{
        /*Accumulate one byte of data into the CRC*/
        uint8_t tmp;

        tmp = data ^ (uint8_t)(*crcAccum &0xff);
        tmp ^= (tmp<<4);
        *crcAccum = (*crcAccum>>8) ^ (tmp<<8) ^ (tmp <<3) ^ (tmp>>4);
}

/**
 * @brief Calculates the X.25 checksum on a byte buffer
 *
 * @param  pBuffer buffer containing the byte array to hash
 * @param  length  length of the byte array
 * @return the checksum over the buffer bytes
 **/
static inline uint16_t crc_calculate(const uint8_t* pBuffer, uint16_t length)
{
        uint16_t crcTmp;
        crc_init(&crcTmp);
	while (length--) {
                crc_accumulate(*pBuffer++, &crcTmp);
        }
        return crcTmp;
}

MAVLINK_HELPER void mavlink_start_checksum(mavlink_message_t* msg)
{
	uint16_t crcTmp = 0;
	crc_init(&crcTmp);
	msg->checksum = crcTmp;
}

MAVLINK_HELPER void mavlink_update_checksum(mavlink_message_t* msg, uint8_t c)
{
	uint16_t checksum = msg->checksum;
	crc_accumulate(c, &checksum);
	msg->checksum = checksum;
}

static inline void _mav_parse_error(mavlink_status_t *status)
{
    status->parse_error++;
}

MAVLINK_HELPER const mavlink_msg_entry_t *mavlink_get_msg_entry(uint32_t msgid)
{
	static const mavlink_msg_entry_t mavlink_message_crcs[] = MAVLINK_MESSAGE_CRCS;
	/*
	use a bisection search to find the right entry. A perfect hash may be better
	Note that this assumes the table is sorted by msgid
	*/
	uint32_t low=0, high=sizeof(mavlink_message_crcs)/sizeof(mavlink_message_crcs[0]) - 1;
	while (low < high) {
		uint32_t mid = (low+1+high)/2;
		if (msgid < mavlink_message_crcs[mid].msgid) {
			high = mid-1;
			continue;
		}
		if (msgid > mavlink_message_crcs[mid].msgid) {
			low = mid;
			continue;
		}
		low = mid;
		break;
	}
	if (mavlink_message_crcs[low].msgid != msgid) {
	    // msgid is not in the table
	    return NULL;
	}
	return &mavlink_message_crcs[low];
}

/**
 * This is a variant of mavlink_frame_char() but with caller supplied
 * parsing buffers. It is useful when you want to create a MAVLink
 * parser in a library that doesn't use any global variables
 *
 * @param rxmsg    parsing message buffer
 * @param status   parsing status buffer
 * @param c        The char to parse
 *
 * @param r_message NULL if no message could be decoded, otherwise the message data
 * @param r_mavlink_status if a message was decoded, this is filled with the channel's stats
 * @return 0 if no message could be decoded, 1 on good message and CRC, 2 on bad CRC
 *
 */
MAVLINK_HELPER mavlink_framing_t mavlink_frame_char_buffer(mavlink_message_t* rxmsg,
														mavlink_status_t* status,
														uint8_t c,
														mavlink_message_t* r_message)
{
	/* Enable this option to check the length of each message.
	   This allows invalid messages to be caught much sooner. Use if the transmission
	   medium is prone to missing (or extra) characters (e.g. a radio that fades in
	   and out). Only use if the channel will only contain messages types listed in
	   the headers.
	*/

	int bufferIndex = 0;

	status->msg_received = MAVLINK_FRAMING_INCOMPLETE;

	switch (status->parse_state)
	{
	case MAVLINK_PARSE_STATE_UNINIT:
	case MAVLINK_PARSE_STATE_IDLE:
		if (c == MAVLINK_STX)
		{
			status->parse_state = MAVLINK_PARSE_STATE_GOT_STX;
			rxmsg->len = 0;
			rxmsg->magic = c;
			status->flags &= ~MAVLINK_STATUS_FLAG_IN_MAVLINK1;
			mavlink_start_checksum(rxmsg);
		} else if (c == MAVLINK_STX_MAVLINK1)
		{
			status->parse_state = MAVLINK_PARSE_STATE_GOT_STX;
			rxmsg->len = 0;
			rxmsg->magic = c;
			status->flags |= MAVLINK_STATUS_FLAG_IN_MAVLINK1;
			mavlink_start_checksum(rxmsg);
		}
		break;

	case MAVLINK_PARSE_STATE_GOT_STX:
			if (status->msg_received
/* Support shorter buffers than the
   default maximum packet size */
#if (MAVLINK_MAX_PAYLOAD_LEN < 255)
				|| c > MAVLINK_MAX_PAYLOAD_LEN
#endif
				)
		{
			status->buffer_overrun++;
			_mav_parse_error(status);
			status->msg_received = 0;
			status->parse_state = MAVLINK_PARSE_STATE_IDLE;
		}
		else
		{
			// NOT counting STX, LENGTH, SEQ, SYSID, COMPID, MSGID, CRC1 and CRC2
			rxmsg->len = c;
			status->packet_idx = 0;
			mavlink_update_checksum(rxmsg, c);
                        if (status->flags & MAVLINK_STATUS_FLAG_IN_MAVLINK1) {
                            rxmsg->incompat_flags = 0;
                            rxmsg->compat_flags = 0;
                            status->parse_state = MAVLINK_PARSE_STATE_GOT_COMPAT_FLAGS;
                        } else {
                            status->parse_state = MAVLINK_PARSE_STATE_GOT_LENGTH;
                        }
		}
		break;

	case MAVLINK_PARSE_STATE_GOT_LENGTH:
		rxmsg->incompat_flags = c;
		if ((rxmsg->incompat_flags & ~MAVLINK_IFLAG_MASK) != 0) {
			// message includes an incompatible feature flag
			_mav_parse_error(status);
			status->msg_received = 0;
			status->parse_state = MAVLINK_PARSE_STATE_IDLE;
			break;
		}
		mavlink_update_checksum(rxmsg, c);
		status->parse_state = MAVLINK_PARSE_STATE_GOT_INCOMPAT_FLAGS;
		break;

	case MAVLINK_PARSE_STATE_GOT_INCOMPAT_FLAGS:
		rxmsg->compat_flags = c;
		mavlink_update_checksum(rxmsg, c);
		status->parse_state = MAVLINK_PARSE_STATE_GOT_COMPAT_FLAGS;
		break;

	case MAVLINK_PARSE_STATE_GOT_COMPAT_FLAGS:
		rxmsg->seq = c;
		mavlink_update_checksum(rxmsg, c);
		status->parse_state = MAVLINK_PARSE_STATE_GOT_SEQ;
		break;

	case MAVLINK_PARSE_STATE_GOT_SEQ:
		rxmsg->sysid = c;
		mavlink_update_checksum(rxmsg, c);
		status->parse_state = MAVLINK_PARSE_STATE_GOT_SYSID;
		break;

	case MAVLINK_PARSE_STATE_GOT_SYSID:
		rxmsg->compid = c;
		mavlink_update_checksum(rxmsg, c);
                status->parse_state = MAVLINK_PARSE_STATE_GOT_COMPID;
		break;

	case MAVLINK_PARSE_STATE_GOT_COMPID:
		rxmsg->msgid = c;
		mavlink_update_checksum(rxmsg, c);
                if (status->flags & MAVLINK_STATUS_FLAG_IN_MAVLINK1) {
                    if(rxmsg->len > 0){
                        status->parse_state = MAVLINK_PARSE_STATE_GOT_MSGID3;
                    } else {
                        status->parse_state = MAVLINK_PARSE_STATE_GOT_PAYLOAD;
                    }
#ifdef MAVLINK_CHECK_MESSAGE_LENGTH
                    if (rxmsg->len != MAVLINK_MESSAGE_LENGTH(rxmsg->msgid))
                    {
			_mav_parse_error(status);
			status->parse_state = MAVLINK_PARSE_STATE_IDLE;
			break;
                    }
#endif
                } else {
                    status->parse_state = MAVLINK_PARSE_STATE_GOT_MSGID1;
                }
		break;

	case MAVLINK_PARSE_STATE_GOT_MSGID1:
		rxmsg->msgid |= c<<8;
		mavlink_update_checksum(rxmsg, c);
		status->parse_state = MAVLINK_PARSE_STATE_GOT_MSGID2;
		break;

	case MAVLINK_PARSE_STATE_GOT_MSGID2:
		rxmsg->msgid |= ((uint32_t)c)<<16;
		mavlink_update_checksum(rxmsg, c);
		if(rxmsg->len > 0){
			status->parse_state = MAVLINK_PARSE_STATE_GOT_MSGID3;
		} else {
			status->parse_state = MAVLINK_PARSE_STATE_GOT_PAYLOAD;
		}
#ifdef MAVLINK_CHECK_MESSAGE_LENGTH
	        if (rxmsg->len != MAVLINK_MESSAGE_LENGTH(rxmsg->msgid))
		{
			_mav_parse_error(status);
			status->parse_state = MAVLINK_PARSE_STATE_IDLE;
			break;
                }
#endif
		break;

	case MAVLINK_PARSE_STATE_GOT_MSGID3:
		_MAV_PAYLOAD_NON_CONST(rxmsg)[status->packet_idx++] = (char)c;
		mavlink_update_checksum(rxmsg, c);
		if (status->packet_idx == rxmsg->len)
		{
			status->parse_state = MAVLINK_PARSE_STATE_GOT_PAYLOAD;
		}
		break;

	case MAVLINK_PARSE_STATE_GOT_PAYLOAD:
		{
			const mavlink_msg_entry_t *e = mavlink_get_msg_entry(rxmsg->msgid);
			uint8_t crc_extra = e ? e->crc_extra : 0;
			mavlink_update_checksum(rxmsg, crc_extra);
			if (c != (rxmsg->checksum & 0xFF)) {
				status->parse_state = MAVLINK_PARSE_STATE_GOT_BAD_CRC1;
			} else {
				status->parse_state = MAVLINK_PARSE_STATE_GOT_CRC1;
			}
			rxmsg->ck[0] = c;

			// zero-fill the packet to cope with short incoming packets
			if (e && status->packet_idx < e->max_msg_len) {
				memset(&_MAV_PAYLOAD_NON_CONST(rxmsg)[status->packet_idx], 0, e->max_msg_len - status->packet_idx);
			}
		}
		break;

	case MAVLINK_PARSE_STATE_GOT_CRC1:
	case MAVLINK_PARSE_STATE_GOT_BAD_CRC1:
		if (status->parse_state == MAVLINK_PARSE_STATE_GOT_BAD_CRC1 || c != (rxmsg->checksum >> 8)) {
			// got a bad CRC message
			status->msg_received = MAVLINK_FRAMING_BAD_CRC;
		} else {
			// Successfully got message
			status->msg_received = MAVLINK_FRAMING_OK;
		}
		rxmsg->ck[1] = c;

		if (rxmsg->incompat_flags & MAVLINK_IFLAG_SIGNED) {
			status->parse_state = MAVLINK_PARSE_STATE_SIGNATURE_WAIT;
			status->signature_wait = MAVLINK_SIGNATURE_BLOCK_LEN;

			// If the CRC is already wrong, don't overwrite msg_received,
			// otherwise we can end up with garbage flagged as valid.
			if (status->msg_received != MAVLINK_FRAMING_BAD_CRC) {
				status->msg_received = MAVLINK_FRAMING_INCOMPLETE;
			}
		} else {
			status->parse_state = MAVLINK_PARSE_STATE_IDLE;
			if (r_message != NULL) {
				memcpy(r_message, rxmsg, sizeof(mavlink_message_t));
			}
		}
		break;
	case MAVLINK_PARSE_STATE_SIGNATURE_WAIT:
		rxmsg->signature[MAVLINK_SIGNATURE_BLOCK_LEN-status->signature_wait] = c;
		status->signature_wait--;
		if (status->signature_wait == 0) {
			// we have the whole signature, check it is OK
			status->msg_received = MAVLINK_FRAMING_OK;
			status->parse_state = MAVLINK_PARSE_STATE_IDLE;
			if (r_message !=NULL) {
				memcpy(r_message, rxmsg, sizeof(mavlink_message_t));
			}
		}
		break;
	}

	bufferIndex++;
	// If a message has been sucessfully decoded, check index
	if (status->msg_received == MAVLINK_FRAMING_OK)
	{
		//while(status->current_seq != rxmsg->seq)
		//{
		//	status->packet_rx_drop_count++;
		//               status->current_seq++;
		//}
		status->current_rx_seq = rxmsg->seq;
		// Initial condition: If no packet has been received so far, drop count is undefined
		if (status->packet_rx_success_count == 0) status->packet_rx_drop_count = 0;
		// Count this packet as received
		status->packet_rx_success_count++;
	}

    if (r_message != NULL) {
        r_message->len = rxmsg->len; // Provide visibility on how far we are into current msg
    }
    status->current_rx_seq++;
    status->parse_error = 0;

	if (status->msg_received == MAVLINK_FRAMING_BAD_CRC) {
		/*
		  the CRC came out wrong. We now need to overwrite the
		  msg CRC with the one on the wire so that if the
		  caller decides to forward the message anyway that
		  mavlink_msg_to_send_buffer() won't overwrite the
		  checksum
		 */
		if (r_message != NULL) {
			r_message->checksum = rxmsg->ck[0] | (rxmsg->ck[1]<<8);
		}
	}

	return status->msg_received;
}