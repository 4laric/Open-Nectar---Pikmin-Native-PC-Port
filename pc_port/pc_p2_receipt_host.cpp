#include "pc_p2_receipt_host.h"
#include "pc_p2_receipt.h"
#include <memory>
#include <string>

namespace {
std::unique_ptr<P2Receipt::FileReceiptPersistence> persistence;
std::unique_ptr<P2Receipt::ReceiptLedger> ledger;
} // namespace

bool pc_p2_receipt_host_open(const char* path)
{
	try {
		persistence = std::make_unique<P2Receipt::FileReceiptPersistence>(path ? path : "p2-receipts.txt");
		ledger = std::make_unique<P2Receipt::ReceiptLedger>(*persistence);
	} catch (...) {
		ledger.reset();
		persistence.reset();
		return false;
	}
	return true;
}

bool pc_p2_receipt_host_ready() { return ledger != nullptr; }

bool pc_p2_receipt_host_valid(const char* value)
{
	return value && P2Receipt::validToken(value, 128);
}

bool pc_p2_receipt_host_grant(const char* seed, const char* reward, const char* slotOrActor, const char* encounter)
{
	if (!ledger) {
		return false;
	}
	try {
		return ledger->grant(seed, reward, slotOrActor, encounter);
	} catch (...) {
		return false;
	}
}

void pc_p2_receipt_host_close()
{
	ledger.reset();
	persistence.reset();
}
