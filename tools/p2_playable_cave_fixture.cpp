// Headless persistence regression; this is not engine/gameplay acceptance.
#include "pc_p2_receipt_host.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

static void require(bool ok) { if (!ok) throw std::runtime_error("receipt assertion failed"); }
int main(int argc, char** argv)
{
    if (argc != 2) return 2;
    const std::filesystem::path directory(argv[1]);
    if (std::filesystem::exists(directory)) return 2;
    std::filesystem::create_directories(directory);
    const std::string path = (directory / "receipts.txt").string();
    auto handle = pc_p2_receipt_host_open(path.c_str());
    require(handle != nullptr);
    require(pc_p2_receipt_host_has(handle, "930", "treasure:forest_1:f1:water", "leaf0", "cave_treasure") == 0);
    require(pc_p2_receipt_host_grant(handle, "930", "treasure:forest_1:f1:water", "leaf0", "cave_treasure") == P2ReceiptHostResult::Granted);
    pc_p2_receipt_host_close(handle);
    require(pc_p2_receipt_host_has(handle, "930", "a", "b", "c") == -1);
    handle = pc_p2_receipt_host_open(path.c_str());
    require(handle != nullptr);
    require(pc_p2_receipt_host_has(handle, "930", "treasure:forest_1:f1:water", "leaf0", "cave_treasure") == 1);
    require(pc_p2_receipt_host_has(handle, "931", "treasure:forest_1:f1:water", "leaf0", "cave_treasure") == 0);
    require(pc_p2_receipt_host_count(handle) == 1);
    require(pc_p2_receipt_host_grant(handle, "930", "treasure:forest_1:f1:water", "leaf0", "cave_treasure") == P2ReceiptHostResult::Duplicate);
    pc_p2_receipt_host_close(handle);
    std::ofstream(path, std::ios::trunc) << "corrupt ledger\n";
    require(pc_p2_receipt_host_open(path.c_str()) == nullptr);
    require(pc_p2_receipt_host_open(directory.string().c_str()) == nullptr);
    std::cout << "P2_CAVE_RECEIPT_HEADLESS PASS restart duplicate foreign-seed corrupt unreadable\n";
}
