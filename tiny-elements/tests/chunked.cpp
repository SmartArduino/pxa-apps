// SPDX-License-Identifier: CC0-1.0
// Exercise the exact production transport, without SDK imports or a fake game.
#define POCKET_CHUNK_STORAGE_TEST
#include "../../common/pocket-play/chunked-runtime.cpp"
#include "../core.hpp"
#include <assert.h>
#include <stdio.h>
#include <map>
#include <string>
#include <vector>
using Bytes = std::vector<uint8_t>;
using Disk = std::map<std::string, Bytes>;
namespace pc = pocket_chunks;
static unsigned writes;
static const char *logical = "pxa-tiny-elements.save.v1";
static std::string key(unsigned bank, unsigned chunk) {
    return std::string(logical) + ".b" + char('0' + bank) + ".c" + char('0' + chunk);
}
static Bytes pattern(size_t size, uint32_t seed) {
    Bytes bytes(size);
    for (auto &b : bytes) { seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5; b = uint8_t(seed); }
    return bytes;
}
static void store(Disk &disk, const pc::Writer &writer) {
    assert(writer.active && writer.size() && writer.size() <= 960);
    const std::string name = writer.at_head() ? logical : key(writer.next.bank, writer.stage);
    assert(name.size() <= 64);
    disk[name] = Bytes(writer.data(), writer.data() + writer.size()); ++writes;
}
static bool assemble(const Disk &disk, Bytes &out) {
    auto head = disk.find(logical);
    pc::Reader reader;
    if (head == disk.end() || !reader.begin(head->second.data(), head->second.size())) return false;
    for (unsigned i = 0; i < pc::chunks(reader.committed); ++i) {
        auto chunk = disk.find(key(reader.committed.bank, i));
        if (chunk == disk.end()) return false;
        const int result = reader.append(chunk->second.data(), chunk->second.size());
        if (result < 0) return false;
        if (result == 1) { out.assign(reader.payload, reader.payload + reader.committed.total); return true; }
    }
    return false;
}
static pc::Commit commit(const Disk &disk) {
    pc::Commit info;
    const auto &head = disk.at(logical); assert(pc::decode(head.data(), head.size(), info)); return info;
}
static void finish(Disk &disk, pc::Writer &writer) {
    while (writer.active) { store(disk, writer); writer.acknowledge(); }
}
static void expect(const Disk &disk, const Bytes &bytes) {
    Bytes actual; assert(assemble(disk, actual) && actual == bytes);
}
static Disk initial(const Bytes &bytes) {
    Disk disk; pc::Writer writer; assert(writer.begin(bytes.data(), bytes.size())); finish(disk, writer); return disk;
}
int main(int argc, char **argv) {
    assert(argc == 2);
    const Bytes old = pattern(1780, 1), next = pattern(1780, 2), later = pattern(1780, 3);
    // First-ever commit: chunks alone never make a save visible.
    for (unsigned cut = 0; cut <= 3; ++cut) {
        Disk disk; pc::Writer writer; Bytes result;
        assert(writer.begin(old.data(), old.size()));
        for (unsigned i = 0; i < cut; ++i) { store(disk, writer); if (i + 1 < cut) writer.acknowledge(); }
        if (cut < 3) assert(!assemble(disk, result)); else expect(disk, old);
    }
    const Disk baseline = initial(old);
    // Power fails before/after every successful set, including before head ACK.
    for (unsigned cut = 0; cut <= 3; ++cut) {
        Disk disk = baseline; pc::Writer writer; writer.restore(commit(disk));
        assert(writer.begin(next.data(), next.size()));
        assert(writer.next.bank != writer.committed.bank);
        for (unsigned i = 0; i < cut; ++i) { store(disk, writer); if (i + 1 < cut) writer.acknowledge(); }
        expect(disk, cut == 3 ? next : old);
        // A reboot uses only the committed head, ignores orphaned chunks and
        // writes the opposite bank completely before replacing that head.
        pc::Writer reboot; reboot.restore(commit(disk));
        assert(reboot.begin(later.data(), later.size()));
        const Bytes visible = cut == 3 ? next : old;
        while (reboot.active) {
            const bool head = reboot.at_head(); store(disk, reboot);
            expect(disk, head ? later : visible); reboot.acknowledge();
        }
        expect(disk, later);
    }
    // Failure can mean no durable write or an applied write with a lost ACK.
    // Retrying either case must restart chunk zero with the immutable snapshot.
    for (unsigned failed_stage = 0; failed_stage < 3; ++failed_stage) for (unsigned applied = 0; applied < 2; ++applied) {
        Disk disk = baseline; pc::Writer writer; writer.restore(commit(disk));
        Bytes input = next; assert(writer.begin(input.data(), input.size()));
        input = later; assert(!writer.begin(input.data(), input.size()));
        const unsigned bank = writer.next.bank;
        for (unsigned stage = 0; stage < failed_stage; ++stage) { store(disk, writer); writer.acknowledge(); }
        if (applied) store(disk, writer);
        writer.retry(); assert(writer.stage == 0 && writer.next.bank == bank && writer.committed.generation == 1);
        expect(disk, failed_stage == 2 && applied ? next : old);
        finish(disk, writer); expect(disk, next);
        assert(writer.committed.generation == 2);
        assert(writer.begin(later.data(), later.size())); assert(writer.next.bank != bank);
        finish(disk, writer); expect(disk, later);
    }
    // Exact sizes at both chunk boundaries and the supported worst-case bound.
    for (size_t size : {size_t(1), size_t(896), size_t(897), size_t(1780), pc::CAPACITY}) {
        const Bytes bytes = pattern(size, unsigned(size)); Disk disk = initial(bytes); expect(disk, bytes);
        assert(disk.at(logical).size() == 24);
        for (auto &row : disk) assert(row.second.size() <= 960);
    }
    pc::Writer rejected; const Bytes too_large = pattern(pc::CAPACITY + 1, 11);
    assert(!rejected.begin(too_large.data(), too_large.size()) && !rejected.begin(old.data(), 0));
    auto limit = commit(baseline); limit.generation = UINT64_MAX; rejected.restore(limit);
    assert(!rejected.begin(old.data(), old.size()));
    // No partially assembled, missing, truncated, mixed or corrupt data escapes.
    const auto info = commit(baseline);
    for (unsigned which = 0; which < 2; ++which) {
        for (unsigned damage = 0; damage < 4; ++damage) {
            Disk disk = baseline; auto &part = disk.at(key(info.bank, which)); Bytes out = later;
            if (damage == 0) disk.erase(key(info.bank, which));
            if (damage == 1) part.pop_back();
            if (damage == 2) part[part.size()/2] ^= 1;
            if (damage == 3) part = Bytes(next.begin() + which * pc::CHUNK, next.begin() + which * pc::CHUNK + pc::chunk_size(info, which));
            assert(!assemble(disk, out) && out == later);
        }
    }
    for (unsigned field : {0u, 4u, 5u, 6u, 7u, 12u, 20u, 21u, 22u, 23u}) {
        Disk disk = baseline; auto &head = disk.at(logical); Bytes out = later;
        if (field == 5) head[field] = 2;
        else if (field == 6 || field == 7) { head[6] = 1; head[7] = 7; } // 1793
        else if (field == 12) for (unsigned i = 12; i < 20; ++i) head[i] = 0;
        else head[field] ^= 2;
        assert(!assemble(disk, out) && out == later);
    }
    { Disk disk = baseline; disk[logical].pop_back(); Bytes out; assert(!assemble(disk, out)); }
    // Outer integrity cannot turn an invalid game payload into accepted state.
    elements::State state{}, restored{}; elements::init(state); elements::init(restored);
    Bytes game(elements::SAVE_SIZE); assert(elements::encode(state, game.data(), game.size()) == game.size());
    game[50] ^= 1; Disk invalid = initial(game); Bytes decoded; assert(assemble(invalid, decoded));
    assert(!elements::decode(restored, decoded.data(), decoded.size()));
    Bytes unchanged(elements::SAVE_SIZE); elements::encode(restored, unchanged.data(), unchanged.size());
    Bytes original(elements::SAVE_SIZE); elements::encode(state, original.data(), original.size()); assert(unchanged == original);
    // Repeated generations with high-entropy worlds and faults at every phase.
    Disk disk = baseline; Bytes visible = old; uint32_t random = 12345;
    for (unsigned round = 0; round < 200; ++round) {
        const Bytes target = pattern(1780, round + 77); pc::Writer writer; writer.restore(commit(disk));
        assert(writer.begin(target.data(), target.size())); unsigned faults = 0;
        while (writer.active) {
            random ^= random << 13; random ^= random >> 17; random ^= random << 5;
            const bool fail = faults < 6 && (random & 3) == 0;
            if (!fail || (random & 4)) store(disk, writer);
            Bytes actual; assert(assemble(disk, actual) && (actual == visible || actual == target));
            if (fail) { writer.retry(); ++faults; } else writer.acknowledge();
        }
        expect(disk, target); visible = target;
    }
    char path[4096]; snprintf(path, sizeof(path), "%s/chunked-results.json", argv[1]); FILE *file = fopen(path, "w"); assert(file);
    fprintf(file, "{\"passed\":true,\"max_chunk_bytes\":896,\"head_bytes\":24,\"payload_bytes\":1780,\"durable_writes\":%u,\"faulted_generations\":200,\"checks\":[\"first_commit_orphans\",\"power_loss_every_phase\",\"inactive_bank_preserved\",\"head_last\",\"failed_and_ambiguous_ack_retry_from_chunk_zero\",\"immutable_inflight_snapshot\",\"reboot_then_next_bank\",\"chunk_boundaries\",\"generation_overflow_rejection\",\"missing_truncated_corrupt_mixed_chunks\",\"strict_head_validation\",\"inner_game_checksum_atomic_rejection\",\"randomized_fault_sequences\"]}\n", writes);
    fclose(file); puts("PASS chunked production storage: torn commits, retries, generations and integrity");
}
