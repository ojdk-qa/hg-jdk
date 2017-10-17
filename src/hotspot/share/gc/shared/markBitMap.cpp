/*
 * Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 *
 */

// Concurrent marking bit map wrapper

#include "precompiled.hpp"
#include "gc/shared/markBitMap.inline.hpp"
#include "utilities/bitMap.inline.hpp"

MarkBitMapRO::MarkBitMapRO(int shifter) :
  _bm(),
  _shifter(shifter) {
  _bmStartWord = 0;
  _bmWordSize = 0;
}

#ifndef PRODUCT
bool MarkBitMapRO::covers(MemRegion heap_rs) const {
  // assert(_bm.map() == _virtual_space.low(), "map inconsistency");
  assert(((size_t)_bm.size() * ((size_t)1 << _shifter)) == _bmWordSize,
         "size inconsistency");
  return _bmStartWord == (HeapWord*)(heap_rs.start()) &&
         _bmWordSize  == heap_rs.word_size();
}
#endif

void MarkBitMapRO::print_on_error(outputStream* st, const char* prefix) const {
  _bm.print_on_error(st, prefix);
}

size_t MarkBitMap::compute_size(size_t heap_size) {
  return ReservedSpace::allocation_align_size_up(heap_size / mark_distance());
}

size_t MarkBitMap::mark_distance() {
  return MinObjAlignmentInBytes * BitsPerByte;
}

void MarkBitMap::initialize(MemRegion heap, MemRegion bitmap) {
  _bmStartWord = heap.start();
  _bmWordSize = heap.word_size();

  _bm = BitMapView((BitMap::bm_word_t*) bitmap.start(), _bmWordSize >> _shifter);
}

void MarkBitMap::clear_range(MemRegion mr) {
  mr.intersection(MemRegion(_bmStartWord, _bmWordSize));
  assert(!mr.is_empty(), "unexpected empty region");
  // convert address range into offset range
  _bm.clear_range(heapWordToOffset(mr.start()),
                  heapWordToOffset(mr.end()));
}

void MarkBitMap::clear_range_large(MemRegion mr) {
  mr.intersection(MemRegion(_bmStartWord, _bmWordSize));
  assert(!mr.is_empty(), "unexpected empty region");
  // convert address range into offset range
  _bm.clear_large_range(heapWordToOffset(mr.start()),
                        heapWordToOffset(mr.end()));
}
