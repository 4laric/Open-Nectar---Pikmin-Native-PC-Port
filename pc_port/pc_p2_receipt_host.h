#pragma once
// Engine-free bridge to the shared pc_p2_receipt.h provider. This header keeps
// <windows.h> (pulled in by pc_p2_receipt.h for its atomic rename) out of engine
// translation units. Multiple durable ledgers at distinct paths coexist: open()
// returns an opaque handle per path, and grant()/close() take that handle, so a
// second consumer can never redirect or disable the first consumer's ledger.
// A consumer that only needs an isolated atomic write may keep using
// pc_p2_receipt_host_atomic_write(path, data) (stateless).

// Opaque handle; nullptr is invalid / not opened.
typedef const void* P2ReceiptHostHandle;

// Open (or reuse) the durable ordinary receipt ledger at `path`. Returns a
// handle, or nullptr when the persisted state cannot be opened (corrupt or
// wrong-version); a missing file opens empty. Re-opening the same path returns
// the same ledger (same durable state), never a fresh one.
P2ReceiptHostHandle pc_p2_receipt_host_open(const char* path);

// The path backing a handle (diagnosis), or nullptr for an invalid handle.
const char* pc_p2_receipt_host_path(P2ReceiptHostHandle handle);

// Stable token validator (mirrors P2Receipt::validToken).
bool pc_p2_receipt_host_valid(const char* value);

// Error is distinct from a durable duplicate; callers must not consume rewards on Error.
enum class P2ReceiptHostResult { Error = -1, Duplicate = 0, Granted = 1 };

// Grant exactly once over the handle's ledger. Returns Granted only on the first
// occurrence of (seed, reward, slotOrActor, encounter).
P2ReceiptHostResult pc_p2_receipt_host_grant(P2ReceiptHostHandle handle, const char* seed,
	const char* reward, const char* slotOrActor, const char* encounter);

// Release one handle; other handles are unaffected.
void pc_p2_receipt_host_close(P2ReceiptHostHandle handle);

// Stateless atomic write (unrelated to any open ledger).
bool pc_p2_receipt_host_atomic_write(const char* path, const char* data);
