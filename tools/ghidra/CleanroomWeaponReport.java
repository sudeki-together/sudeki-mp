// Traces the exact-build CCharacterWeapon::SetWeapon argument semantics.
// Read-only: refuses to inspect any executable other than the supported GOG build.
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

public class CleanroomWeaponReport extends GhidraScript {
    private static final String EXPECTED_SHA256 =
        "8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94";

    private Address address(long value) {
        return currentProgram.getAddressFactory().getDefaultAddressSpace()
            .getAddress(value);
    }

    private void report(
            DecompInterface decompiler,
            long value,
            String role,
            Set<Address> seen) {
        Function function = currentProgram.getFunctionManager()
            .getFunctionAt(address(value));
        if (function == null || !seen.add(function.getEntryPoint())) {
            return;
        }
        DecompileResults result = decompiler.decompileFunction(
            function, 90, monitor);
        println("");
        println("===== " + role + " " + function.getEntryPoint() +
            " " + function.getName(true) + " =====");
        println(result.decompileCompleted() ?
            result.getDecompiledFunction().getC() : "Decompiler failed");
        ReferenceIterator references = currentProgram.getReferenceManager()
            .getReferencesTo(function.getEntryPoint());
        while (references.hasNext() && !monitor.isCancelled()) {
            Reference reference = references.next();
            if (!reference.getReferenceType().isCall()) {
                continue;
            }
            Function caller = currentProgram.getFunctionManager()
                .getFunctionContaining(reference.getFromAddress());
            println("CALLER callsite=" + reference.getFromAddress() +
                " function=" + (caller == null ? "<none>" :
                    caller.getEntryPoint() + " " + caller.getName(true)));
            if (caller != null && seen.add(caller.getEntryPoint())) {
                DecompileResults callerResult = decompiler.decompileFunction(
                    caller, 90, monitor);
                println(callerResult.decompileCompleted() ?
                    callerResult.getDecompiledFunction().getC() :
                    "Caller decompiler failed");
            }
        }
    }

    @Override
    protected void run() throws Exception {
        String actualSha256 = currentProgram.getExecutableSHA256();
        if (!EXPECTED_SHA256.equalsIgnoreCase(actualSha256)) {
            throw new Exception("Unexpected executable SHA256: " + actualSha256);
        }
        println("SudekiMP cleanroom weapon report");
        println("SHA256=" + actualSha256);
        DecompInterface decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);
        Set<Address> seen = new HashSet<Address>();
        report(decompiler, 0x004d8790L,
            "CCharacterWeapon::SetWeapon", seen);
        report(decompiler, 0x00421ce0L,
            "native item lookup used by SetWeapon", seen);
        report(decompiler, 0x004204d0L,
            "FillInventory", seen);
        report(decompiler, 0x004d7e30L,
            "CCharacterWeapon::SetWeaponVisible", seen);
        decompiler.dispose();
    }
}
