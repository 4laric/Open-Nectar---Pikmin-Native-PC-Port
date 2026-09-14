#pragma once

// Lane 06 ordinary Onion/AP delivery receiver. This is the provider surface that
// connects a real P2 family corpse delivery through the durable ordinary receipt
// ledger (pc_p2_receipt.h) without ever confusing it with the P1-proxy Teki type
// the family reuses, or with the experimental Research Pod economy.
//
// Engine-free, like pc_p2_receipt.h: no engine types, no retail asset, no
// save-file layout. Native save mutation stays coordinated with lane 01. The
// P1-proxy and P2-source vocabularies use disjoint prexes ("corpse:p1:" and
// "corpse:p2:") so a proxied P1 reward can never collide with a real P2 source,
// even when their numeric parts coincide. Exactly-once, persistence and restart
// behaviour are inherited from P2Receipt::ReceiptLedger.
//
// Python mirror: experimental/pikmin2_delivery.py.

#include "pc_p2_receipt.h"
#include <string>

namespace P2Delivery {

// Proxy identity for an ordinary P1 enemy encountered on `stage`.
inline std::string p1ProxyIdentity(int tekiType, int stage)
{
	return "corpse:p1:" + std::to_string(tekiType) + ":" + std::to_string(stage);
}

// Source identity for an imported P2 enemy `sourceId` on `stage`.
inline std::string p2SourceIdentity(unsigned sourceId, int stage)
{
	return "corpse:p2:" + std::to_string(sourceId) + ":" + std::to_string(stage);
}

// Exactly-once delivery receiver. sourceId == 0 selects the P1-proxy identity
// path; any non-zero sourceId selects the P2-source identity path. Grants defer
// to the wrapped ledger so exactly-once guarantees hold unchanged.
class DeliveryReceiver {
public:
	explicit DeliveryReceiver(P2Receipt::ReceiptLedger& ledger) : mLedger(ledger) {}

	std::string identity(unsigned sourceId, int tekiType, int stage) const
	{
		return sourceId != 0 ? p2SourceIdentity(sourceId, stage)
				     : p1ProxyIdentity(tekiType, stage);
	}

	std::string slotOrActor(unsigned generatorToken) const
	{
		return "g" + std::to_string(generatorToken);
	}

	bool deliver(const std::string& seed, unsigned sourceId, int tekiType, int stage,
		unsigned generatorToken, const std::string& encounter)
	{
		return mLedger.grant(seed, identity(sourceId, tekiType, stage),
			slotOrActor(generatorToken), encounter);
	}

	bool delivered(const std::string& seed, unsigned sourceId, int tekiType, int stage,
		unsigned generatorToken, const std::string& encounter) const
	{
		return mLedger.has(seed, identity(sourceId, tekiType, stage),
			slotOrActor(generatorToken), encounter);
	}

private:
	P2Receipt::ReceiptLedger& mLedger;
};

// Builds the ordinary receipt descriptor for a P2 source so reconcileOrdinary can
// assert that an expected ordinary check has a real ordinary source (never pod).
inline P2Receipt::Descriptor sourceDescriptor(unsigned sourceId, int stage,
	const std::string& family = "lane-13-bulborbs")
{
	P2Receipt::Descriptor descriptor;
	descriptor.identity = p2SourceIdentity(sourceId, stage);
	descriptor.family = family;
	descriptor.drop = "corpse";
	descriptor.ledger = P2Receipt::Ledger::Onion;
	return descriptor;
}

} // namespace P2Delivery
