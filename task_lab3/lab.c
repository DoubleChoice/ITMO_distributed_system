#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>

#include "message.h"
#include "log.h"
#include "process.h"
#include "banking.h"

timestamp_t lamport_clock = 0;

void parent_work(int count_nodes)
{
    AllHistory all_history;
    all_history.s_history_len = count_nodes - 1;

    /* STUDENT IMPLEMENTATION STARTED */
    /* Implement starting synchronization */
    Message msg;
    // Parent workflow
    // Phase 1: Receive STARTED messages from all child processes
    for (int i = 1; i < count_nodes; i++) {
        receive(i, &msg);
        lamport_clock = msg.s_header.s_local_time > lamport_clock ? msg.s_header.s_local_time : lamport_clock;
        lamport_clock++;
    }

    // Phase 2: Call function bank_operations() and use transfer()
    /* Useful work */
    bank_operations(count_nodes - 1);

    // Phase 3: Sends STOP message to all child processes
    fill_message(&msg, STOP, ++lamport_clock, NULL, 0);
    send_multicast(&msg);

    // Phase 4: Receive DONE messages from all child processes
    for (int i = 1; i < count_nodes; i++) {
        receive(i, &msg);
        lamport_clock = msg.s_header.s_local_time > lamport_clock ? msg.s_header.s_local_time : lamport_clock;
        lamport_clock++;
    }


    // Phase 5:  receive all BALANCE_HISTORIES messages and aggregate AllHistory and call print_history()
    /* Implement finishing synchronization and collecting AllHistory */
    for (int i = 1; i < count_nodes; i++) {
        receive(i, &msg);
        lamport_clock = msg.s_header.s_local_time > lamport_clock ? msg.s_header.s_local_time : lamport_clock;
        lamport_clock++;
        BalanceHistory* history = (BalanceHistory*)msg.s_payload;
        memcpy(&all_history.s_history[i - 1], history, msg.s_header.s_payload_len);
    }
    print_history(&all_history);
}

void child_work(struct child_arguments args)
{
    /* Child arguments */
    local_id self_id = args.self_id;
    int count_nodes = args.count_nodes;
    uint8_t balance = args.balance;

    /* BalanceHistory initialization */
    BalanceHistory history;
    history.s_history_len = 1;
    history.s_id = self_id;
    memset(history.s_history, 0, sizeof(history.s_history));
    history.s_history[0].s_balance = balance;
    for (int i = 0; i < MAX_T; ++i) {
        history.s_history[i].s_time = i;
    }

    /* System process identifiers used for logs */
    pid_t self_pid = getpid();
    pid_t parent_pid = getppid();

    /* STUDENT IMPLEMENTATION STARTED */

    // Buffer for logging
    char buf[BUF_SIZE];
    Message msg;

    // Child workflow
    // Phase 1: 
    // Phase 1.1: Notify all processes that the child has started
    snprintf(buf, BUF_SIZE, log_started_fmt, 0, self_id, self_pid, parent_pid, 0);
    fill_message(&msg, STARTED, ++lamport_clock, buf, strlen(buf));
    send_multicast(&msg);
    shared_logger(buf);
    //Phase 1.2: Receive STARTED messages from all other child processes
    for (int i = 1; i < count_nodes; i++) {
        if (i != self_id) {					//A process should not receive messages from itself, so i need to check.
            receive(i, &msg);				// Block until STARTED message is received
            lamport_clock = msg.s_header.s_local_time > lamport_clock ? msg.s_header.s_local_time : lamport_clock;
            lamport_clock++;
        }
    }
    // Log received all STARTED messages
    snprintf(buf, BUF_SIZE, log_received_all_started_fmt, 0, self_id);
    shared_logger(buf);

    // Phase 2: Useful work of child process is waiting and handling TRANSFER and STOP messages
    while (1) {
        Message msg;
        receive_any(&msg);
        lamport_clock = msg.s_header.s_local_time > lamport_clock ? msg.s_header.s_local_time : lamport_clock;
        lamport_clock++;
        if (msg.s_header.s_type == STOP) { // Analyse Message type
            break;
        }
        if (msg.s_header.s_type == TRANSFER) {
            TransferOrder* order = (TransferOrder*)msg.s_payload;   // Get payload from TRANSFER message
            if (order->s_src == self_id) {                          // If it's source client, reduce its balance
                for (int i = history.s_history_len; i < lamport_clock; i++) {             // Fill the BalanceHistory which is empty at timestamps without transferring 
                    history.s_history[i].s_balance = balance;
                    history.s_history[i].s_time = i;
                    history.s_history[i].s_balance_pending_in = 0;
                }
                history.s_history_len = lamport_clock;                                // Fill the BalanceHistory at the timestamps with transferring
                balance -= order->s_amount;
                history.s_history[history.s_history_len].s_balance = balance;
                history.s_history[history.s_history_len].s_time = lamport_clock;
                history.s_history[history.s_history_len].s_balance_pending_in = 0;
                history.s_history_len++;
                TransferOrder s_order = { self_id, order->s_dst, order->s_amount };
                Message transfer_msg;
                fill_message(&transfer_msg, TRANSFER, ++lamport_clock, &s_order, sizeof(s_order));          // Send TRANSFER message to desnation
                send(order->s_dst, &transfer_msg);

            }
            else if (order->s_dst == self_id) {                         // If it's desnation client, increase its balance
                for (int i = history.s_history_len; i < lamport_clock; i++) {               // Fill the BalanceHistory which is empty at timestamps without transferring
                    history.s_history[i].s_balance = balance;
                    history.s_history[i].s_time = i;
                    //history.s_history[i].s_balance_pending_in = order->s_amount;
                }
                history.s_history_len = lamport_clock;                                // Fill the BalanceHistory at the timestamps with transferring
                balance += order->s_amount;
                history.s_history[history.s_history_len].s_balance = balance;
                history.s_history[history.s_history_len].s_time = lamport_clock;
                history.s_history[history.s_history_len].s_balance_pending_in = 0;
                history.s_history_len++;

                for (int i = msg.s_header.s_local_time - 1; i < lamport_clock; i++) {
                    history.s_history[i].s_balance_pending_in = order->s_amount;
                }
                Message ack_msg;
                fill_message(&ack_msg, ACK, ++lamport_clock, NULL, 0);                  // Send ACK to parent
                send(0, &ack_msg);
            }
        }
    }

    // phase 3:
    // phase 3.1: sends a message of type DONE to all other processes and prints log
    snprintf(buf, BUF_SIZE, log_done_fmt, 0, self_id, 0);
    fill_message(&msg, DONE, ++lamport_clock, buf, strlen(buf));
    send_multicast(&msg);
    shared_logger(buf);  // Log the DONE message

    // phase 3.2: Process waits for DONE messages from all other child processes and prints log
    for (int i = 1; i < count_nodes; i++) {
        if (i != self_id) {					//A process should not receive messages from itself, so i need to check.
            receive(i, &msg);  // Block until DONE message is received
            lamport_clock = msg.s_header.s_local_time > lamport_clock ? msg.s_header.s_local_time : lamport_clock;
            lamport_clock++;
        }
    }

    // Log received all DONE messages
    snprintf(buf, BUF_SIZE, log_received_all_done_fmt, 0, self_id);
    shared_logger(buf);

    // Send BALANCE_HISTORY message to parent
    Message balance_msg;
    fill_message(&balance_msg, BALANCE_HISTORY, ++lamport_clock, &history, sizeof(history));
    send(0, &balance_msg);
}

void transfer(local_id src, local_id dst,
    balance_t amount)
{
    TransferOrder order = { src, dst, amount };

    /* STUDENT IMPLEMENTATION STARTED */
    Message transfer_msg;
    fill_message(&transfer_msg, TRANSFER, ++lamport_clock, &order, sizeof(order));
    send(src, &transfer_msg);               // Parent send the TRANSFER message to source client
    Message ack_msg;
    receive(dst, &ack_msg);                 // Parent Wait for ACK from desination client
    lamport_clock = transfer_msg.s_header.s_local_time > lamport_clock ? transfer_msg.s_header.s_local_time : lamport_clock;
    lamport_clock++;
}

/* STUDENTS SHOULD NOT CHANGE THIS FUNCTION */
__attribute__((weak)) void bank_operations(local_id max_id)
{
    for (int i = 1; i < max_id; ++i) {
        transfer(i, i + 1, i);
    }
    if (max_id > 1) {
        transfer(max_id, 1, 1);
    }
}
