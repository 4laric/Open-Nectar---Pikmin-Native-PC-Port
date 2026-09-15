#pragma once
// Engine-free bridge to the shared pc_p2_delivery.h provider, mirroring
// pc_p2_receipt_host.h. Keeps <windows.h> (pulled in by pc_p2_receipt.h for its
// atomic rename) out of engine translation units: pc_randomizer.cpp and the
// engine call the plain C functions here, while this TU owns the persistent
// ledger + DeliveryReceiver instance.
bool pc_p2_delivery_host_open(const char* path);
bool pc_p2_delivery_host_ready();
// Error is distinct from a durable duplicate; callers must not consume rewards on Error.
enum class P2DeliveryHostResult { Error = -1, Duplicate = 0, Granted = 1 };
// Ordinary P2 delivery: always p1Proxy=false and a non-zero bound source id.
P2DeliveryHostResult pc_p2_delivery_host_deliver(const char* seed, unsigned sourceId, int tekiType,
	int stage, unsigned generatorToken, const char* encounter);
void pc_p2_delivery_host_close();
