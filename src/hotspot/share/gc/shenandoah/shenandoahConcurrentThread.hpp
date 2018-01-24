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

#ifndef SHARE_VM_GC_SHENANDOAH_SHENANDOAHCONCURRENTTHREAD_HPP
#define SHARE_VM_GC_SHENANDOAH_SHENANDOAHCONCURRENTTHREAD_HPP

#include "gc/shenandoah/shenandoahHeap.hpp"
#include "gc/shenandoah/shenandoahSharedVariables.hpp"
#include "gc/shared/concurrentGCThread.hpp"
#include "gc/shared/gcCause.hpp"
#include "memory/resourceArea.hpp"
#include "runtime/task.hpp"

// Periodic task is useful for doing asynchronous things that do not require (heap) locks,
// or synchronization with other parts of collector. These could run even when ShenandoahConcurrentThread
// is busy driving the GC cycle.
class ShenandoahPeriodicTask : public PeriodicTask {
private:
  ShenandoahConcurrentThread* _thread;
public:
  ShenandoahPeriodicTask(ShenandoahConcurrentThread* thread) :
          PeriodicTask(100), _thread(thread) {}
  virtual void task();
};

class ShenandoahConcurrentThread: public ConcurrentGCThread {
  friend class VMStructs;

private:
  typedef enum {
    none,
    concurrent_partial,
    concurrent_traversal,
    concurrent_normal,
    stw_degenerated,
    stw_full,
  } GCMode;

  // While we could have a single lock for these, it may risk unblocking
  // explicit GC waiters when alloc failure GC cycle finishes. We want instead
  // to make complete explicit cycle for for demanding customers.
  Monitor _alloc_failure_waiters_lock;
  Monitor _explicit_gc_waiters_lock;
  ShenandoahPeriodicTask _periodic_task;

public:
  void run_service();
  void stop_service();

private:
  ShenandoahSharedFlag _explicit_gc;
  ShenandoahSharedFlag _alloc_failure_gc;
  ShenandoahSharedFlag _graceful_shutdown;
  ShenandoahSharedFlag _do_counters_update;
  ShenandoahSharedFlag _force_counters_update;
  GCCause::Cause _explicit_gc_cause;
  ShenandoahHeap::ShenandoahDegenerationPoint _degen_point;

  bool check_cancellation_or_degen(ShenandoahHeap::ShenandoahDegenerationPoint point);
  void service_concurrent_normal_cycle(GCCause::Cause cause);
  void service_stw_full_cycle(GCCause::Cause cause);
  void service_stw_degenerated_cycle(GCCause::Cause cause, ShenandoahHeap::ShenandoahDegenerationPoint point);
  void service_concurrent_partial_cycle(GCCause::Cause cause);
  void service_concurrent_traversal_cycle(GCCause::Cause cause);

  bool try_set_alloc_failure_gc();
  void notify_alloc_failure_waiters();
  bool is_alloc_failure_gc();

  void notify_explicit_gc_waiters();

public:
  // Constructor
  ShenandoahConcurrentThread();
  ~ShenandoahConcurrentThread();

  // Handle allocation failure from normal allocation.
  // Blocks until memory is available.
  void handle_alloc_failure();

  // Handle allocation failure from evacuation path.
  // Optionally blocks while collector is handling the failure.
  void handle_alloc_failure_evac();

  // Handle explicit GC request.
  // Blocks until GC is over.
  void handle_explicit_gc(GCCause::Cause cause);

  void handle_counters_update();
  void trigger_counters_update();
  void handle_force_counters_update();
  void set_forced_counters_update(bool value);

  void start();
  void prepare_for_graceful_shutdown();
  bool in_graceful_shutdown();

  char* name() const { return (char*)"ShenandoahConcurrentThread";}

  // Printing
  void print_on(outputStream* st) const;
  void print() const;
};

#endif // SHARE_VM_GC_SHENANDOAH_SHENANDOAHCONCURRENTTHREAD_HPP
