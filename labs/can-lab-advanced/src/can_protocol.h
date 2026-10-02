/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * PSOC Control CAN Command & Telemetry lab - shared CAN wire format.
 *
 * This is provided as-is; it is not one of the lab touchpoints. Both roles
 * include this header so they agree on the CAN identifier and payload
 * layout without needing to coordinate by hand.
 */

#ifndef PSOC_CONTROL_CAN_LAB_CAN_PROTOCOL_H_
#define PSOC_CONTROL_CAN_LAB_CAN_PROTOCOL_H_

/* Standard (11-bit) CAN identifier used for the setpoint frame. */
#define CAN_ID_SETPOINT 0x100

/* Payload is a single byte: 0-255 setpoint, linear mapping to PWM duty. */
#define CAN_DLC_SETPOINT 1

#endif /* PSOC_CONTROL_CAN_LAB_CAN_PROTOCOL_H_ */
