#include "j1939_virtual.h"
#include <queue>
#include <unordered_map>
#include <stdio.h>

struct hash_t {
    size_t operator()(const j1939_port_t *self) const {
        return (size_t)self;
    }
};

struct pred_t {
    bool operator()(const j1939_port_t *l, const j1939_port_t *r) const {
        return l == r;
    }
};

using node_t = std::queue<j1939_spdu_t>;
using bus_t = std::unordered_map<j1939_port_t *, node_t, hash_t, pred_t>;

static bus_t _bus{};

extern "C" uint32_t j1939_virtual_get_tick(j1939_port_t *self) {
  static uint32_t count = 0;
  count += 50;
  return count;
}

extern "C" j1939_status_t j1939_virtual_transmit(j1939_port_t *self, const j1939_spdu_t *msg, uint32_t timeout_ms) {
  for (auto &[port, node] : _bus) {
    if (port == self) {
      continue;
    }
    node.push(*msg);
  }
  printf("port [%02lX] tx id [%08X] size [%d] data [", (size_t)self, msg->id.u32, msg->size);
  for (uint16_t idx = 0; idx < msg->size; ++idx) {
    printf("%02X%s", msg->data[idx], idx == msg->size - 1 ? "]\n" : " ");
  }
  return J1939_OK;
}

extern "C" j1939_status_t j1939_virtual_receive(j1939_port_t *self, j1939_spdu_t *msg, uint32_t timeout_ms) {
  for (auto &[port, node] : _bus) {
    if (port != self) {
      continue;
    }
    if (node.size() == 0) {
      return J1939_TIMEOUT;
    }
    *msg = node.front();
    node.pop();
  }
  printf("port [%02lX] rx id [%08X] size [%d] data [", (size_t)self, msg->id.u32, msg->size);
  for (uint16_t idx = 0; idx < msg->size; ++idx) {
    printf("%02X%s", msg->data[idx], idx == msg->size - 1 ? "]\n" : " ");
  }
  return J1939_OK;
}

extern "C" void j1939_virtual_add_node(j1939_port_t *self) {
  _bus[self] = {};
}
