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
#ifndef J1939_SRC_H
#define J1939_SRC_H
#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include "j1939_types.h"

typedef void (*j1939_cb_t)(j1939_port_t *port, const j1939_pdu_t *msg);

typedef j1939_status_t (*j1939_send_t)(j1939_port_t *self, const j1939_spdu_t *msg, int timeout_ms);

typedef j1939_status_t (*j1939_read_t)(j1939_port_t *self, j1939_spdu_t *msg, int timeout_ms);

typedef uint32_t (*j1939_tick_t)(j1939_port_t *self);

typedef struct j1939_config {
  uint8_t self_address;
  j1939_cb_t recv_cb;
  j1939_cb_t timeout_cb;
  j1939_send_t send;
  j1939_read_t read;
  j1939_tick_t tick;
  j1939_port_t *port;
} j1939_config_t;

typedef struct j1939 j1939_t;

j1939_pgn_t j1939_get_pgn(j1939_id_t id);

void j1939_set_pgn(j1939_id_t *id, j1939_pgn_t pgn);

j1939_pdu_t *j1939_pdu_create(j1939_id_t id, const void *data, uint16_t size);

void j1939_pdu_delete(j1939_pdu_t *msg);

j1939_t *j1939_create(j1939_config_t *config);

j1939_status_t j1939_delete(j1939_t *self);

j1939_status_t j1939_status(j1939_t *self);

j1939_status_t j1939_transmit_static(j1939_t *self, const j1939_spdu_t *msg, int timeout_ms);

j1939_status_t j1939_transmit(j1939_t *self, const j1939_pdu_t *msg, int timeout_ms);

j1939_status_t j1939_receive_static(j1939_t *self, j1939_spdu_t *msg, int timeout_ms);

j1939_status_t j1939_receive(j1939_t *self, int timeout_ms);

j1939_status_t j1939_tp_cm_transmit_manager(j1939_t *self, int timeout_ms);

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* J1939_SRC_H */
