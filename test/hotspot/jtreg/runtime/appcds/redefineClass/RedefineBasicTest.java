/*
 * Copyright (c) 2016, 2017, Oracle and/or its affiliates. All rights reserved.
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

/*
 * @test
 * @summary Run /runtime/RedefineTests/RedefineRunningMethods in AppCDS mode to
 *          make sure class redefinition works with CDS.
 *          (Note: AppCDS does not support uncompressed oops)
 * @requires (vm.opt.UseCompressedOops == null) | (vm.opt.UseCompressedOops == true)
 * @library /test/lib /test/hotspot/jtreg/runtime/RedefineTests /test/hotspot/jtreg/runtime/appcds
 * @modules java.compiler
 *          java.instrument
 *          jdk.jartool/sun.tools.jar
 *          java.base/jdk.internal.misc
 *          java.management
 * @run main RedefineClassHelper
 * @build sun.hotspot.WhiteBox RedefineBasic
 * @run main RedefineBasicTest
 */

import jdk.test.lib.process.OutputAnalyzer;

public class RedefineBasicTest {
    public static String sharedClasses[] = {
        "RedefineBasic",
        "RedefineBasic$B",
        "RedefineBasic$SubclassOfB",
        "RedefineBasic$Subclass2OfB",
        "RedefineClassHelper",
        "jdk/test/lib/compiler/InMemoryJavaCompiler",
        "jdk/test/lib/compiler/InMemoryJavaCompiler$FileManagerWrapper",
        "jdk/test/lib/compiler/InMemoryJavaCompiler$FileManagerWrapper$1",
        "jdk/test/lib/compiler/InMemoryJavaCompiler$MemoryJavaFileObject"
    };

    public static void main(String[] args) throws Exception {
        String wbJar =
            ClassFileInstaller.writeJar("WhiteBox.jar", "sun.hotspot.WhiteBox");
        String appJar =
            ClassFileInstaller.writeJar("RedefineBasic.jar", sharedClasses);
        String useWb = "-Xbootclasspath/a:" + wbJar;

        OutputAnalyzer output;
        TestCommon.testDump(appJar, sharedClasses, useWb);

        // redefineagent.jar is created by executing "@run main RedefineClassHelper"
        // which should be called before executing RedefineBasicTest
        output = TestCommon.exec(appJar, useWb,
                                 "-XX:+UnlockDiagnosticVMOptions",
                                 "-XX:+WhiteBoxAPI",
                                 "-javaagent:redefineagent.jar",
                                 "RedefineBasic");
        TestCommon.checkExec(output);
    }
}
