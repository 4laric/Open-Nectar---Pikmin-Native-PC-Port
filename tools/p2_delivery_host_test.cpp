#include "pc_p2_delivery_host.h"
#include <cassert>
#include <filesystem>
#include <fstream>

int main()
{
	using R = P2DeliveryHostResult;
	namespace fs = std::filesystem;
	const auto dir = fs::temp_directory_path() / "p2-delivery-host-test";
	std::filesystem::create_directories(dir);
	const auto path = (dir / "receipts.txt").string();
	fs::remove(path);

	// Unopened / bad arguments fail closed.
	assert(pc_p2_delivery_host_deliver("s", 45, 3, 1, 77, "corpse") == R::Error);
	assert(pc_p2_delivery_host_open(path.c_str()));
	assert(pc_p2_delivery_host_deliver(nullptr, 45, 3, 1, 77, "corpse") == R::Error);
	assert(pc_p2_delivery_host_deliver("s", 0, 3, 1, 77, "corpse") == R::Error);

	// First delivery grants; a repeat is a durable duplicate.
	assert(pc_p2_delivery_host_deliver("seed-a", 45, 3, 1, 77, "corpse") == R::Granted);
	assert(pc_p2_delivery_host_deliver("seed-a", 45, 3, 1, 77, "corpse") == R::Duplicate);
	// A different source on the same stage is a genuinely new grant.
	assert(pc_p2_delivery_host_deliver("seed-a", 44, 3, 1, 77, "corpse") == R::Granted);

	// Process restart over the same durable sidecar never re-grants.
	pc_p2_delivery_host_close();
	assert(pc_p2_delivery_host_open(path.c_str()));
	assert(pc_p2_delivery_host_deliver("seed-a", 45, 3, 1, 77, "corpse") == R::Duplicate);
	assert(pc_p2_delivery_host_deliver("seed-a", 45, 3, 1, 78, "corpse") == R::Granted);

	// A P1-proxy delivery (sourceId 0) is never accepted here (p1Proxy always false).
	assert(pc_p2_delivery_host_deliver("seed-a", 0, 3, 1, 77, "corpse") == R::Error);

	// Corrupt persisted state must be rejected, not silently replayed.
	pc_p2_delivery_host_close();
	{ std::ofstream out(path); out << "P2_RECEIPTS_1\ns truncated\n"; }
	assert(!pc_p2_delivery_host_open(path.c_str()));
	assert(!pc_p2_delivery_host_ready());

	std::filesystem::remove(dir / "receipts.txt");
	std::filesystem::remove(dir);
	assert(!pc_p2_delivery_host_ready());
	return 0;
}
