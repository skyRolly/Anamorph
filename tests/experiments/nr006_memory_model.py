#!/usr/bin/env python3
"""Bounded read-from/coherence model for one slot's first reuse (not a C++ verifier).

Enumerates three tag observations and three payload observations. The fixed
producer order includes target publication then reuse; the reader acquires that
publication. See ADR-0070's proof for the unbounded argument and model limits.
"""
from itertools import product


def observations(payload_acquire=True, invalidate=True):
    writer = ["value0", "value1", "tag1", "publish"]
    if invalidate:
        writer += ["invalid"]
    writer += ["value2", "tag2"]
    reader = ["observe", "before", "payload", "after"]
    tag_order = ["tag1"] + (["invalid"] if invalidate else []) + ["tag2"]
    value_order = ["value0", "value1", "value2"]
    orders = {"tag": tag_order, "value": value_order}
    nodes = writer + reader
    allowed = []
    for before, value, after in product(tag_order, value_order, tag_order):
        rf = {"before": before, "payload": value, "after": after}
        hb = {(a, b) for seq in [writer, reader] for a, b in zip(seq, seq[1:])}
        hb.add(("publish", "observe"))
        hb.update((rf[r], r) for r in ["before", "after"])
        if payload_acquire:
            hb.add((value, "payload"))
        while True:
            more = {(a, d) for a, b in hb for c, d in hb if b == c}
            updated = hb | more
            if updated == hb:
                break
            hb = updated
        if any((n, n) in hb for n in nodes):
            continue
        legal = True
        # The per-object write/read, read/write and read/read coherence rules.
        for atom, writes in orders.items():
            reads = [r for r in rf if (r == "payload") == (atom == "value")]
            for r in reads:
                index = writes.index(rf[r])
                for wi, w in enumerate(writes):
                    if (w, r) in hb and index < wi:
                        legal = False
                    if (r, w) in hb and index >= wi:
                        legal = False
                for other in reads:
                    if (r, other) in hb and index > writes.index(rf[other]):
                        legal = False
        if legal:
            allowed.append((before, value, after))
    accepted = [x for x in allowed if x[0] == "tag1" and x[2] == "tag1"]
    wrong = [x for x in accepted if x[1] != "value1"]
    return allowed, accepted, wrong


def main():
    variants = [
        ("proposed", True, True, False),
        ("negative: relaxed payload load", False, True, True),
        ("negative: missing invalidation", True, False, True),
    ]
    for name, acquire, invalidate, expect_bad in variants:
        legal, accepted, wrong = observations(acquire, invalidate)
        assert legal and accepted, (name, "vacuous model")
        assert bool(wrong) == expect_bad, (name, wrong)
        print(f"{name}: {len(legal)} legal observations, {len(accepted)} accepted, "
              f"{len(wrong)} wrong accepted")
    print("Model and both weakening controls passed; production code unchanged.")


if __name__ == "__main__":
    main()
