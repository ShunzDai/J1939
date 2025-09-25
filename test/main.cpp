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
#include "j1939_virtual.h"
#include <stdio.h>

static auto recv_cb = +[](j1939_port_t *port, const j1939_pdu_t *msg) {
  printf("port [%02lX] recv id [%08X] size [%d] data [", (size_t)port, msg->id.u32, msg->size);
  for (uint16_t idx = 0; idx < msg->size; ++idx) {
    printf("%02X%s", msg->data[idx], idx == msg->size - 1 ? "]\n" : " ");
  }
};

static auto timeout_cb = +[](j1939_port_t *port, const j1939_pdu_t *msg) {
  printf("port [%02lX] timeout id [%08X] size [%d] data [", (size_t)port, msg->id.u32, msg->size);
  for (uint16_t idx = 0; idx < msg->size; ++idx) {
    printf("%02X%s", msg->data[idx], idx == msg->size - 1 ? "]\n" : " ");
  }
};

void test1() {
  j1939_config_t config[] = {
    {
      .self_address = 0x00,
      .recv_cb = recv_cb,
      .timeout_cb = timeout_cb,
      .send = j1939_virtual_transmit,
      .read = j1939_virtual_receive,
      .tick = j1939_virtual_get_tick,
      .port = 0,
    },
    {
      .self_address = 0x01,
      .recv_cb = recv_cb,
      .timeout_cb = timeout_cb,
      .send = j1939_virtual_transmit,
      .read = j1939_virtual_receive,
      .tick = j1939_virtual_get_tick,
      .port = (j1939_port_t *)1,
    },
  };

  j1939_virtual_add_node(config[0].port);
  j1939_virtual_add_node(config[1].port);

  j1939_t *bus[] = {j1939_create(&config[0]), j1939_create(&config[1])};

  j1939_id_t id = {
    .source_address = 0x00,
    .pdu_specific = 0x01,
    .pdu_format = 0xE0,
    .data_page = 0,
    .ex_data_page = 0,
    .priority = 0b110,
  };

  j1939_transmit(bus[0], j1939_pdu_create(id, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz1234567890", 62), -1);

  printf(">>> %d\n", j1939_receive(bus[0], -1));
  printf(">>> %d\n", j1939_tp_cm_transmit_manager(bus[0], -1));
  printf(">>> %d\n", j1939_receive(bus[1], -1));
  printf(">>> %d\n", j1939_tp_cm_transmit_manager(bus[1], -1));

  printf(">>> %d\n", j1939_receive(bus[0], -1));
  printf(">>> %d\n", j1939_tp_cm_transmit_manager(bus[0], -1));
  printf(">>> %d\n", j1939_receive(bus[1], -1));
  printf(">>> %d\n", j1939_tp_cm_transmit_manager(bus[1], -1));

  printf(">>> %d\n", j1939_receive(bus[0], -1));
  printf(">>> %d\n", j1939_tp_cm_transmit_manager(bus[0], -1));
  printf(">>> %d\n", j1939_receive(bus[1], -1));
  printf(">>> %d\n", j1939_tp_cm_transmit_manager(bus[1], -1));

  printf(">>> %d\n", j1939_receive(bus[0], -1));
  printf(">>> %d\n", j1939_tp_cm_transmit_manager(bus[0], -1));
  printf(">>> %d\n", j1939_receive(bus[1], -1));
  printf(">>> %d\n", j1939_tp_cm_transmit_manager(bus[1], -1));

  printf(">>> %d\n", j1939_receive(bus[0], -1));
  printf(">>> %d\n", j1939_tp_cm_transmit_manager(bus[0], -1));
  printf(">>> %d\n", j1939_receive(bus[1], -1));
  printf(">>> %d\n", j1939_tp_cm_transmit_manager(bus[1], -1));

  printf(">>> %d\n", j1939_receive(bus[0], -1));
  printf(">>> %d\n", j1939_tp_cm_transmit_manager(bus[0], -1));
  printf(">>> %d\n", j1939_receive(bus[1], -1));
  printf(">>> %d\n", j1939_tp_cm_transmit_manager(bus[1], -1));

  printf(">>> %d\n", j1939_receive(bus[0], -1));
  printf(">>> %d\n", j1939_tp_cm_transmit_manager(bus[0], -1));
  printf(">>> %d\n", j1939_receive(bus[1], -1));
  printf(">>> %d\n", j1939_tp_cm_transmit_manager(bus[1], -1));

  printf(">>> %d\n", j1939_receive(bus[0], -1));
  printf(">>> %d\n", j1939_tp_cm_transmit_manager(bus[0], -1));
  printf(">>> %d\n", j1939_receive(bus[1], -1));
  printf(">>> %d\n", j1939_tp_cm_transmit_manager(bus[1], -1));

  printf(">>> %d\n", j1939_receive(bus[0], -1));
  printf(">>> %d\n", j1939_tp_cm_transmit_manager(bus[0], -1));
  printf(">>> %d\n", j1939_receive(bus[1], -1));
  printf(">>> %d\n", j1939_tp_cm_transmit_manager(bus[1], -1));

  printf(">>> %d\n", j1939_receive(bus[0], -1));
  printf(">>> %d\n", j1939_tp_cm_transmit_manager(bus[0], -1));
  printf(">>> %d\n", j1939_receive(bus[1], -1));
  printf(">>> %d\n", j1939_tp_cm_transmit_manager(bus[1], -1));

  j1939_delete(bus[0]);
  j1939_delete(bus[1]);
}

/* valgrind --tool=memcheck --leak-check=full ./test/test */
int main(int argc, char *argv[]) {
  test1();
}
