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

#include "c1/c1_IR.hpp"
#include "gc/g1/satbMarkQueue.hpp"
#include "gc/shenandoah/brooksPointer.hpp"
#include "gc/shenandoah/shenandoahBarrierSetAssembler.hpp"
#include "gc/shenandoah/shenandoahConnectionMatrix.hpp"
#include "gc/shenandoah/shenandoahHeap.hpp"
#include "gc/shenandoah/shenandoahHeapRegion.hpp"
#include "gc/shenandoah/shenandoahThreadLocalData.hpp"
#include "gc/shenandoah/c1/shenandoahBarrierSetC1.hpp"

#ifdef ASSERT
#define __ gen->lir(__FILE__, __LINE__)->
#else
#define __ gen->lir()->
#endif

void ShenandoahPreBarrierStub::emit_code(LIR_Assembler* ce) {
  ShenandoahBarrierSetAssembler* bs = (ShenandoahBarrierSetAssembler*)BarrierSet::barrier_set()->barrier_set_assembler();
  bs->gen_pre_barrier_stub(ce, this);
}

void ShenandoahWriteBarrierStub::emit_code(LIR_Assembler* ce) {
  ShenandoahBarrierSetAssembler* bs = (ShenandoahBarrierSetAssembler*)BarrierSet::barrier_set()->barrier_set_assembler();
  bs->gen_write_barrier_stub(ce, this);
}

void ShenandoahBarrierSetC1::pre_barrier(LIRAccess& access, LIR_Opr addr_opr, LIR_Opr pre_val) {
  LIRGenerator* gen = access.gen();
  CodeEmitInfo* info = access.access_emit_info();
  DecoratorSet decorators = access.decorators();

  // First we test whether marking is in progress.
  BasicType flag_type;
  bool patch = (decorators & C1_NEEDS_PATCHING) != 0;
  bool do_load = pre_val == LIR_OprFact::illegalOpr;
  if (in_bytes(SATBMarkQueue::byte_width_of_active()) == 4) {
    flag_type = T_INT;
  } else {
    guarantee(in_bytes(SATBMarkQueue::byte_width_of_active()) == 1,
              "Assumption");
    // Use unsigned type T_BOOLEAN here rather than signed T_BYTE since some platforms, eg. ARM,
    // need to use unsigned instructions to use the large offset to load the satb_mark_queue.
    flag_type = T_BOOLEAN;
  }
  LIR_Opr thrd = gen->getThreadPointer();
  LIR_Address* mark_active_flag_addr =
    new LIR_Address(thrd,
                    in_bytes(ShenandoahThreadLocalData::satb_mark_queue_active_offset()),
                    flag_type);
  // Read the marking-in-progress flag.
  LIR_Opr flag_val = gen->new_register(T_INT);
  __ load(mark_active_flag_addr, flag_val);
  __ cmp(lir_cond_notEqual, flag_val, LIR_OprFact::intConst(0));

  LIR_PatchCode pre_val_patch_code = lir_patch_none;

  CodeStub* slow;

  if (do_load) {
    assert(pre_val == LIR_OprFact::illegalOpr, "sanity");
    assert(addr_opr != LIR_OprFact::illegalOpr, "sanity");

    if (patch)
      pre_val_patch_code = lir_patch_normal;

    pre_val = gen->new_register(T_OBJECT);

    if (!addr_opr->is_address()) {
      assert(addr_opr->is_register(), "must be");
      addr_opr = LIR_OprFact::address(new LIR_Address(addr_opr, T_OBJECT));
    }
    slow = new ShenandoahPreBarrierStub(addr_opr, pre_val, pre_val_patch_code, info ? new CodeEmitInfo(info) : NULL);
  } else {
    assert(addr_opr == LIR_OprFact::illegalOpr, "sanity");
    assert(pre_val->is_register(), "must be");
    assert(pre_val->type() == T_OBJECT, "must be an object");

    slow = new ShenandoahPreBarrierStub(pre_val);
  }

  __ branch(lir_cond_notEqual, T_INT, slow);
  __ branch_destination(slow->continuation());
}

void ShenandoahBarrierSetC1::post_barrier(LIRAccess& access, LIR_OprDesc* addr, LIR_OprDesc* new_val) {
  if (! UseShenandoahMatrix) {
    // No need for that barrier if not using matrix.
    return;
  }

  LIRGenerator* gen = access.gen();

  // If the "new_val" is a constant NULL, no barrier is necessary.
  if (new_val->is_constant() &&
      new_val->as_constant_ptr()->as_jobject() == NULL) return;

  new_val = ensure_in_register(access, new_val);
  assert(new_val->is_register(), "must be a register at this point");

  if (addr->is_address()) {
    LIR_Address* address = addr->as_address_ptr();
    LIR_Opr ptr = gen->new_pointer_register();
    if (!address->index()->is_valid() && address->disp() == 0) {
      __ move(address->base(), ptr);
    } else {
      // assert(address->disp() != max_jint, "lea doesn't support patched addresses!");
      __ leal(addr, ptr);
    }
    addr = ptr;
  }
  assert(addr->is_register(), "must be a register at this point");

  LabelObj* L_done = new LabelObj();
  __ cmp(lir_cond_equal, new_val, LIR_OprFact::oopConst(NULL_WORD));
  __ branch(lir_cond_equal, T_OBJECT, L_done->label());

  ShenandoahConnectionMatrix* matrix = ShenandoahHeap::heap()->connection_matrix();

  LIR_Opr heap_base = gen->new_pointer_register();
  __ move(LIR_OprFact::intptrConst(ShenandoahHeap::heap()->base()), heap_base);

  LIR_Opr tmp1 = gen->new_pointer_register();
  __ move(new_val, tmp1);
  __ sub(tmp1, heap_base, tmp1);
  __ unsigned_shift_right(tmp1, LIR_OprFact::intConst(ShenandoahHeapRegion::region_size_bytes_shift_jint()), tmp1, LIR_OprDesc::illegalOpr());

  LIR_Opr tmp2 = gen->new_pointer_register();
  __ move(addr, tmp2);
  __ sub(tmp2, heap_base, tmp2);
  __ unsigned_shift_right(tmp2, LIR_OprFact::intConst(ShenandoahHeapRegion::region_size_bytes_shift_jint()), tmp2, LIR_OprDesc::illegalOpr());

  LIR_Opr tmp3 = gen->new_pointer_register();
  __ move(LIR_OprFact::longConst(matrix->stride_jint()), tmp3);
  __ mul(tmp1, tmp3, tmp1);
  __ add(tmp1, tmp2, tmp1);

  LIR_Opr tmp4 = gen->new_pointer_register();
  __ move(LIR_OprFact::intptrConst((intptr_t) matrix->matrix_addr()), tmp4);
  LIR_Address* matrix_elem_addr = new LIR_Address(tmp4, tmp1, T_BYTE);

  LIR_Opr tmp5 = gen->new_register(T_INT);
  __ move(matrix_elem_addr, tmp5);
  __ cmp(lir_cond_notEqual, tmp5, LIR_OprFact::intConst(0));
  __ branch(lir_cond_notEqual, T_BYTE, L_done->label());

  // Aarch64 cannot move constant 1. Load it into a register.
  LIR_Opr one = gen->new_register(T_INT);
  __ move(LIR_OprFact::intConst(1), one);
  __ move(one, matrix_elem_addr);

  __ branch_destination(L_done->label());
}

LIR_Opr ShenandoahBarrierSetC1::read_barrier(LIRAccess& access, LIR_Opr obj, CodeEmitInfo* info, bool need_null_check) {
  if (UseShenandoahGC && ShenandoahReadBarrier) {
    return read_barrier_impl(access, obj, info, need_null_check);
  } else {
    return obj;
  }
}

LIR_Opr ShenandoahBarrierSetC1::read_barrier_impl(LIRAccess& access, LIR_Opr obj, CodeEmitInfo* info, bool need_null_check) {
  assert(UseShenandoahGC && (ShenandoahReadBarrier || ShenandoahStoreValReadBarrier), "Should be enabled");
  LIRGenerator* gen = access.gen();
  LabelObj* done = new LabelObj();
  LIR_Opr result = gen->new_register(T_OBJECT);
  __ move(obj, result);
  if (need_null_check) {
    __ cmp(lir_cond_equal, result, LIR_OprFact::oopConst(NULL));
    __ branch(lir_cond_equal, T_LONG, done->label());
  }
  LIR_Address* brooks_ptr_address = gen->generate_address(result, BrooksPointer::byte_offset(), T_ADDRESS);
  __ load(brooks_ptr_address, result, info ? new CodeEmitInfo(info) : NULL, lir_patch_none);

  __ branch_destination(done->label());
  return result;
}

LIR_Opr ShenandoahBarrierSetC1::write_barrier(LIRAccess& access, LIR_Opr obj, CodeEmitInfo* info, bool need_null_check) {
  if (UseShenandoahGC && ShenandoahWriteBarrier) {
    return write_barrier_impl(access, obj, info, need_null_check);
  } else {
    return obj;
  }
}

LIR_Opr ShenandoahBarrierSetC1::write_barrier_impl(LIRAccess& access, LIR_Opr obj, CodeEmitInfo* info, bool need_null_check) {
  assert(UseShenandoahGC && (ShenandoahWriteBarrier || ShenandoahStoreValEnqueueBarrier), "Should be enabled");
  LIRGenerator* gen = access.gen();

  obj = ensure_in_register(access, obj);
  assert(obj->is_register(), "must be a register at this point");
  LIR_Opr result = gen->new_register(T_OBJECT);
  __ move(obj, result);

  LIR_Opr thrd = gen->getThreadPointer();
  LIR_Address* active_flag_addr =
    new LIR_Address(thrd,
                    in_bytes(ShenandoahThreadLocalData::gc_state_offset()),
                    T_BYTE);
  // Read and check the gc-state-flag.
  LIR_Opr flag_val = gen->new_register(T_INT);
  __ load(active_flag_addr, flag_val);
  LIR_Opr mask = LIR_OprFact::intConst(ShenandoahHeap::HAS_FORWARDED |
                                       ShenandoahHeap::EVACUATION |
                                       ShenandoahHeap::TRAVERSAL);
  LIR_Opr mask_reg = gen->new_register(T_INT);
  __ move(mask, mask_reg);

  if (TwoOperandLIRForm) {
    __ logical_and(flag_val, mask_reg, flag_val);
  } else {
    LIR_Opr masked_flag = gen->new_register(T_INT);
    __ logical_and(flag_val, mask_reg, masked_flag);
    flag_val = masked_flag;
  }
  __ cmp(lir_cond_notEqual, flag_val, LIR_OprFact::intConst(0));

  CodeStub* slow = new ShenandoahWriteBarrierStub(obj, result, info ? new CodeEmitInfo(info) : NULL, need_null_check);
  __ branch(lir_cond_notEqual, T_INT, slow);
  __ branch_destination(slow->continuation());

  return result;
}

LIR_Opr ShenandoahBarrierSetC1::ensure_in_register(LIRAccess& access, LIR_Opr obj) {
  if (!obj->is_register()) {
    LIRGenerator* gen = access.gen();
    LIR_Opr obj_reg = gen->new_register(T_OBJECT);
    if (obj->is_constant()) {
      __ move(obj, obj_reg);
    } else {
      __ leal(obj, obj_reg);
    }
    obj = obj_reg;
  }
  return obj;
}

LIR_Opr ShenandoahBarrierSetC1::storeval_barrier(LIRAccess& access, LIR_Opr obj, CodeEmitInfo* info, bool need_null_check) {
  LIRGenerator* gen = access.gen();
  if (UseShenandoahGC) {
    if (ShenandoahStoreValEnqueueBarrier) {
      obj = write_barrier_impl(access, obj, info, need_null_check);
      pre_barrier(access, LIR_OprFact::illegalOpr, obj);
    }
    if (ShenandoahStoreValReadBarrier) {
      obj = read_barrier_impl(access, obj, info, true /*need_null_check*/);
    }
  }
  return obj;
}

void ShenandoahBarrierSetC1::store_at(LIRAccess& access, LIR_Opr value) {
  access.set_base(write_barrier(access, access.base().item().result(), access.access_emit_info(), access.needs_null_check()));
  LIR_Opr resolved = resolve_address(access, false);
  access.set_resolved_addr(resolved);
  if (access.is_oop()) {
    if (ShenandoahSATBBarrier) {
      pre_barrier(access, access.resolved_addr(), LIR_OprFact::illegalOpr /* pre_val */);
    }
    value = storeval_barrier(access, value, access.access_emit_info(), access.needs_null_check());
  }
  BarrierSetC1::store_at_resolved(access, value);

  if (access.is_oop() && UseShenandoahMatrix) {
    post_barrier(access, access.resolved_addr(), value);
  }
}

void ShenandoahBarrierSetC1::load_at(LIRAccess& access, LIR_Opr result) {
  LIRItem& base_item = access.base().item();
  access.set_base(read_barrier(access, access.base().item().result(), access.access_emit_info(), access.needs_null_check()));
  LIR_Opr resolved = resolve_address(access, false);
  access.set_resolved_addr(resolved);
  BarrierSetC1::load_at_resolved(access, result);
  access.set_base(base_item);

  if (ShenandoahKeepAliveBarrier) {
    DecoratorSet decorators = access.decorators();
    bool is_weak = (decorators & ON_WEAK_OOP_REF) != 0;
    bool is_phantom = (decorators & ON_PHANTOM_OOP_REF) != 0;
    bool is_anonymous = (decorators & ON_UNKNOWN_OOP_REF) != 0;
    LIRGenerator *gen = access.gen();
    if (access.is_oop() && (is_weak || is_phantom || is_anonymous)) {
      // Register the value in the referent field with the pre-barrier
      LabelObj *Lcont_anonymous;
      if (is_anonymous) {
        Lcont_anonymous = new LabelObj();
        generate_referent_check(access, Lcont_anonymous);
      }
      pre_barrier(access, LIR_OprFact::illegalOpr /* addr_opr */,
                  result /* pre_val */);
      if (is_anonymous) {
        __ branch_destination(Lcont_anonymous->label());
      }
    }
  }
}

LIR_Opr ShenandoahBarrierSetC1::atomic_cmpxchg_at(LIRAccess& access, LIRItem& cmp_value, LIRItem& new_value) {
  access.load_address();
  access.set_base(write_barrier(access, access.base().item().result(), access.access_emit_info(), access.needs_null_check()));
  LIR_Opr resolved = resolve_address(access, true);
  access.set_resolved_addr(resolved);
  if (access.is_oop()) {
    if (ShenandoahSATBBarrier) {
      pre_barrier(access, access.resolved_addr(),
                  LIR_OprFact::illegalOpr /* pre_val */);
    }
  }
  LIR_Opr result = atomic_cmpxchg_at_resolved(access, cmp_value, new_value);
  if (access.is_oop() && UseShenandoahMatrix) {
    post_barrier(access, access.resolved_addr(), new_value.result());
  }
  return result;
}

LIR_Opr ShenandoahBarrierSetC1::atomic_xchg_at(LIRAccess& access, LIRItem& value) {
  access.set_base(write_barrier(access, access.base().item().result(), access.access_emit_info(), access.needs_null_check()));
  LIR_Opr resolved = resolve_address(access, true);
  access.set_resolved_addr(resolved);
  if (access.is_oop()) {
    if (ShenandoahSATBBarrier) {
      pre_barrier(access, access.resolved_addr(),
                  LIR_OprFact::illegalOpr /* pre_val */);
    }
  }
  LIR_Opr result = BarrierSetC1::atomic_xchg_at_resolved(access, value);
  if (access.is_oop() && UseShenandoahMatrix) {
    post_barrier(access, access.resolved_addr(), value.result());
  }
  return result;
}

LIR_Opr ShenandoahBarrierSetC1::atomic_add_at(LIRAccess& access, LIRItem& value) {
  access.load_address();
  access.set_base(write_barrier(access, access.base().item().result(), access.access_emit_info(), access.needs_null_check()));
  LIR_Opr resolved = resolve_address(access, true);
  access.set_resolved_addr(resolved);
  return BarrierSetC1::atomic_add_at_resolved(access, value);
}

LIR_Opr ShenandoahBarrierSetC1::resolve_for_read(LIRAccess& access) {
  return read_barrier(access, access.base().opr(), access.access_emit_info(), access.needs_null_check());
}

LIR_Opr ShenandoahBarrierSetC1::resolve_for_write(LIRAccess& access) {
  return write_barrier(access, access.base().opr(), access.access_emit_info(), access.needs_null_check());
}

class C1ShenandoahPreBarrierCodeGenClosure : public StubAssemblerCodeGenClosure {
  virtual OopMapSet* generate_code(StubAssembler* sasm) {
    ShenandoahBarrierSetAssembler* bs = (ShenandoahBarrierSetAssembler*)BarrierSet::barrier_set()->barrier_set_assembler();
    bs->generate_c1_pre_barrier_runtime_stub(sasm);
    return NULL;
  }
};

void ShenandoahBarrierSetC1::generate_c1_runtime_stubs(BufferBlob* buffer_blob) {
  C1ShenandoahPreBarrierCodeGenClosure pre_code_gen_cl;
  _pre_barrier_c1_runtime_code_blob = Runtime1::generate_blob(buffer_blob, -1,
                                                              "shenandoah_pre_barrier_slow",
                                                              false, &pre_code_gen_cl);
}

