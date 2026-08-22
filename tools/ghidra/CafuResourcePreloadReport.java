// Exact-build, read-only report for the missing Cafu W033 preload path.
// @category SudekiMP

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.ReferenceIterator;

import java.util.HashSet;
import java.util.Set;

public class CafuResourcePreloadReport extends GhidraScript {
    private static final String EXPECTED_SHA256 =
        "8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94";

    private Address address(long value) {
        return currentProgram.getAddressFactory().getDefaultAddressSpace()
            .getAddress(value);
    }

    private Function function(long value) {
        Function exact = currentProgram.getFunctionManager()
            .getFunctionAt(address(value));
        return exact != null ? exact : currentProgram.getFunctionManager()
            .getFunctionContaining(address(value));
    }

    private void decompile(DecompInterface decompiler, long value,
            String role, Set<Address> seen) {
        Function target = function(value);
        println("");
        println("===== " + role + " target=" + address(value) +
            " function=" + (target == null ? "<none>" :
                target.getEntryPoint() + " " + target.getName(true)) +
            " =====");
        if (target == null || !seen.add(target.getEntryPoint())) {
            return;
        }
        DecompileResults result = decompiler.decompileFunction(
            target, 120, monitor);
        println(result.decompileCompleted() ?
            result.getDecompiledFunction().getC() :
            "Decompiler failed: " + result.getErrorMessage());
    }

    private void listCallers(long value, String role) {
        Function target = function(value);
        if (target == null) {
            return;
        }
        println("");
        println("CALLERS role=" + role + " function=" +
            target.getEntryPoint());
        ReferenceIterator references = currentProgram.getReferenceManager()
            .getReferencesTo(target.getEntryPoint());
        while (references.hasNext() && !monitor.isCancelled()) {
            Reference reference = references.next();
            if (!reference.getReferenceType().isCall()) {
                continue;
            }
            Function caller = currentProgram.getFunctionManager()
                .getFunctionContaining(reference.getFromAddress());
            println("  callsite=" + reference.getFromAddress() +
                " caller=" + (caller == null ? "<none>" :
                    caller.getEntryPoint() + " " + caller.getName(true)));
        }
    }

    @Override
    protected void run() throws Exception {
        String[] arguments = getScriptArgs();
        boolean homOnly = arguments.length > 0 &&
            "hom-only".equalsIgnoreCase(arguments[0]);
        boolean internalsOnly = arguments.length > 0 &&
            "internals-only".equalsIgnoreCase(arguments[0]);
        boolean proxyOnly = arguments.length > 0 &&
            "proxy-only".equalsIgnoreCase(arguments[0]);
        boolean loaderOnly = arguments.length > 0 &&
            "loader-only".equalsIgnoreCase(arguments[0]);
        int firstHomSlot = arguments.length > 1 ?
            Integer.decode(arguments[1]) : 0;
        int homSlotCount = arguments.length > 2 ?
            Integer.decode(arguments[2]) : 16;
        String actualSha256 = currentProgram.getExecutableSHA256();
        if (!EXPECTED_SHA256.equalsIgnoreCase(actualSha256)) {
            throw new Exception("Unexpected executable SHA256: " + actualSha256);
        }
        println("SudekiMP Cafu resource preload report");
        println("SHA256=" + actualSha256);
        println("ImageBase=" + currentProgram.getImageBase());

        long[] targets = {
            0x00404fb0L,
            0x004b1b00L,
            0x00501d60L,
            0x0041c0d0L,
            0x0041c1b0L,
            0x0041c1c0L,
            0x0043e490L,
            0x0043e540L,
            0x0043e840L,
            0x00523500L
        };
        String[] roles = {
            "CGroupPlayers AllPendingCharactersLoaded",
            "InternalSpawnPC at position",
            "SpawnLoadedPlayers pending-character completion",
            "CSceneManager PVSPreload",
            "CGELPreloadHandle Release",
            "CGELPreloadHandle IsPreloadFinished",
            "typed resource manager lookup",
            "resource manager queue/update candidate",
            "typed resource release",
            "unsafe weapon model constructor"
        };
        DecompInterface decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);
        Set<Address> seen = new HashSet<Address>();
        if (!homOnly && !internalsOnly && !proxyOnly && !loaderOnly) {
            for (int index = 0; index < targets.length; ++index) {
                decompile(decompiler, targets[index], roles[index], seen);
                listCallers(targets[index], roles[index]);
            }
        }
        long[] homManagerMethods = {
            0x00457620L, 0x00451410L, 0x00451600L, 0x00451800L,
            0x004518e0L, 0x00444f40L, 0x004519e0L, 0x00451a30L,
            0x00451850L, 0x00447390L, 0x00451880L, 0x00451a50L,
            0x00449820L, 0x0044ced0L, 0x00451310L, 0x00451440L
        };
        if (loaderOnly) {
            long[] loaderTargets = {
                0x00472050L,
                0x00540730L,
                0x005407c0L,
                0x005408c0L,
                0x00540920L,
                0x00540e60L
            };
            String[] loaderRoles = {
                "HOM resource queue submission",
                "HomResource constructor",
                "HomResource destructor body",
                "HomResource synchronous setup candidate",
                "HomResource load completion candidate",
                "HomResource deleting destructor thunk"
            };
            for (int index = 0; index < loaderTargets.length; ++index) {
                decompile(decompiler, loaderTargets[index],
                    loaderRoles[index], seen);
                listCallers(loaderTargets[index], loaderRoles[index]);
            }
            decompiler.dispose();
            return;
        }
        if (proxyOnly) {
            long[] proxyTargets = {
                0x0043b860L,
                0x00451310L,
                0x005407a0L,
                0x005406f0L,
                0x005406c0L,
                0x005406d0L,
                0x00452c50L,
                0x00469a40L,
                0x00469e70L
            };
            String[] proxyRoles = {
                "HomResource destructor",
                "HomResource false predicate",
                "HomResource payload getter",
                "HomResource resource-name getter",
                "HomResource type getter",
                "HomResource state getter",
                "HomResource release callback",
                "resource-holder callback construction",
                "resource-holder direct payload extraction"
            };
            for (int index = 0; index < proxyTargets.length; ++index) {
                decompile(decompiler, proxyTargets[index],
                    proxyRoles[index], seen);
                listCallers(proxyTargets[index], proxyRoles[index]);
            }
            decompiler.dispose();
            return;
        }
        if (internalsOnly) {
            long[] internalTargets = {
                0x00411730L,
                0x00451600L,
                0x0045df00L,
                0x0045f0d0L,
                0x0045eae0L,
                0x0045ed00L
            };
            String[] internalRoles = {
                "generic typed-resource request wrapper",
                "HOM manager creating lookup",
                "resource collection find",
                "resource collection create",
                "resource holder populate",
                "resource request/update"
            };
            for (int index = 0; index < internalTargets.length; ++index) {
                decompile(decompiler, internalTargets[index],
                    internalRoles[index], seen);
                listCallers(internalTargets[index], internalRoles[index]);
            }
            decompiler.dispose();
            return;
        }
        int lastHomSlot = Math.min(
            homManagerMethods.length,
            Math.max(0, firstHomSlot) + Math.max(0, homSlotCount)
        );
        for (int slot = Math.max(0, firstHomSlot);
                slot < lastHomSlot; ++slot) {
            String role = String.format(
                "runtime-confirmed HOM manager vtable slot +0x%02x",
                slot * 4
            );
            decompile(decompiler, homManagerMethods[slot], role, seen);
            listCallers(homManagerMethods[slot], role);
        }
        decompiler.dispose();
    }
}
