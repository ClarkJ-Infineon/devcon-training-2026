/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * PSOC Control CAN Command & Telemetry lab - entry point.
 *
 * Provided, not a lab touchpoint. Dispatches to the role-specific
 * implementation selected by the Kconfig fragment passed at build time
 * (conf/role_command.conf or conf/role_telemetry.conf).
 */

#include <zephyr/kernel.h>

#if defined(CONFIG_LAB_ROLE_COMMAND)
void run_command_node(void);
#elif defined(CONFIG_LAB_ROLE_TELEMETRY)
void run_telemetry_node(void);
#else
#error "Select a lab role: build with -DEXTRA_CONF_FILE=conf/role_command.conf or conf/role_telemetry.conf"
#endif

int main(void)
{
#if defined(CONFIG_LAB_ROLE_COMMAND)
	run_command_node();
#elif defined(CONFIG_LAB_ROLE_TELEMETRY)
	run_telemetry_node();
#endif
	return 0;
}
