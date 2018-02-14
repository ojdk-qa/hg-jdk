/*
 * Copyright (c) 2013, 2015, Red Hat, Inc. and/or its affiliates.
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

#ifndef SHARE_VM_GC_SHENANDOAH_SHENANDOAHBARRIERSET_HPP
#define SHARE_VM_GC_SHENANDOAH_SHENANDOAHBARRIERSET_HPP

#include "gc/shenandoah/shenandoahHeap.hpp"
#include "gc/shared/barrierSet.hpp"

class ShenandoahBarrierSet: public BarrierSet {
private:

  ShenandoahHeap* _heap;

public:

  ShenandoahBarrierSet(ShenandoahHeap* heap);

  void print_on(outputStream* st) const;

  bool is_a(BarrierSet::Name bsn);

  bool is_aligned(HeapWord* hw);
  void resize_covered_region(MemRegion mr);

  void write_ref_array(HeapWord* start, size_t count);
  void write_ref_array_work(MemRegion r);

  template <class T> void
  write_ref_array_pre_work(T* dst, int count);

  void write_ref_array_pre(oop* dst, int count, bool dest_uninitialized);

  void write_ref_array_pre(narrowOop* dst, int count, bool dest_uninitialized);


  // We export this to make it available in cases where the static
  // type of the barrier set is known.  Note that it is non-virtual.
  template <class T> inline void inline_write_ref_field_pre(T* field, oop new_val);

  // These are the more general virtual versions.
  void write_ref_field_pre_work(oop* field, oop new_val);
  void write_ref_field_pre_work(narrowOop* field, oop new_val);
  void write_ref_field_pre_work(void* field, oop new_val);

  void write_ref_field_work(void* v, oop o, bool release = false);
  void write_region_work(MemRegion mr);

  virtual oop read_barrier(oop src);

  static inline oop resolve_oop_static_not_null(oop p);

  static inline oop resolve_oop_static(oop p);

  virtual oop write_barrier(oop obj);
  static oopDesc* write_barrier_IRT(oopDesc* src);
  static oopDesc* write_barrier_JRT(oopDesc* src);

  virtual oop storeval_barrier(oop obj);

  virtual void keep_alive_barrier(oop obj);

  bool obj_equals(oop obj1, oop obj2);
  bool obj_equals(narrowOop obj1, narrowOop obj2);

#ifdef ASSERT
  virtual bool is_safe(oop o);
  virtual bool is_safe(narrowOop o);
  virtual void verify_safe_oop(oop p);
#endif

  static void enqueue(oop obj);

private:
  bool need_update_refs_barrier();

  template <class T, bool UPDATE_MATRIX, bool STOREVAL_WRITE_BARRIER, bool ALWAYS_ENQUEUE>
  void write_ref_array_loop(HeapWord* start, size_t count);

  oop write_barrier_impl(oop obj);

#ifndef CC_INTERP
public:
  void interpreter_read_barrier(MacroAssembler* masm, Register dst);
  void interpreter_read_barrier_not_null(MacroAssembler* masm, Register dst);
  void interpreter_write_barrier(MacroAssembler* masm, Register dst);
  void interpreter_storeval_barrier(MacroAssembler* masm, Register dst, Register tmp);
  void asm_acmp_barrier(MacroAssembler* masm, Register op1, Register op2);

private:
  void interpreter_read_barrier_impl(MacroAssembler* masm, Register dst);
  void interpreter_read_barrier_not_null_impl(MacroAssembler* masm, Register dst);
  void interpreter_write_barrier_impl(MacroAssembler* masm, Register dst);

#endif

  static void keep_alive_if_weak(DecoratorSet decorators, oop value) {
    assert((decorators & ON_UNKNOWN_OOP_REF) == 0, "Reference strength must be known");
    const bool on_strong_oop_ref = (decorators & ON_STRONG_OOP_REF) != 0;
    const bool peek              = (decorators & AS_NO_KEEPALIVE) != 0;
    if (!peek && !on_strong_oop_ref && value != NULL) {
      ((ShenandoahBarrierSet*) BarrierSet::barrier_set())->keep_alive_barrier(value);
    }
  }

public:
  // Callbacks for runtime accesses.
  template <DecoratorSet decorators, typename BarrierSetT = ShenandoahBarrierSet>
  class AccessBarrier: public BarrierSet::AccessBarrier<decorators, BarrierSetT> {
    typedef BarrierSet::AccessBarrier<decorators, BarrierSetT> Raw;

  public:
    // Primitive heap accesses. These accessors get resolved when
    // IN_HEAP is set (e.g. when using the HeapAccess API), it is
    // not an oop_* overload, and the barrier strength is AS_NORMAL.
    template <typename T>
    static T load_in_heap(T* addr) {
      ShouldNotReachHere();
      return Raw::template load<T>(addr);
    }

    template <typename T>
    static T load_in_heap_at(oop base, ptrdiff_t offset) {
      base = ((ShenandoahBarrierSet*) BarrierSet::barrier_set())->read_barrier(base);
      return Raw::template load_at<T>(base, offset);
    }

    template <typename T>
    static void store_in_heap(T* addr, T value) {
      ShouldNotReachHere();
      Raw::store(addr, value);
    }

    template <typename T>
    static void store_in_heap_at(oop base, ptrdiff_t offset, T value) {
      base = ((ShenandoahBarrierSet*) BarrierSet::barrier_set())->write_barrier(base);
      Raw::store_at(base, offset, value);
    }

    template <typename T>
    static T atomic_cmpxchg_in_heap(T new_value, T* addr, T compare_value) {
      ShouldNotReachHere();
      return Raw::atomic_cmpxchg(new_value, addr, compare_value);
    }

    template <typename T>
    static T atomic_cmpxchg_in_heap_at(T new_value, oop base, ptrdiff_t offset, T compare_value) {
      base = ((ShenandoahBarrierSet*) BarrierSet::barrier_set())->write_barrier(base);
      return Raw::oop_atomic_cmpxchg_at(new_value, base, offset, compare_value);
    }

    template <typename T>
    static T atomic_xchg_in_heap(T new_value, T* addr) {
      ShouldNotReachHere();
      return Raw::atomic_xchg(new_value, addr);
    }

    template <typename T>
    static T atomic_xchg_in_heap_at(T new_value, oop base, ptrdiff_t offset) {
      base = ((ShenandoahBarrierSet*) BarrierSet::barrier_set())->write_barrier(base);
      return Raw::atomic_xchg_at(new_value, base, offset);
    }

    template <typename T>
    static bool arraycopy_in_heap(arrayOop src_obj, arrayOop dst_obj, T* src, T* dst, size_t length) {
      return Raw::arraycopy(src_obj, dst_obj, src, dst, length);
    }

    // Heap oop accesses. These accessors get resolved when
    // IN_HEAP is set (e.g. when using the HeapAccess API), it is
    // an oop_* overload, and the barrier strength is AS_NORMAL.
    template <typename T>
    static oop oop_load_in_heap(T* addr) {
      ShouldNotReachHere();
      oop value = Raw::template oop_load<oop>(addr);
      keep_alive_if_weak(decorators, value);
      return value;
    }

    static oop oop_load_in_heap_at(oop base, ptrdiff_t offset) {
      base = ((ShenandoahBarrierSet*) BarrierSet::barrier_set())->read_barrier(base);
      oop value = Raw::template oop_load_at<oop>(base, offset);
      keep_alive_if_weak(AccessBarrierSupport::resolve_possibly_unknown_oop_ref_strength<decorators>(base, offset), value);
      return value;
    }

    template <typename T>
    static void oop_store_in_heap(T* addr, oop value) {
      ((ShenandoahBarrierSet*) BarrierSet::barrier_set())->write_ref_field_pre_work(addr, value);
      Raw::oop_store(addr, value);
    }

    static void oop_store_in_heap_at(oop base, ptrdiff_t offset, oop value) {
      base = ((ShenandoahBarrierSet*) BarrierSet::barrier_set())->write_barrier(base);
      value = ((ShenandoahBarrierSet*) BarrierSet::barrier_set())->storeval_barrier(value);

      oop_store_in_heap(AccessInternal::oop_field_addr<decorators>(base, offset), value);
    }

    template <typename T>
    static oop oop_atomic_cmpxchg_in_heap(oop new_value, T* addr, oop compare_value);

    static oop oop_atomic_cmpxchg_in_heap_at(oop new_value, oop base, ptrdiff_t offset, oop compare_value) {
      base = ((ShenandoahBarrierSet*) BarrierSet::barrier_set())->write_barrier(base);
      new_value = ((ShenandoahBarrierSet*) BarrierSet::barrier_set())->storeval_barrier(new_value);
      return oop_atomic_cmpxchg_in_heap(new_value, AccessInternal::oop_field_addr<decorators>(base, offset), compare_value);
    }

    template <typename T>
    static oop oop_atomic_xchg_in_heap(oop new_value, T* addr);

    static oop oop_atomic_xchg_in_heap_at(oop new_value, oop base, ptrdiff_t offset) {
      base = ((ShenandoahBarrierSet*) BarrierSet::barrier_set())->write_barrier(base);
      new_value = ((ShenandoahBarrierSet*) BarrierSet::barrier_set())->storeval_barrier(new_value);
      return oop_atomic_xchg_in_heap(new_value, AccessInternal::oop_field_addr<decorators>(base, offset));
    }

    template <typename T>
    static bool oop_arraycopy_in_heap(arrayOop src_obj, arrayOop dst_obj, T* src, T* dst, size_t length) {
      ((ShenandoahBarrierSet*) BarrierSet::barrier_set())->write_ref_array_pre(dst, length, false);
      bool success = Raw::oop_arraycopy(src_obj, dst_obj, src, dst, length);
      ((ShenandoahBarrierSet*) BarrierSet::barrier_set())->write_ref_array((HeapWord*) dst, length);
      return success;
    }

    // Clone barrier support
    static void clone_in_heap(oop src, oop dst, size_t size) {
      Raw::clone(src, dst, size);
      ((ShenandoahBarrierSet*) BarrierSet::barrier_set())->write_region(MemRegion((HeapWord*) dst, size));
    }

    // Needed for loads on non-heap weak references
    template <typename T>
    static oop oop_load_not_in_heap(T* addr) {
      oop value = Raw::oop_load_not_in_heap(addr);
      keep_alive_if_weak(decorators, value);
      return value;
    }

  };

};

template<>
struct BarrierSet::GetName<ShenandoahBarrierSet> {
  static const BarrierSet::Name value = BarrierSet::Shenandoah;
};

template<>
struct BarrierSet::GetType<BarrierSet::Shenandoah> {
  typedef ShenandoahBarrierSet type;
};

#endif //SHARE_VM_GC_SHENANDOAH_SHENANDOAHBARRIERSET_HPP
