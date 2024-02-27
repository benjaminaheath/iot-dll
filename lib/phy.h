#ifndef PHY_H
#define PHY_H

#include <stdlib.h>
#include <stdint.h>

#include "comms_config.h"
#include "structs.h"
#include "util.h"
#include "dll.h"

uint8_t* chan;
size_t chan_size = 0;

uint8_t noise = (unsigned) (PHY_NOISE_FREQ * RAND_MAX);

// phy services
recv_callback_phy phy_phy();
void send_phy(uint8_t byte);
uint8_t recv_phy();

// phy internal methods
static void __send_phy_byte(uint8_t byte);
static uint8_t __channel(uint8_t byte);
static uint8_t __noise(uint8_t byte);

#endif