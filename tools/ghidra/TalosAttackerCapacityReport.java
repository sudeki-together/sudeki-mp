// Traces the CAiUnit attacker-capacity configuration used by Talos and his
// clones. Read-only and exact-build gated.
// @category SudekiMP

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Data;
import ghidra.program.model.listing.DataIterator;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.ReferenceIterator;
import java.util.HashSet;
import java.util.Set;

public class TalosAttackerCapacityReport extends GhidraScript {
    private static final String EXPECTED_SHA256 =
        "8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94";

    private void decompile(
        DecompInterface decompiler,
        Function function,
        String role,
        Set<Address> seen
    ) {
        if (function == null || !seen.add(function.getEntryPoint())) {
            return;
        }
        println("\n===== " + role + " " + function.getEntryPoint() +
            " " + function.getName(true) + " =====");
        DecompileResults results = decompiler.decompileFunction(
            function, 120, monitor);
        println(results.decompileCompleted() ?
            results.getDecompiledFunction().getC() :
            results.getErrorMessage());
    }

    private void findString(
        DecompInterface decompiler,
        String needle,
        Set<Address> seen
    ) {
        DataIterator iterator = currentProgram.getListing().getDefinedData(true);
        int matches = 0;
        while (iterator.hasNext() && !monitor.isCancelled()) {
            Data data = iterator.next();
            String value;
            ReferenceIterator references;
            if (!data.hasStringValue()) {
                continue;
            }
            value = data.getDefaultValueRepresentation();
            if (value == null || !value.contains(needle)) {
                continue;
            }
            println("\nSTRING needle=" + needle + " address=" +
                data.getAddress() + " value=" + value);
            references = currentProgram.getReferenceManager()
                .getReferencesTo(data.getAddress());
            while (references.hasNext()) {
                Reference reference = references.next();
                Function owner = currentProgram.getFunctionManager()
                    .getFunctionContaining(reference.getFromAddress());
                println("ref=" + reference.getFromAddress() + " type=" +
                    reference.getReferenceType() + " owner=" +
                    (owner == null ? "<none>" : owner.getEntryPoint()));
                decompile(decompiler, owner, needle + " owner", seen);
            }
            ++matches;
        }
        println("STRING_MATCHES needle=" + needle + " count=" + matches);
    }

    private Address address(long value) {
        return currentProgram.getAddressFactory().getDefaultAddressSpace()
            .getAddress(value);
    }

    private void reportFunctionReferences(
        DecompInterface decompiler,
        long value,
        String role,
        Set<Address> seen
    ) throws Exception {
        Address target = address(value);
        ReferenceIterator references = currentProgram.getReferenceManager()
            .getReferencesTo(target);
        println("\n===== FUNCTION REFERENCES " + role + " " + target +
            " =====");
        while (references.hasNext()) {
            Reference reference = references.next();
            Function owner = currentProgram.getFunctionManager()
                .getFunctionContaining(reference.getFromAddress());
            println("ref=" + reference.getFromAddress() + " type=" +
                reference.getReferenceType() + " owner=" +
                (owner == null ? "<none>" : owner.getEntryPoint()));
            decompile(decompiler, owner, role + " caller", seen);
        }
    }

    private void reportFunctions(
        DecompInterface decompiler,
        long[] values,
        String role,
        Set<Address> seen
    ) {
        for (long value : values) {
            Function function = currentProgram.getFunctionManager()
                .getFunctionAt(address(value));
            decompile(decompiler, function, role, seen);
        }
    }

    @Override
    protected void run() throws Exception {
        if (!EXPECTED_SHA256.equalsIgnoreCase(
                currentProgram.getExecutableSHA256())) {
            throw new Exception("Unexpected executable SHA256");
        }
        DecompInterface decompiler = new DecompInterface();
        Set<Address> seen = new HashSet<Address>();
        decompiler.openProgram(currentProgram);
        findString(decompiler, "Max attackers on target", seen);
        findString(decompiler, "Max ranged attackers on target", seen);
        findString(decompiler, "AI Unit Type", seen);
        findString(decompiler, "Boss monster AiUnits", seen);
        reportFunctionReferences(decompiler, 0x005a8ce0L,
            "target-state layout +24/+28", seen);
        reportFunctionReferences(decompiler, 0x005aee80L,
            "attack-state layout +24/+28", seen);
        reportFunctionReferences(decompiler, 0x005ae6a0L,
            "ranged-state layout +28/+2c", seen);
        reportFunctionReferences(decompiler, 0x006dc674L,
            "AiState_PC_MeleeBase vtable", seen);
        reportFunctionReferences(decompiler, 0x006dc604L,
            "AiState_PC_MissileBase vtable", seen);
        reportFunctions(decompiler, new long[] {
            0x005a98e0L, 0x005ae940L, 0x005ae9f0L, 0x005acc20L,
            0x005aead0L, 0x0043b860L, 0x005aea10L, 0x0043a390L,
            0x005a88f0L, 0x005ae070L, 0x005ae820L,
            0x005ae280L, 0x005ae310L, 0x005ae3f0L, 0x005ae0c0L,
            0x005adf60L, 0x005ae000L
        }, "PC melee/missile vtable method", seen);
        decompiler.dispose();
    }
}
