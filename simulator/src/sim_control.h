#pragma once

// Start the HTTP control task for simulator UI automation.
// `host` should typically be "127.0.0.1". `port` must be 1-65535.
// Returns true if the task was created successfully.
bool sim_control_start_http_task(const char *host, int port);
