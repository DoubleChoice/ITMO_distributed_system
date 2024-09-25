#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>

#include "message.h"
#include "log.h"
#include "process.h"

void parent_work(int count_nodes)
{
	Message msg;
	// Parent workflow
	// Phase 1: Receive STARTED messages from all child processes
	for (int i = 1; i < count_nodes; i++) {
		receive(i, &msg);
	}

	// Phase 3: Receive DONE messages from all child processes
	for (int i = 1; i < count_nodes; i++) {
		receive(i, &msg);
	}
}

void child_work(struct child_arguments args)
{
	// Child arguments
	local_id self_id = args.self_id;
	int count_nodes = args.count_nodes;

	// System process identifiers used for logs
	pid_t self_pid = getpid();
	pid_t parent_pid = getppid();

	// Buffer for logging
	char buf[BUF_SIZE];
	Message msg;

	// Child workflow
	// Phase 1: 
	// Phase 1.1: Notify all processes that the child has started
	snprintf(buf, BUF_SIZE, log_started_fmt, 0, self_id, self_pid, parent_pid, 0);
	fill_message(&msg, STARTED, 0, buf, strlen(buf));
	send_multicast(&msg);
	shared_logger(buf);
	//Phase 1.2: Receive STARTED messages from all other child processes
	for (int i = 1; i < count_nodes; i++) {
		if (i != self_id) {					//A process should not receive messages from itself, so i need to check.
			receive(i, &msg);				// Block until STARTED message is received
		}
	}
	// Log received all STARTED messages
	snprintf(buf, BUF_SIZE, log_received_all_started_fmt, 0, self_id);
	shared_logger(buf);

	// Phase 2: skip

	// phase 3:
	// phase 3.1: sends a message of type DONE to all other processes and prints log
	snprintf(buf, BUF_SIZE, log_done_fmt, 0, self_id, 0);
	fill_message(&msg, DONE, 0, buf, strlen(buf));
	send_multicast(&msg);
	shared_logger(buf);  // Log the DONE message

	// phase 3.2: Process waits for DONE messages from all other child processes and prints log
	for (int i = 1; i < count_nodes; i++) {
		if (i != self_id) {					//A process should not receive messages from itself, so i need to check.
			receive(i, &msg);  // Block until DONE message is received
		}
	}

	// Log received all DONE messages
	snprintf(buf, BUF_SIZE, log_received_all_done_fmt, 0, self_id);
	shared_logger(buf);

}
