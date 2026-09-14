#pragma once
// Engine-free bridge to the shared pc_p2_receipt.h provider.
//
// pc_p2_receipt.h includes <windows.h> on Windows for its atomic rename, which
// conflicts with the engine's `typedef u32 HWND` in AtxStream.h. Keeping the
// provider in its own translation unit (this one) lets family modules consume
// the ordinary-Onion receipt ledger without pulling windows.h into an engine TU.
bool pc_p2_receipt_host_open(const char* path);
bool pc_p2_receipt_host_ready();
bool pc_p2_receipt_host_valid(const char* value);
bool pc_p2_receipt_host_grant(const char* seed, const char* reward, const char* slotOrActor,
                              const char* encounter);
void pc_p2_receipt_host_close();
