#!/usr/bin/env python3
"""Post-link step for the web side module (docs/DATA-FORMAT.md section 11).

The core throws C++ exceptions, and the bridge turns them into JSON errors. Godot 4.5's official web templates build the
main module with exceptions off: its __cxa_throw aborts, and it defines no `__cpp_exception` tag. The side module therefore
links its own libc++abi and libunwind (wasm-exceptions variant, -Bsymbolic). The one thing wasm-ld still imports is the
`__cpp_exception` tag, and the main module cannot supply it ("tag import requires a WebAssembly.Tag").

This script turns that import into a tag the module defines itself. Every throw and catch of a C++ exception happens inside
this module, so a module-local tag is correct. Nothing else changes: the other sections are copied byte for byte.

    python wasm_localize_tag.py <module.wasm>     # rewrites in place; no-op if there is no such import
"""
import sys

TAG_NAME = b"__cpp_exception"


def uleb(data, pos):
    result = shift = 0
    while True:
        b = data[pos]; pos += 1
        result |= (b & 0x7F) << shift
        if not b & 0x80:
            return result, pos
        shift += 7


def enc(n):
    out = bytearray()
    while True:
        b = n & 0x7F; n >>= 7
        if n:
            out.append(b | 0x80)
        else:
            out.append(b)
            return bytes(out)


def main(path):
    data = open(path, "rb").read()
    if data[:8] != b"\0asm\x01\0\0\0":
        sys.exit("not a wasm module: " + path)
    sections = []  # (id, payload)
    pos = 8
    while pos < len(data):
        sid = data[pos]; size, p = uleb(data, pos + 1)
        sections.append((sid, data[p:p + size]))
        pos = p + size
    if any(sid == 13 for sid, _ in sections):
        sys.exit("module already defines tags; refusing to guess the index order")
    out_sections = []
    tag_type = None
    for sid, payload in sections:
        if sid != 2:
            out_sections.append((sid, payload)); continue
        count, p = uleb(payload, 0)
        kept = []
        imported_tags = 0
        for _ in range(count):
            start = p
            mlen, p = uleb(payload, p); p += mlen
            flen, q = uleb(payload, p); field = payload[q:q + flen]; p = q + flen
            kind = payload[p]; p += 1
            if kind == 0:   # func: typeidx
                _, p = uleb(payload, p)
            elif kind == 1:  # table: reftype + limits
                p += 1; flags = payload[p]; p += 1; _, p = uleb(payload, p)
                if flags & 1: _, p = uleb(payload, p)
            elif kind == 2:  # memory: limits
                flags = payload[p]; p += 1; _, p = uleb(payload, p)
                if flags & 1: _, p = uleb(payload, p)
            elif kind == 3:  # global: valtype + mut
                p += 2
            elif kind == 4:  # tag: attribute + typeidx
                attr = payload[p]; p += 1
                typeidx, p = uleb(payload, p)
                imported_tags += 1
                if field == TAG_NAME:
                    tag_type = (attr, typeidx); continue
            else:
                sys.exit("unknown import kind %d" % kind)
            kept.append(payload[start:p])
        if tag_type is None:
            print("no %s import: nothing to do" % TAG_NAME.decode()); return
        if imported_tags != 1:
            sys.exit("expected exactly one imported tag, found %d" % imported_tags)
        out_sections.append((2, enc(len(kept)) + b"".join(kept)))
    # The tag section (id 13) goes after memory (5) and before global (6): find the first section that must follow it.
    tag_payload = enc(1) + bytes([tag_type[0]]) + enc(tag_type[1])
    order = {1: 1, 2: 2, 3: 3, 4: 4, 5: 5, 13: 6, 6: 7, 7: 8, 8: 9, 9: 10, 12: 11, 10: 12, 11: 13}
    result = []
    inserted = False
    for sid, payload in out_sections:
        if not inserted and sid != 0 and order.get(sid, 0) > order[13]:
            result.append((13, tag_payload)); inserted = True
        result.append((sid, payload))
    if not inserted:
        result.append((13, tag_payload))
    blob = bytearray(data[:8])
    for sid, payload in result:
        blob.append(sid); blob += enc(len(payload)); blob += payload
    open(path, "wb").write(bytes(blob))
    print("localized %s tag (type %d) in %s" % (TAG_NAME.decode(), tag_type[1], path))


if __name__ == "__main__":
    main(sys.argv[1])
