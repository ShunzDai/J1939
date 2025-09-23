/**
  * Copyright 2022 ShunzDai
  *
  * Licensed under the Apache License, Version 2.0 (the "License");
  * you may not use this file except in compliance with the License.
  * You may obtain a copy of the License at
  *
  *     http://www.apache.org/licenses/LICENSE-2.0
  *
  * Unless required by applicable law or agreed to in writing, software
  * distributed under the License is distributed on an "AS IS" BASIS,
  * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  * See the License for the specific language governing permissions and
  * limitations under the License.
  */
#include "j1939.h"
#include "j1939_port.h"
#if defined J1939_PORT_VIRTUAL
#include "j1939_virtual.h"
#endif /* J1939_PORT_VIRTUAL */
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdio.h>

#define J1939_SIZE_PROTOCOL_PAYLOAD         (J1939_SIZE_DATAFIELD - 1)

#define J1939_ADDRESS_DIVIDE                0xF0/* DO NOT MODIFIED THIS PRAMETER */
#define J1939_ADDRESS_NULL                  0xFE/* DO NOT MODIFIED THIS PRAMETER */
#define J1939_ADDRESS_GLOBAL                0xFF/* DO NOT MODIFIED THIS PRAMETER */

/* Reference SAE J1939-21 5.10.1.1 */
/* min size = 9, max size = 1785 */
#define J1939_TP_MAX_MSG_SIZE              (UCHAR_MAX * J1939_SIZE_PROTOCOL_PAYLOAD)
/* Config response packets number of TP CTS */
#define J1939_TP_CM_CTS_RESPONSE            4

#define J1939_TP_DEFAULT_PRIORITY           0x07

#define J1939_TP_BAM_TX_INTERVAL            50

/* Reference https://elearning.vector.com/mod/page/view.php?id=422 */
/* Reference SAE J1939-81 */
/* Used for identification of an ECU and for detection of address conflicts */
#define J1939_PGN_ADDR_CLAIMED              0x00EE00
/* Reference SAE J1939-21 */
/* Other pgns can be requested using this pgn, similarly as for the CAN Remote Frame. */
/* But note: j1939 does not support Remote Frames. The Request pgn is a CAN data frame. */
#define J1939_PGN_REQUEST                   0x00EA00
/* Reference SAE J1939-21 */
/* Transmits the payload data for the transport protocols */
#define J1939_PGN_TP_DT                     0x00EB00
/* Reference SAE J1939-21 */
/* Supplies the metadata (number of bytes, packets, etc.) for transport protocols */
#define J1939_PGN_TP_CM                     0x00EC00
/* Reference SAE J1939-21 */
/* Manufacturer-specific definable specific pgn */
#define J1939_PGN_PROPRIETARY_A             0x00EF00
/* Reference SAE J1939-21 */
/* Manufacturer-specific definable additional specific pgn */
#define J1939_PGN_PROPRIETARY_A1            0x01EF00
/* Reference SAE J1939-21 */
/* Used for acknowledgement of various network services. Can be positive or negative. */
/* The acknowledgement is referenced accordingly in the application layer. */
#define J1939_PGN_ACKNOWLEDGEMENT           0x00E800

/* transport protocol internal status */
typedef enum j1939_tp_status {
  /* ready to transmit/receive transport self messages */
  J1939_TP_READY,
  J1939_TP_COMPLETE_TX,
  J1939_TP_COMPLETE_RX,
  J1939_TP_CM_ABORT_TX,
  J1939_TP_CM_CTS_TX,
  J1939_TP_CM_CTS_RX,
  J1939_TP_CM_ACK_TX,
  J1939_TP_CM_ACK_RX,
  J1939_TP_DT_BAM_TX,
  J1939_TP_DT_BAM_RX,
  J1939_TP_DT_CMDT_TX,
  J1939_TP_DT_CMDT_RX,
} j1939_tp_status_t;

/* Transport self timeout parameters */
typedef enum j1939_timeout {
  /* Reference SAE J1939-21 5.10.2.4 */
  J1939_TIMEOUT_TR                          = 200,
  J1939_TIMEOUT_TH                          = 500,
  J1939_TIMEOUT_T1                          = 750,
  J1939_TIMEOUT_T2                          = 1250,
  J1939_TIMEOUT_T3                          = 1250,
  J1939_TIMEOUT_T4                          = 1050,
} j1939_timeout_t;

typedef enum j1939_control{
  J1939_CONTROL_RTS                         = 0x10U,
  J1939_CONTROL_CTS                         = 0x11U,
  J1939_CONTROL_ACK                         = 0x13U,
  J1939_CONTROL_BAM                         = 0x20U,
  J1939_CONTROL_ABORT                       = 0xFFU,
} j1939_control_t;

/* Transport self - broadcast announce message structure */
typedef struct j1939_bam {
  /* Fixed at 32 */
  uint64_t control                          : 8;
  /* Message size in bytes */
  uint64_t message_size                     : 16;
  /* Number of packets */
  uint64_t total_packets                    : 8;
  /* Fixed at 0xFF */
  uint64_t reserved                         : 8;
  /* pgn */
  uint64_t pgn                              : 24;
} j1939_bam_t;

/* Transport self - request to send struct */
typedef struct j1939_rts {
  /* Fixed at 16 */
  uint64_t control                          : 8;
  /* Message size in bytes */
  uint64_t message_size                     : 16;
  /* Number of packets */
  uint64_t total_packets                    : 8;
  /* Fixed at 0xFF */
  uint64_t reserved                         : 8;
  /* pgn */
  uint64_t pgn                              : 24;
} j1939_rts_t;

/* Transport self - clear to send struct */
typedef struct j1939_cts {
  /* Fixed at 17 */
  uint64_t control                          : 8;
  /* Max number of packets that can be sent at once. (Not larger than byte 5 of RTS) */
  uint64_t response_packets                  : 8;
  /* Next sequence number to start with */
  uint64_t next_sequence                     : 8;
  /* Fixed at 0xFFFF */
  uint64_t reserved                         : 16;
  /* pgn */
  uint64_t pgn                              : 24;
} j1939_cts_t;

/* Transport self - EndofmsgACK_t struct */
typedef struct j1939_ack {
  /* Fixed at 19 */
  uint64_t control                          : 8;
  /* Total message size in bytes */
  uint64_t message_size                     : 16;
  /* Total number of packets */
  uint64_t total_packets                    : 8;
  /* Fixed at 0xFF */
  uint64_t reserved                         : 8;
  /* pgn */
  uint64_t pgn                              : 24;
} j1939_ack_t;

/* Transport self - Connection abort struct */
typedef struct j1939_abort {
  /* Fixed at 255 */
  uint64_t control                          : 8;
  /* Connection abort reason */
  uint64_t reason                           : 8;
  /* Fixed at 0xFFFFFF */
  uint64_t reserved                         : 24;
  /* pgn */
  uint64_t pgn                              : 24;
} j1939_abort_t;

struct j1939 {
  uint8_t self_address;
  uint8_t total_packets;
  uint8_t packets_count;
  uint8_t response_packets;
  uint8_t abort_reason;
  uint32_t tick;
  j1939_tp_status_t status;
  j1939_cb_t recv_cb;
  j1939_cb_t timeout_cb;
  j1939_port_t *port;
  j1939_message_t *lmsg;
  void *arg;
};

static inline uint8_t get_total_packets(uint16_t size) {
  return (size - 1) / J1939_SIZE_PROTOCOL_PAYLOAD + 1;
}

static inline uint8_t get_last_section(uint16_t size) {
  return (size % J1939_SIZE_PROTOCOL_PAYLOAD) ? (size % J1939_SIZE_PROTOCOL_PAYLOAD) : (J1939_SIZE_PROTOCOL_PAYLOAD);
}

uint32_t j1939_get_pgn(uint32_t pdu) {
  /* Reference SAE J1939-21 5.1.2 */
  return ((((j1939_pdu_t *)&pdu)->reserved << 17 | ((j1939_pdu_t *)&pdu)->data_page << 16) | (((j1939_pdu_t *)&pdu)->pdu_format < J1939_ADDRESS_DIVIDE)) ?
         (((j1939_pdu_t *)&pdu)->pdu_format << 8) : (((j1939_pdu_t *)&pdu)->pdu_format << 8 | ((j1939_pdu_t *)&pdu)->pdu_specific);
}

void j1939_set_pgn(uint32_t *pdu, const uint32_t pgn) {
  /* Reference SAE J1939-21 5.1.2 */
  ((j1939_pdu_t *)pdu)->reserved = (pgn >> 17) & 0x01;
  ((j1939_pdu_t *)pdu)->data_page = (pgn >> 16) & 0x01;
  ((j1939_pdu_t *)pdu)->pdu_format = (pgn >> 8) & 0xFF;
  if (((j1939_pdu_t *)pdu)->pdu_format >= J1939_ADDRESS_DIVIDE)
    ((j1939_pdu_t *)pdu)->pdu_specific = (pgn >> 0) & 0xFF;
}

static j1939_status_t j1939_tp_cm_rts_transmit_manager(j1939_t *self, uint32_t timeout_ms) {
  j1939_status_t res = J1939_OK;
  j1939_static_message_t m = { .size = J1939_SIZE_DATAFIELD, };

  m.pdu.source_address = self->lmsg->pdu.source_address;
  m.pdu.pdu_specific = self->lmsg->pdu.pdu_specific;
  m.pdu.priority = J1939_TP_DEFAULT_PRIORITY;
  j1939_set_pgn(&m.id, J1939_PGN_TP_CM);
  ((j1939_rts_t *)m.data)->control = J1939_CONTROL_RTS;
  ((j1939_rts_t *)m.data)->message_size = self->lmsg->size;
  ((j1939_rts_t *)m.data)->total_packets = self->total_packets;
  ((j1939_rts_t *)m.data)->pgn = j1939_get_pgn(self->lmsg->id);
  ((j1939_rts_t *)m.data)->reserved = 0xFF;

  if ((res = j1939_port_transmit(self->port, &m, timeout_ms)) == J1939_OK) {
    self->status = J1939_TP_CM_CTS_RX;
    self->tick = j1939_port_get_tick();
  }

  return res;
}

static j1939_status_t j1939_tp_cm_rts_receive_manager(j1939_t *self, j1939_static_message_t *msg){
  if (self->status != J1939_TP_READY)
    return J1939_ERROR;

  self->lmsg = j1939_message_create(0, NULL, ((j1939_rts_t *)msg->data)->message_size);
  self->lmsg->pdu.source_address = msg->pdu.source_address;
  self->lmsg->pdu.pdu_specific = msg->pdu.pdu_specific;
  j1939_set_pgn(&self->lmsg->id, ((j1939_rts_t *)msg->data)->pgn);

  self->total_packets = ((j1939_rts_t *)msg->data)->total_packets;

  self->status = J1939_TP_CM_CTS_TX;

  self->tick = j1939_port_get_tick();

  return J1939_OK;
}

static j1939_status_t j1939_tp_cm_cts_transmit_manager(j1939_t *self) {
  j1939_status_t res = J1939_OK;
  j1939_static_message_t m = { .size = J1939_SIZE_DATAFIELD, };

  m.pdu.source_address = self->lmsg->pdu.pdu_specific;
  m.pdu.pdu_specific = self->lmsg->pdu.source_address;
  m.pdu.priority = J1939_TP_DEFAULT_PRIORITY;
  j1939_set_pgn(&m.id, J1939_PGN_TP_CM);
  ((j1939_cts_t *)m.data)->control = J1939_CONTROL_CTS;
  ((j1939_cts_t *)m.data)->next_sequence = self->packets_count + 1;
  ((j1939_cts_t *)m.data)->pgn = j1939_get_pgn(self->lmsg->id);
  ((j1939_cts_t *)m.data)->reserved = 0xFFFF;
  ((j1939_cts_t *)m.data)->response_packets = (self->total_packets - self->packets_count < J1939_TP_CM_CTS_RESPONSE) ? self->total_packets - self->packets_count : J1939_TP_CM_CTS_RESPONSE;

  self->response_packets = ((j1939_cts_t *)m.data)->response_packets;

  if ((res = j1939_port_transmit(self->port, &m, J1939_TIMEOUT_TR)) == J1939_OK) {
    self->status = J1939_TP_DT_CMDT_RX;
    self->tick = j1939_port_get_tick();
  }

  return res;
}

static j1939_status_t j1939_tp_cm_cts_receive_manager(j1939_t *self, j1939_static_message_t *msg) {
  if (self->status != J1939_TP_CM_CTS_RX)
    return J1939_ERROR;
  else if (j1939_get_pgn(self->lmsg->id) != ((j1939_cts_t *)msg->data)->pgn)
    return J1939_ERROR;
  else if (self->packets_count + 1 != ((j1939_cts_t *)msg->data)->next_sequence)
    return J1939_ERROR;

  self->response_packets = ((j1939_cts_t *)msg->data)->response_packets;

  self->status = J1939_TP_DT_CMDT_TX;

  self->tick = j1939_port_get_tick();

  return J1939_OK;
}

static j1939_status_t j1939_tp_cm_ack_transmit_manager(j1939_t *self) {
  j1939_status_t res = J1939_OK;
  j1939_static_message_t m = { .size = J1939_SIZE_DATAFIELD, };

  m.pdu.source_address = self->lmsg->pdu.pdu_specific;
  m.pdu.pdu_specific = self->lmsg->pdu.source_address;
  m.pdu.priority = J1939_TP_DEFAULT_PRIORITY;
  j1939_set_pgn(&m.id, J1939_PGN_TP_CM);

  ((j1939_ack_t *)m.data)->control = J1939_CONTROL_ACK;
  ((j1939_ack_t *)m.data)->message_size = self->lmsg->size;
  ((j1939_ack_t *)m.data)->pgn = j1939_get_pgn(self->lmsg->id);
  ((j1939_ack_t *)m.data)->reserved = 0xFF;
  ((j1939_ack_t *)m.data)->total_packets = self->total_packets;

  if ((res = j1939_port_transmit(self->port, &m, J1939_TIMEOUT_TR)) == J1939_OK) {
    self->status = J1939_TP_COMPLETE_RX;
    self->tick = j1939_port_get_tick();
  }

  return res;
}

static j1939_status_t j1939_tp_cm_ack_receive_manager(j1939_t *self, j1939_static_message_t *msg) {
  if (self->status != J1939_TP_CM_ACK_RX)
    return J1939_ERROR;
  else if (j1939_get_pgn(self->lmsg->id) != ((j1939_ack_t *)msg->data)->pgn)
    return J1939_ERROR;
  else if (self->lmsg->size != ((j1939_ack_t *)msg->data)->message_size)
    return J1939_ERROR;
  else if (self->total_packets != ((j1939_ack_t *)msg->data)->total_packets)
    return J1939_ERROR;

  self->status = J1939_TP_COMPLETE_TX;

  return J1939_OK;
}

static j1939_status_t j1939_tp_cm_bam_transmit_manager(j1939_t *self, uint32_t timeout_ms) {
  j1939_status_t res = J1939_OK;
  j1939_static_message_t m = { .size = J1939_SIZE_DATAFIELD, };

  m.pdu.source_address = self->lmsg->pdu.source_address;
  m.pdu.pdu_specific = J1939_ADDRESS_GLOBAL;
  m.pdu.priority = J1939_TP_DEFAULT_PRIORITY;
  j1939_set_pgn(&m.id, J1939_PGN_TP_CM);
  ((j1939_bam_t *)&m.data)->control = J1939_CONTROL_BAM;
  ((j1939_bam_t *)&m.data)->message_size = self->lmsg->size;
  ((j1939_bam_t *)&m.data)->total_packets = self->total_packets;
  ((j1939_bam_t *)&m.data)->pgn = j1939_get_pgn(self->lmsg->id);
  ((j1939_bam_t *)&m.data)->reserved = 0xFF;

  if ((res = j1939_port_transmit(self->port, &m, timeout_ms)) == J1939_OK) {
    self->status = J1939_TP_DT_BAM_TX;
    self->tick = j1939_port_get_tick();
  }

  return res;
}

static j1939_status_t j1939_tp_cm_bam_receive_manager(j1939_t *self, j1939_static_message_t *msg) {
  if (self->status != J1939_TP_READY)
    return J1939_ERROR;

  self->lmsg = j1939_message_create(0, NULL, ((j1939_bam_t *)msg->data)->message_size);
  self->lmsg->pdu.source_address = msg->pdu.source_address;
  self->lmsg->pdu.pdu_specific = msg->pdu.pdu_specific;
  j1939_set_pgn(&self->lmsg->id, ((j1939_bam_t *)msg->data)->pgn);

  self->total_packets = ((j1939_bam_t *)msg->data)->total_packets;

  self->status = J1939_TP_DT_BAM_RX;
  self->tick = j1939_port_get_tick();

  return J1939_OK;
}

// static j1939_status_t j1939_tp_cm_abort_transmit_manager(j1939_t *self, uint32_t timeout_ms) {
//   j1939_status_t res = J1939_OK;
//   j1939_static_message_t m = { .size = J1939_SIZE_DATAFIELD, };

//   m.pdu.source_address = self->lmsg->pdu.pdu_specific;
//   m.pdu.pdu_specific = self->lmsg->pdu.source_address;
//   m.pdu.priority = J1939_TP_DEFAULT_PRIORITY;
//   j1939_set_pgn(&m.id, J1939_PGN_TP_CM);

//   ((j1939_abort_t *)m.data)->control = J1939_CONTROL_ABORT;
//   ((j1939_abort_t *)m.data)->reason = (uint8_t)self->abort_reason;
//   ((j1939_abort_t *)m.data)->reserved = 0xFFFFFF;
//   ((j1939_abort_t *)m.data)->pgn = j1939_get_pgn(self->lmsg->id);

//   if ((res = j1939_port_transmit(self->port, &m, timeout_ms)) == J1939_OK) {
//     self->status = J1939_TP_READY;
//     self->tick = j1939_port_get_tick();
//   }

//   return res;
// }

static j1939_status_t j1939_tp_cm_abort_receive_manager(j1939_t *self, j1939_static_message_t *msg){
  if (j1939_get_pgn(self->lmsg->id) != ((j1939_abort_t *)msg->data)->pgn)
    return J1939_ERROR;

  // J1939_MessageDelete(&self->lmsg);

  memset(self, 0, sizeof(struct j1939));

  return J1939_OK;
}

static j1939_status_t j1939_tp_dt_transmit_manager(j1939_t *self, uint32_t timeout_ms) {
  j1939_status_t res = J1939_OK;
  j1939_static_message_t m = { .size = J1939_SIZE_DATAFIELD, };
  uint8_t section = J1939_SIZE_PROTOCOL_PAYLOAD;

  m.pdu.source_address = self->lmsg->pdu.source_address;
  m.pdu.pdu_specific = self->status == J1939_TP_DT_BAM_TX ? J1939_ADDRESS_GLOBAL : self->lmsg->pdu.pdu_specific;
  m.pdu.priority = J1939_TP_DEFAULT_PRIORITY;
  j1939_set_pgn(&m.id, J1939_PGN_TP_DT);

  if (self->packets_count + 1 == self->total_packets) {
    section = get_last_section(self->lmsg->size);
    memset(&m.data[1] + section, 0xFF, J1939_SIZE_PROTOCOL_PAYLOAD - section);
  }

  m.data[0] = self->packets_count + 1;
  memcpy(&m.data[1], self->lmsg->data + self->packets_count * J1939_SIZE_PROTOCOL_PAYLOAD, section);

  if ((res = j1939_port_transmit(self->port, &m, timeout_ms)) == J1939_OK) {
    self->packets_count += 1;
    switch (self->status) {
      case J1939_TP_DT_BAM_TX:
        if (self->packets_count == self->total_packets)
          self->status = J1939_TP_COMPLETE_TX;
        break;
      case J1939_TP_DT_CMDT_TX:
        if (self->packets_count == self->total_packets)
          self->status = J1939_TP_CM_ACK_RX;
        else if (--self->response_packets == 0)
          self->status = J1939_TP_CM_CTS_RX;
        break;
      default:
        break;
    }
    self->tick = j1939_port_get_tick();
  }

  return res;
}

static j1939_status_t j1939_tp_dt_receive_manager(j1939_t *self, j1939_static_message_t *msg) {
  uint8_t section = J1939_SIZE_PROTOCOL_PAYLOAD;

  if (self->packets_count + 1 != msg->data[0]) {
    /* TODO: TP_CM_CTS_TX */
    return J1939_OK;
  }

  if (++self->packets_count == self->total_packets) {
    section = get_last_section(self->lmsg->size);
    switch (self->status) {
      case J1939_TP_DT_BAM_RX:
        self->status = J1939_TP_COMPLETE_RX;
        break;
      case J1939_TP_DT_CMDT_RX:
        j1939_tp_cm_ack_transmit_manager(self);
        break;
      default:
        break;
    }
  }
  else if (self->status == J1939_TP_DT_CMDT_RX) {
    if (--self->response_packets == 0)
      self->status = J1939_TP_CM_CTS_TX;
  }

  memcpy(self->lmsg->data + (self->packets_count - 1) * J1939_SIZE_PROTOCOL_PAYLOAD, &msg->data[1], section);

  self->tick = j1939_port_get_tick();

  return J1939_OK;
}

static j1939_status_t j1939_tp_dt_bam_transmit_manager(j1939_t *self) {
  return j1939_port_get_tick() - self->tick < J1939_TP_BAM_TX_INTERVAL ? J1939_BLOCKED : j1939_tp_dt_transmit_manager(self, J1939_TIMEOUT_TR);
}

static j1939_status_t j1939_tp_dt_cmdt_transmit_manager(j1939_t *self) {
  return self->response_packets ? j1939_tp_dt_transmit_manager(self, J1939_TIMEOUT_T3) : J1939_ERROR;
}

j1939_status_t j1939_tp_cm_transmit_helper(j1939_t *self, uint32_t timeout_ms, j1939_status_t (*func)(j1939_t *)) {
  return j1939_port_get_tick() - self->tick < timeout_ms ? func(self) : J1939_TIMEOUT;
}

j1939_status_t j1939_tp_cm_transmit_manager(j1939_t *self, uint32_t timeout_ms) {
  j1939_status_t res = J1939_OK;
  switch (self->status) {
    case J1939_TP_CM_CTS_TX:
      res = j1939_tp_cm_transmit_helper(self, J1939_TIMEOUT_TR, j1939_tp_cm_cts_transmit_manager);
      break;
    case J1939_TP_DT_BAM_TX:
      res = j1939_tp_cm_transmit_helper(self, J1939_TIMEOUT_TR, j1939_tp_dt_bam_transmit_manager);
      break;
    case J1939_TP_DT_CMDT_TX:
      res = j1939_tp_cm_transmit_helper(self, J1939_TIMEOUT_T3, j1939_tp_dt_cmdt_transmit_manager);
      break;
    default:
      res = J1939_ERROR;
      break;
  }
  return res;
}

static j1939_status_t j1939_tp_cm_receive_manager(j1939_t *self, j1939_static_message_t *msg) {
  j1939_status_t res = J1939_OK;
  switch ((j1939_control_t)msg->data[0]) {
    case J1939_CONTROL_RTS:
      res = j1939_tp_cm_rts_receive_manager(self, msg);
      break;
    case J1939_CONTROL_CTS:
      res = j1939_tp_cm_cts_receive_manager(self, msg);
      break;
    case J1939_CONTROL_ACK:
      res = j1939_tp_cm_ack_receive_manager(self, msg);
      break;
    case J1939_CONTROL_BAM:
      res = j1939_tp_cm_bam_receive_manager(self, msg);
      break;
    case J1939_CONTROL_ABORT:
      res = j1939_tp_cm_abort_receive_manager(self, msg);
      break;
    default:
      res = J1939_ERROR;
  }
  return res;
}

j1939_status_t j1939_receive_filter(j1939_t *self, const j1939_message_t *msg) {
  j1939_status_t res = J1939_OK;
  /* pdu1 filter */
  if (msg->pdu.pdu_format < J1939_ADDRESS_DIVIDE) {
    if (msg->pdu.pdu_specific != self->self_address && msg->pdu.pdu_specific != J1939_ADDRESS_GLOBAL)
      res = J1939_ERROR;
  }
  return res;
}

j1939_message_t *j1939_message_create(uint32_t id, const void *data, uint16_t size) {
  if (size > J1939_TP_MAX_MSG_SIZE)
    return NULL;
  j1939_message_t *self = (j1939_message_t *)malloc(sizeof(j1939_message_t) + size);
  self->id = id;
  self->size = size;
  data ? memcpy(self->data, data, size) : memset(self->data, 0, size);
  return self;
}

void j1939_message_delete(j1939_message_t *msg) {
  free(msg);
}

j1939_t *j1939_create(j1939_config_t *config) {
  j1939_t *self = (j1939_t *)calloc(1, sizeof(struct j1939));
  self->port = config->port;
  self->self_address = config->self_address;
  self->recv_cb = config->recv_cb;
  self->timeout_cb = config->timeout_cb;
  self->arg = config->arg;
  #if defined J1939_PORT_VIRTUAL
  j1939_virtual_add_node(self->port);
  #endif /* J1939_PORT_VIRTUAL */
  return self;
}

j1939_status_t j1939_delete(j1939_t *self) {
  if (self->lmsg)
    j1939_message_delete(self->lmsg);

  free(self);
  return J1939_OK;
}

j1939_status_t j1939_transmit(j1939_t *self, const j1939_message_t *msg, uint32_t timeout_ms) {
  j1939_status_t res = J1939_OK;
  if (self->status >= J1939_TP_COMPLETE_RX /*!= J1939_TP_READY*/)
    res = J1939_BUSY;
  else if (msg->size > J1939_TP_MAX_MSG_SIZE)
    res = J1939_ERROR;
  else if (msg->size > J1939_SIZE_DATAFIELD) {
    self->lmsg = (j1939_message_t *)msg;
    self->total_packets = get_total_packets(msg->size);
    self->packets_count = 0;
    if (msg->pdu.pdu_format < J1939_ADDRESS_DIVIDE)
      res = j1939_tp_cm_rts_transmit_manager(self, timeout_ms);
    else
      res = j1939_tp_cm_bam_transmit_manager(self, timeout_ms);
  }
  else
    res = j1939_port_transmit(self->port, (const j1939_static_message_t *)msg, timeout_ms);
  return res;
}

j1939_status_t j1939_receive(j1939_t *self, uint32_t timeout_ms) {
  j1939_status_t res = J1939_OK;
  j1939_static_message_t m = { .size = J1939_SIZE_DATAFIELD, };
  if ((res = j1939_port_receive(self->port, &m, timeout_ms)) == J1939_OK && (res = j1939_receive_filter(self, (j1939_message_t *)&m)) == J1939_OK) {
    switch (j1939_get_pgn(m.id)) {
      case J1939_PGN_TP_CM:
        j1939_tp_cm_receive_manager(self, &m);
        break;
      case J1939_PGN_TP_DT:
        j1939_tp_dt_receive_manager(self, &m);
        if (self->status == J1939_TP_COMPLETE_RX) {
          self->recv_cb(self->port, self->lmsg, self->arg);
          self->status = J1939_TP_READY;
        }
        break;
      default:
        self->recv_cb(self->port, (j1939_message_t *)&m, self->arg);
        break;
    }
  }
  return res;
}

j1939_status_t j1939_status(j1939_t *self) {
  return (self->status == J1939_TP_READY) ? J1939_OK : J1939_BUSY;
}

