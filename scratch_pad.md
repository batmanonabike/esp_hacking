# Scratch pad

## For AI in agent mode after a conversation about FSM for BLE Server

```
Create new utility code in `bitmans_bles.c` and `bitmans_bles.h` (for ble server) in `bitmans_lib` that implements all of the above finite state machine logic.
All methods should be prefixed with `bitmans_bles_` and not use existing methods in `bitmans_lib`. Lets keep this seperate.
Pay particular attention to ensuring that:
1. global use is at a minimum.
2. race conditions potential is reduced by the fsm,
3. the user/caller has control of registration, such as adding additional characteristics or add a cccd.
4. user code is very simple for developing a full gatts server include registration, advertising, reading/writing, disconnecting, stopping services, starting services while still providing control. 
 
Allow the user to deefine multiple services. 
Allow the FSM to handle the low-level state transitions internally, but still provide high-level callbacks to the user (like on_service_ready, on_connected, on_read_request).
On error transition to an error state and let the user decide how to handle it.  Provide functions that permit this.
Use RTOS for tasks.

The user should also be able to stop/start the service and stop/start advertising.
Note the existince of bitmans_hash_table.c/.h for helping with state.

Feel free to stop and ask questions.

```