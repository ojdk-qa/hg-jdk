/*
* Copyright (c) 2016, Oracle and/or its affiliates. All rights reserved.
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
*/

/*
 * @test TestGCOldWithShenandoah
 * @key gc
 * @key stress
 * @summary Stress the GC by trying to make old objects more likely to be garbage than young objects.
 *
 * @run main/othervm/timeout=600 -Xmx384M -XX:+UseShenandoahGC -XX:+UnlockDiagnosticVMOptions -XX:+UnlockExperimentalVMOptions                                          -XX:+ShenandoahVerify TestGCOld 50 1 20 10 10000
 * @run main/othervm/timeout=600 -Xmx384M -XX:+UseShenandoahGC -XX:+UnlockDiagnosticVMOptions -XX:+UnlockExperimentalVMOptions -XX:ShenandoahGCHeuristics=passive       -XX:+ShenandoahVerify TestGCOld 50 1 20 10 10000
 * @run main/othervm/timeout=600 -Xmx384M -XX:+UseShenandoahGC -XX:+UnlockDiagnosticVMOptions -XX:+UnlockExperimentalVMOptions -XX:ShenandoahGCHeuristics=adaptive      -XX:+ShenandoahVerify TestGCOld 50 1 20 10 10000
 * @run main/othervm/timeout=600 -Xmx384M -XX:+UseShenandoahGC -XX:+UnlockDiagnosticVMOptions -XX:+UnlockExperimentalVMOptions -XX:ShenandoahGCHeuristics=static        -XX:+ShenandoahVerify TestGCOld 50 1 20 10 10000
 * @run main/othervm/timeout=600 -Xmx384M -XX:+UseShenandoahGC -XX:+UnlockDiagnosticVMOptions -XX:+UnlockExperimentalVMOptions -XX:ShenandoahGCHeuristics=connected     -XX:+ShenandoahVerify TestGCOld 50 1 20 10 10000
 * @run main/othervm/timeout=600 -Xmx384M -XX:+UseShenandoahGC -XX:+UnlockDiagnosticVMOptions -XX:+UnlockExperimentalVMOptions -XX:ShenandoahGCHeuristics=generational  -XX:+ShenandoahVerify TestGCOld 50 1 20 10 10000
 * @run main/othervm/timeout=600 -Xmx384M -XX:+UseShenandoahGC -XX:+UnlockDiagnosticVMOptions -XX:+UnlockExperimentalVMOptions -XX:ShenandoahGCHeuristics=LRU           -XX:+ShenandoahVerify TestGCOld 50 1 20 10 10000
 *
 * @run main/othervm -Xmx384M -XX:+UseShenandoahGC -XX:+UnlockDiagnosticVMOptions -XX:+UnlockExperimentalVMOptions                                          TestGCOld 50 1 20 10 10000
 * @run main/othervm -Xmx384M -XX:+UseShenandoahGC -XX:+UnlockDiagnosticVMOptions -XX:+UnlockExperimentalVMOptions -XX:ShenandoahGCHeuristics=passive       TestGCOld 50 1 20 10 10000
 * @run main/othervm -Xmx384M -XX:+UseShenandoahGC -XX:+UnlockDiagnosticVMOptions -XX:+UnlockExperimentalVMOptions -XX:ShenandoahGCHeuristics=adaptive      TestGCOld 50 1 20 10 10000
 * @run main/othervm -Xmx384M -XX:+UseShenandoahGC -XX:+UnlockDiagnosticVMOptions -XX:+UnlockExperimentalVMOptions -XX:ShenandoahGCHeuristics=static        TestGCOld 50 1 20 10 10000
 * @run main/othervm -Xmx384M -XX:+UseShenandoahGC -XX:+UnlockDiagnosticVMOptions -XX:+UnlockExperimentalVMOptions -XX:ShenandoahGCHeuristics=continuous    TestGCOld 50 1 20 10 10000
 * @run main/othervm -Xmx384M -XX:+UseShenandoahGC -XX:+UnlockDiagnosticVMOptions -XX:+UnlockExperimentalVMOptions -XX:ShenandoahGCHeuristics=aggressive    TestGCOld 50 1 20 10 10000
 * @run main/othervm -Xmx384M -XX:+UseShenandoahGC -XX:+UnlockDiagnosticVMOptions -XX:+UnlockExperimentalVMOptions -XX:ShenandoahGCHeuristics=connected     TestGCOld 50 1 20 10 10000
 * @run main/othervm -Xmx384M -XX:+UseShenandoahGC -XX:+UnlockDiagnosticVMOptions -XX:+UnlockExperimentalVMOptions -XX:ShenandoahGCHeuristics=generational  TestGCOld 50 1 20 10 10000
 * @run main/othervm -Xmx384M -XX:+UseShenandoahGC -XX:+UnlockDiagnosticVMOptions -XX:+UnlockExperimentalVMOptions -XX:ShenandoahGCHeuristics=LRU           TestGCOld 50 1 20 10 10000
 * @run main/othervm -Xmx384M -XX:+UseShenandoahGC -XX:+UnlockDiagnosticVMOptions -XX:+UnlockExperimentalVMOptions -XX:ShenandoahGCHeuristics=aggressive    -XX:+ShenandoahOOMDuringEvacALot TestGCOld 50 1 20 10 10000
 */

public class TestGCOldWithShenandoah {

    public static void main(String[] args) {
        TestGCOld.main(args);
    }
}
