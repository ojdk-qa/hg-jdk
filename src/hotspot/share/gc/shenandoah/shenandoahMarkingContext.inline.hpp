/*
 * Copyright (c) 2018, Red Hat, Inc. and/or its affiliates.
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

#ifndef SHARE_VM_GC_SHENANDOAH_SHENANDOAHMARKINGCONTEXT_INLINE_HPP
#define SHARE_VM_GC_SHENANDOAH_SHENANDOAHMARKINGCONTEXT_INLINE_HPP

#include "gc/shenandoah/shenandoahMarkingContext.hpp"

inline MarkBitMap* ShenandoahMarkingContext::mark_bit_map() {
  return &_mark_bit_map;
}

inline bool ShenandoahMarkingContext::mark(oop obj) {
  shenandoah_assert_not_forwarded(NULL, obj);
  HeapWord* addr = (HeapWord*) obj;
  return (! allocated_after_mark_start(addr)) && _mark_bit_map.parMark(addr);
}

inline bool ShenandoahMarkingContext::is_marked(oop obj) const {
  HeapWord* addr = (HeapWord*) obj;
  return allocated_after_mark_start(addr) || _mark_bit_map.isMarked(addr);
}

inline bool ShenandoahMarkingContext::allocated_after_mark_start(HeapWord* addr) const {
  uintx index = ((uintx) addr) >> ShenandoahHeapRegion::region_size_bytes_shift();
  HeapWord* top_at_mark_start = _top_at_mark_starts[index];
  bool alloc_after_mark_start = addr >= top_at_mark_start;
  return alloc_after_mark_start;
}

#endif // SHARE_VM_GC_SHENANDOAH_SHENANDOAHMARKINGCONTEXT_INLINE_HPP
