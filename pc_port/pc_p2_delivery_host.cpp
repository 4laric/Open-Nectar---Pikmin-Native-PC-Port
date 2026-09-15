#include "pc_p2_delivery_host.h"
#include "pc_p2_delivery.h"
#include <memory>
#include <string>

namespace {
std::unique_ptr<P2Receipt::FileReceiptPersistence> persistence;
std::unique_ptr<P2Receipt::ReceiptLedger> ledger;
std::unique_ptr<P2Delivery::DeliveryReceiver> receiver;
} // namespace

bool pc_p2_delivery_host_open(const char* path)
{
	try {
		persistence = std::make_unique<P2Receipt::FileReceiptPersistence>(path ? path : "p2-delivery-receipts.txt");
		ledger = std::make_unique<P2Receipt::ReceiptLedger>(*persistence);
		receiver = std::make_unique<P2Delivery::DeliveryReceiver>(*ledger);
	} catch (...) {
		receiver.reset();
		ledger.reset();
		persistence.reset();
		return false;
	}
	return true;
}

bool pc_p2_delivery_host_ready() { return receiver != nullptr; }

P2DeliveryHostResult pc_p2_delivery_host_deliver(const char* seed, unsigned sourceId, int tekiType,
	int stage, unsigned generatorToken, const char* encounter)
{
	if (!receiver || !seed || !encounter || sourceId == 0) {
		return P2DeliveryHostResult::Error;
	}
	try {
		return receiver->deliver(seed, sourceId, tekiType, stage, generatorToken, encounter, /*p1Proxy=*/false)
		    ? P2DeliveryHostResult::Granted : P2DeliveryHostResult::Duplicate;
	} catch (...) {
		return P2DeliveryHostResult::Error;
	}
}

void pc_p2_delivery_host_close()
{
	receiver.reset();
	ledger.reset();
	persistence.reset();
}
