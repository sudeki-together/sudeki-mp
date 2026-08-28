// Maps exact engine-owned usable/trigger discovery APIs for a future
// actor-local P2 interaction resolver. Read-only: it does not alter the
// executable, project, or runtime state.
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

public class InteractionResolverReport extends GhidraScript {
    private static final String EXPECTED_SHA256 =
        "8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94";

    private void decompile(DecompInterface decompiler, Function function,
            String role, Set<Address> seen) {
        if (function == null || !seen.add(function.getEntryPoint())) {
            return;
        }
        DecompileResults result = decompiler.decompileFunction(
            function, 120, monitor);
        println("\n===== " + role + " function=" +
            function.getEntryPoint() + " " + function.getName(true) +
            " =====");
        println(result.decompileCompleted() ?
            result.getDecompiledFunction().getC() :
            "Decompiler failed: " + result.getErrorMessage());
    }

    private void reportFunctionsMatching(DecompInterface decompiler,
            String needle, Set<Address> seen) {
        for (Function function : currentProgram.getFunctionManager()
                .getFunctions(true)) {
            if (monitor.isCancelled()) {
                return;
            }
            if (function.getName(true).indexOf(needle) >= 0) {
                decompile(decompiler, function,
                    "export/symbol containing " + needle, seen);
            }
        }
    }

    private void reportCallers(DecompInterface decompiler, Function function,
            Set<Address> seen) {
        ReferenceIterator refs = currentProgram.getReferenceManager()
            .getReferencesTo(function.getEntryPoint());
        println("\nREFERENCES target=" + function.getEntryPoint() + " " +
            function.getName(true));
        while (refs.hasNext() && !monitor.isCancelled()) {
            Reference ref = refs.next();
            Function caller = currentProgram.getFunctionManager()
                .getFunctionContaining(ref.getFromAddress());
            println("REFERENCE from=" + ref.getFromAddress() + " type=" +
                ref.getReferenceType() + " caller=" +
                (caller == null ? "<none>" : caller.getEntryPoint() +
                    " " + caller.getName(true)));
            if (ref.getReferenceType().isCall()) {
                decompile(decompiler, caller, "caller", seen);
            }
        }
    }

    private void reportGlobalUsers(DecompInterface decompiler, long value,
            Set<Address> seen) {
        Address target = currentProgram.getAddressFactory()
            .getDefaultAddressSpace().getAddress(value);
        ReferenceIterator refs = currentProgram.getReferenceManager()
            .getReferencesTo(target);
        println("\nGLOBAL REFERENCES target=" + target);
        while (refs.hasNext() && !monitor.isCancelled()) {
            Reference ref = refs.next();
            Function caller = currentProgram.getFunctionManager()
                .getFunctionContaining(ref.getFromAddress());
            println("REFERENCE from=" + ref.getFromAddress() + " type=" +
                ref.getReferenceType() + " caller=" +
                (caller == null ? "<none>" : caller.getEntryPoint() +
                    " " + caller.getName(true)));
            decompile(decompiler, caller, "CTriggerManager global user", seen);
        }
    }

    @Override
    protected void run() throws Exception {
        if (!EXPECTED_SHA256.equalsIgnoreCase(
                currentProgram.getExecutableSHA256())) {
            throw new Exception("Unexpected executable SHA256");
        }
        println("SudekiMP actor-local interaction resolver report");
        println("SHA256=" + currentProgram.getExecutableSHA256());
        println("ImageBase=" + currentProgram.getImageBase());

        DecompInterface decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);
        Set<Address> seen = new HashSet<Address>();
        String[] names = { "GetTriggerManager", "GetUsable", "GetWorldTrigger",
            "UsableGetCanUse", "UsableSetCanUse", "CTriggerManager" };
        for (String name : names) {
            reportFunctionsMatching(decompiler, name, seen);
        }
        /* Some overloaded exports share anonymous function symbols in the
         * imported project.  Their exact RVAs come from the PE export table. */
        long[] direct = { 0x00434c10L, 0x00434c20L, 0x00504400L, 0x00504440L,
            0x0040cf50L, 0x0040cfd0L, 0x0040d330L, 0x00523d30L,
            0x00524320L };
        String[] directRoles = { "GetCollisionSystem export",
            "CCollisionSystem nearby-entity query",
            "GetUsable/GetWorldTrigger char lookup",
            "GetUsable/GetWorldTrigger ResourceName lookup",
            "CTriggerManager frame reset", "CTriggerManager entry/exit scan",
            "CTriggerManager candidate construction", "trigger collision callback",
            "trigger collision eligibility" };
        for (int index = 0; index < direct.length; ++index) {
            Function function = currentProgram.getFunctionManager()
                .getFunctionContaining(currentProgram.getAddressFactory()
                    .getDefaultAddressSpace().getAddress(direct[index]));
            decompile(decompiler, function, directRoles[index], seen);
            if (function != null) {
                reportCallers(decompiler, function, seen);
            }
        }
        for (Function function : currentProgram.getFunctionManager()
                .getFunctions(true)) {
            for (String name : names) {
                if (function.getName(true).indexOf(name) >= 0) {
                    reportCallers(decompiler, function, seen);
                    break;
                }
            }
        }
        reportGlobalUsers(decompiler, 0x00808d24L, seen);
        reportGlobalUsers(decompiler, 0x00808dd4L, seen);
        decompiler.dispose();
    }
}
