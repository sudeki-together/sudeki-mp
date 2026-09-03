// Reports the exact Tal melee-combo dispatch below CCharacterArbiter input.
// The script is read-only and refuses to inspect an unexpected executable.
// @category SudekiMP

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.InstructionIterator;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.ReferenceIterator;

import java.util.HashSet;
import java.util.Set;

public class TalMeleeComboReport extends GhidraScript {
    private static final String EXPECTED_SHA256 =
        "8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94";

    private Address address(long value) {
        return currentProgram.getAddressFactory().getDefaultAddressSpace()
            .getAddress(value);
    }

    private Function functionAtOrContaining(long value) {
        Address target = address(value);
        Function function = currentProgram.getFunctionManager().getFunctionAt(target);
        return function != null ? function :
            currentProgram.getFunctionManager().getFunctionContaining(target);
    }

    private void decompile(
            DecompInterface decompiler,
            Function function,
            String role,
            Set<Address> seen) {
        if (function == null || !seen.add(function.getEntryPoint())) return;
        DecompileResults result = decompiler.decompileFunction(function, 90, monitor);
        println("");
        println("===== " + role + " function=" + function.getEntryPoint() +
            " " + function.getName(true) + " =====");
        if (!result.decompileCompleted()) {
            println("Decompiler failed: " + result.getErrorMessage());
            return;
        }
        println(result.getDecompiledFunction().getC());
    }

    private void decompileAt(
            DecompInterface decompiler,
            long value,
            String role,
            Set<Address> seen) {
        decompile(decompiler, functionAtOrContaining(value),
            role + " target=" + address(value), seen);
    }

    private void decompileDirectCalls(
            DecompInterface decompiler,
            long value,
            String role,
            Set<Address> seen) {
        Function function = functionAtOrContaining(value);
        if (function == null) return;
        println("");
        println("DIRECT CALLS role=" + role + " function=" +
            function.getEntryPoint());
        InstructionIterator instructions = currentProgram.getListing()
            .getInstructions(function.getBody(), true);
        while (instructions.hasNext() && !monitor.isCancelled()) {
            Instruction instruction = instructions.next();
            for (Reference reference : instruction.getReferencesFrom()) {
                if (!reference.getReferenceType().isCall()) continue;
                Function callee = currentProgram.getFunctionManager()
                    .getFunctionAt(reference.getToAddress());
                println("  from=" + instruction.getAddress() + " to=" +
                    reference.getToAddress() + " callee=" +
                    (callee == null ? "<none>" : callee.getName(true)));
                decompile(decompiler, callee,
                    "direct callee of " + role, seen);
            }
        }
    }

    private void reportReferences(
            DecompInterface decompiler,
            long value,
            String role,
            Set<Address> seen) {
        Address target = address(value);
        ReferenceIterator references = currentProgram.getReferenceManager()
            .getReferencesTo(target);
        println("");
        println("REFERENCES role=" + role + " target=" + target);
        while (references.hasNext() && !monitor.isCancelled()) {
            Reference reference = references.next();
            Function caller = currentProgram.getFunctionManager()
                .getFunctionContaining(reference.getFromAddress());
            println("  from=" + reference.getFromAddress() + " type=" +
                reference.getReferenceType() + " caller=" +
                (caller == null ? "<none>" : caller.getName(true)));
            decompile(decompiler, caller, "reference owner for " + role, seen);
        }
    }

    @Override
    protected void run() throws Exception {
        String actualSha256 = currentProgram.getExecutableSHA256();
        if (!EXPECTED_SHA256.equalsIgnoreCase(actualSha256)) {
            throw new Exception("Unexpected executable SHA256: " + actualSha256);
        }
        println("SudekiMP Tal melee-combo report");
        println("SHA256=" + actualSha256);
        println("ImageBase=" + currentProgram.getImageBase());

        DecompInterface decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);
        Set<Address> seen = new HashSet<Address>();

        decompileAt(decompiler, 0x004dac00L,
            "melee attack-kind admission", seen);
        decompileAt(decompiler, 0x004d0730L,
            "melee combo dispatcher", seen);
        decompileDirectCalls(decompiler, 0x004d0730L,
            "melee combo dispatcher", seen);
        decompileAt(decompiler, 0x004d04f0L,
            "combo transition lookup", seen);
        decompileDirectCalls(decompiler, 0x004d04f0L,
            "combo transition lookup", seen);
        decompileAt(decompiler, 0x004d14d0L,
            "accepted combo transition commit", seen);
        decompileDirectCalls(decompiler, 0x004d14d0L,
            "accepted combo transition commit", seen);

        reportReferences(decompiler, 0x006cd568L,
            "AttackWeak/AttackStrong/AttackSweep property string", seen);
        reportReferences(decompiler, 0x006cd658L,
            "AttackStrong string", seen);
        reportReferences(decompiler, 0x006cd668L,
            "AttackWeak string", seen);
        reportReferences(decompiler, 0x006c3b1cL,
            "attack property string pointer", seen);
        reportReferences(decompiler, 0x006c3aa8L,
            "AttackWeak string pointer", seen);
        reportReferences(decompiler, 0x006c3ab0L,
            "AttackStrong string pointer", seen);

        decompiler.dispose();
    }
}
