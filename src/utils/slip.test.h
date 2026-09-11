/*
 Copyright (C) 2017-2026 Fredrik Öhrström (gpl-3.0-or-later)

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef UTILS_SLIP_TEST_H
#define UTILS_SLIP_TEST_H

#include "test.h"
#include "utils/slip.h"

#include <vector>

static auto _slip_suite = describe("slip", []()
{
    // === slipAllEND ===

    it("slipAllEND returns true for empty vector", []()
    {
        std::vector<uchar> v;
        assert(slipAllEND(v));
    });

    it("slipAllEND returns true when all bytes are 0xc0", []()
    {
        std::vector<uchar> v = { 0xc0, 0xc0, 0xc0 };
        assert(slipAllEND(v));
    });

    it("slipAllEND returns false when payload bytes are present", []()
    {
        std::vector<uchar> v = { 0xc0, 0x01, 0xc0 };
        assert(!slipAllEND(v));
    });

    // === addSlipFraming ===

    it("addSlipFraming appends leading and trailing END markers", []()
    {
        std::vector<uchar> from = { 0x01, 0x02, 0x03 };
        std::vector<uchar> to;
        addSlipFraming(from, to);
        assert(to.front() == (uchar)0xc0);
        assert(to.back()  == (uchar)0xc0);
    });

    it("addSlipFraming escapes 0xc0 in payload", []()
    {
        std::vector<uchar> from = { 0xc0 };
        std::vector<uchar> to;
        addSlipFraming(from, to);
        // leading 0xc0, then 0xdb 0xdc, then trailing 0xc0
        assert(to.size() == 4);
        assert(to[0] == (uchar)0xc0);
        assert(to[1] == (uchar)0xdb);
        assert(to[2] == (uchar)0xdc);
        assert(to[3] == (uchar)0xc0);
    });

    it("addSlipFraming escapes 0xdb in payload", []()
    {
        std::vector<uchar> from = { 0xdb };
        std::vector<uchar> to;
        addSlipFraming(from, to);
        // leading 0xc0, then 0xdb 0xdd, then trailing 0xc0
        assert(to.size() == 4);
        assert(to[0] == (uchar)0xc0);
        assert(to[1] == (uchar)0xdb);
        assert(to[2] == (uchar)0xdd);
        assert(to[3] == (uchar)0xc0);
    });

    it("addSlipFraming encodes mixed payload with embedded 0xc0 and 0xdb correctly", []()
    {
        // Payload: { 1, 0xc0, 3, 4, 5, 0xdb }
        // Expected wire: C0 01 DB DC 03 04 05 DB DD C0
        std::vector<uchar> from = { 1, 0xc0, 3, 4, 5, 0xdb };
        std::vector<uchar> expected = { 0xc0, 1, 0xdb, 0xdc, 3, 4, 5, 0xdb, 0xdd, 0xc0 };
        std::vector<uchar> to;
        addSlipFraming(from, to);
        assert(to == expected);
    });

    // === removeSlipFraming — OK ===

    it("removeSlipFraming round-trips plain data", []()
    {
        std::vector<uchar> original = { 0x01, 0x02, 0x03 };
        std::vector<uchar> framed;
        addSlipFraming(original, framed);

        std::vector<uchar> decoded;
        size_t frame_len = 0;
        SlipFrameResult r = removeSlipFraming(framed, &frame_len, decoded);
        assert(r == SlipFrameResult::OK);
        assert(decoded == original);
    });

    it("removeSlipFraming round-trips data containing 0xc0", []()
    {
        std::vector<uchar> original = { 0xc0, 0x01, 0xc0 };
        std::vector<uchar> framed;
        addSlipFraming(original, framed);

        std::vector<uchar> decoded;
        size_t frame_len = 0;
        SlipFrameResult r = removeSlipFraming(framed, &frame_len, decoded);
        assert(r == SlipFrameResult::OK);
        assert(decoded == original);
    });

    it("removeSlipFraming OK sets frame_length to wire bytes consumed", []()
    {
        std::vector<uchar> from = { 1, 0xc0, 3, 4, 5, 0xdb };
        std::vector<uchar> encoded;
        addSlipFraming(from, encoded);

        std::vector<uchar> decoded;
        size_t frame_len = 0;
        SlipFrameResult r = removeSlipFraming(encoded, &frame_len, decoded);
        assert(r == SlipFrameResult::OK);
        assert(decoded == from);
        assert(frame_len == encoded.size());
    });

    it("removeSlipFraming decodes first of two chained frames", []()
    {
        std::vector<uchar> first  = { 1, 0xc0, 3, 4, 5, 0xdb };
        std::vector<uchar> second = { 0xc0, 0xc0, 0xc0, 1, 2, 3, 4, 5, 6, 7, 8 };
        std::vector<uchar> buf;
        addSlipFraming(first, buf);
        addSlipFraming(second, buf);

        std::vector<uchar> decoded;
        size_t frame_len = 0;
        SlipFrameResult r = removeSlipFraming(buf, &frame_len, decoded);
        assert(r == SlipFrameResult::OK);
        assert(decoded == first);
    });

    it("removeSlipFraming decodes second frame after consuming first", []()
    {
        std::vector<uchar> first  = { 1, 0xc0, 3, 4, 5, 0xdb };
        std::vector<uchar> second = { 0xc0, 0xc0, 0xc0, 1, 2, 3, 4, 5, 6, 7, 8 };
        std::vector<uchar> buf;
        addSlipFraming(first, buf);
        addSlipFraming(second, buf);

        std::vector<uchar> decoded;
        size_t frame_len = 0;
        removeSlipFraming(buf, &frame_len, decoded);        // consume first
        buf.erase(buf.begin(), buf.begin() + frame_len);   // advance buffer

        SlipFrameResult r = removeSlipFraming(buf, &frame_len, decoded);
        assert(r == SlipFrameResult::OK);
        assert(decoded == second);
    });

    // === removeSlipFraming — PARTIAL ===

    it("removeSlipFraming returns PARTIAL when no END marker present", []()
    {
        std::vector<uchar> incomplete = { 0x01, 0x02 };
        std::vector<uchar> decoded;
        size_t frame_len = 0;
        SlipFrameResult r = removeSlipFraming(incomplete, &frame_len, decoded);
        assert(r == SlipFrameResult::PARTIAL);
        assert(frame_len == 0);
    });

    it("removeSlipFraming returns PARTIAL for only a leading SLIP_END", []()
    {
        std::vector<uchar> v = { 0xc0 };
        std::vector<uchar> decoded;
        size_t frame_len = 0;
        SlipFrameResult r = removeSlipFraming(v, &frame_len, decoded);
        assert(r == SlipFrameResult::PARTIAL);
        assert(frame_len == 0);
    });

    it("removeSlipFraming returns PARTIAL and clears output when payload has no trailing END", []()
    {
        std::vector<uchar> v = { 0xc0, 1, 2, 3, 4, 5 };
        std::vector<uchar> decoded;
        size_t frame_len = 0;
        SlipFrameResult r = removeSlipFraming(v, &frame_len, decoded);
        assert(r == SlipFrameResult::PARTIAL);
        assert(frame_len == 0);
        assert(decoded.empty());
    });

    it("removeSlipFraming returns PARTIAL for trailing ESC with no following byte", []()
    {
        // Next byte might still be valid; cannot declare corrupt yet.
        std::vector<uchar> v = { 0xc0, 0x01, 0x02, 0xdb };
        std::vector<uchar> decoded;
        size_t frame_len = 0;
        SlipFrameResult r = removeSlipFraming(v, &frame_len, decoded);
        assert(r == SlipFrameResult::PARTIAL);
        assert(frame_len == 0);
    });

    // === removeSlipFraming — CORRUPT ===

    it("removeSlipFraming returns CORRUPT for ESC followed by unrecognised byte", []()
    {
        // { C0, DB, 01, C0 }: leading C0 skipped; DB sets esc; 01 is invalid.
        // frame_length = 3 (bytes 0–2 consumed) enables resync.
        std::vector<uchar> v = { 0xc0, 0xdb, 0x01, 0xc0 };
        std::vector<uchar> decoded;
        size_t frame_len = 0;
        SlipFrameResult r = removeSlipFraming(v, &frame_len, decoded);
        assert(r == SlipFrameResult::CORRUPT);
        assert(frame_len == 3);
        assert(decoded.empty());
    });

    it("removeSlipFraming returns CORRUPT for ESC followed by SLIP_END", []()
    {
        // ESC + 0xC0 must not be accepted as a frame terminator.
        // { C0, DB, C0, C0 }: frame_length = 3.
        std::vector<uchar> v = { 0xc0, 0xdb, 0xc0, 0xc0 };
        std::vector<uchar> decoded;
        size_t frame_len = 0;
        SlipFrameResult r = removeSlipFraming(v, &frame_len, decoded);
        assert(r == SlipFrameResult::CORRUPT);
        assert(frame_len == 3);
        assert(decoded.empty());
    });

    it("removeSlipFraming CORRUPT mid-frame reports bytes consumed for resync", []()
    {
        // { C0, 01, 02, DB, 05, C0 }: leading C0 skipped; 01 and 02 decoded;
        // DB sets esc; 05 is invalid → CORRUPT, frame_length = 5.
        // Erasing 5 bytes leaves { C0 } as a clean frame boundary.
        std::vector<uchar> v = { 0xc0, 0x01, 0x02, 0xdb, 0x05, 0xc0 };
        std::vector<uchar> decoded;
        size_t frame_len = 0;
        SlipFrameResult r = removeSlipFraming(v, &frame_len, decoded);
        assert(r == SlipFrameResult::CORRUPT);
        assert(frame_len == 5);
        assert(decoded.empty());
        v.erase(v.begin(), v.begin() + frame_len);
        assert((v == std::vector<uchar>{ 0xc0 }));
    });

    // === slipFrameSize ===

    it("slipFrameSize returns -1 when no END marker present", []()
    {
        std::vector<uchar> v = { 0x01, 0x02, 0x03 };
        assert(slipFrameSize(v) == -1);
    });

    it("slipFrameSize counts wire bytes before trailing END", []()
    {
        std::vector<uchar> from = { 0x01, 0x02, 0x03 };
        std::vector<uchar> framed;
        addSlipFraming(from, framed);
        assert(slipFrameSize(framed) == 3);
    });

    it("slipFrameSize counts escape pairs as two wire bytes", []()
    {
        // Payload { 0xc0 } encodes to DB DC (2 wire bytes) between the END markers.
        std::vector<uchar> from = { 0xc0 };
        std::vector<uchar> framed;
        addSlipFraming(from, framed);
        assert(slipFrameSize(framed) == 2);
    });
});

#endif // UTILS_SLIP_TEST_H
